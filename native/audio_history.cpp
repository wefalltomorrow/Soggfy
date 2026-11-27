#define WIN32_LEAN_AND_MEAN
#include "audio_history.h"
#include "ogg_history_core.h"
#include "ogg_tags.h"
#include "flac_history_core.h"
#include "capture_association.h"
#include "decoder_context.h"
#include "client_playback.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "hook_rollback.h"
#include "metadata_bridge.h"
#include "classic_ui_backend.h"
#include "post_process.h"
#include "playback_quality.h"
#include "playback_speed.h"
#include "playback_speed_policy.h"
#include "async_log.h"
#include "bounded_queue.h"
#include "retry_schedule.h"
#include <atomic>
#include "library_layout.h"
#include "existing_quality.h"
#include "file_publication.h"
#include "delete_on_close.h"
#include "media_session.h"
#include "listen_metadata.h"
#include "spotify_hook_discovery.h"
#include "vendor/minhook/include/MinHook.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {
using namespace history;
static std::atomic<bool> workers_running{false};
struct OggPage { unsigned char* header; int32_t header_length; unsigned char* body; int32_t body_length; };
static_assert(sizeof(OggPage)==32 && offsetof(OggPage,body)==16,"Windows x64 libogg ABI");
using PageSeek = int32_t(*)(void*,OggPage*);
PageSeek original;
void* target;
using Sniff = void*(*)(void*,const void*);
Sniff original_sniff;
void* sniff_target;
using FlacInit=void*(*)(void*,void*);
using FlacRead=int(*)(void*,unsigned char*,size_t*);
using FlacFrame=int(*)(void*,const void*,const void*);
using FlacError=void(*)(void*,unsigned);
FlacInit original_flac_init;
FlacRead original_flac_read;
FlacFrame original_flac_frame;
FlacError original_flac_error;
void* flac_targets[4]={};
hooks::CallbackCounter audio_callbacks;
struct Slot { uintptr_t context,owner; unsigned length; double time; unsigned kind,epoch; LONG loss; uint64_t input_length,boundary;
    uint32_t format; bool matched,capture; unsigned char bytes[65307]; };
constexpr unsigned capacity=128;
Slot* queue;
unsigned head=0,tail=0,count=0;
SRWLOCK queue_lock=SRWLOCK_INIT;
HANDLE event;
volatile LONG dropped=0,calls=0,pages=0;
volatile LONG ogg_hook_entries=0,ogg_hook_returns=0,ogg_hook_queued=0;
std::atomic<bool> quality_enabled{false};
std::wstring output;
size_t memory_limit=500*1024*1024;
struct CaptureEvent {unsigned epoch;LONG loss;uintptr_t owner;bool capture;uint64_t boundary;};
static CaptureEvent BeginEvent(bool flac) {
    const auto policy=ReadCapturePolicy();
    const auto scope=decoder_context;
    const auto current_boundary=client_playback.Token();
    const auto boundary=scope.active?scope.boundary:current_boundary;
    return {scope.active?scope.epoch:policy.epoch,InterlockedCompareExchange(&dropped,0,0),scope.owner,
        boundary==current_boundary&&!(boundary&1)&&(flac?policy.flac:policy.ogg)&&(!scope.active||scope.epoch==policy.epoch),boundary};
}
static bool EventCurrent(const CaptureEvent& e) {
    return e.epoch==CaptureEpoch()&&e.loss==InterlockedCompareExchange(&dropped,0,0)&&e.boundary==client_playback.Token();
}
static double Now() { return double(GetTickCount64())/1000.0; }
static uint32_t ReadLe32(const unsigned char* bytes) {
    return uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16)|(uint32_t(bytes[3])<<24);
}
static void TraceOggHookPage(LONG entry_order,LONG return_order,LONG queue_order,DWORD thread_id,
                             const DecoderContext& scope,const CaptureEvent& entry,uintptr_t context,
                             uint32_t serial,uint32_t sequence,uint8_t flags,int quality,
                             const char* state,int32_t result) {
    if(!DebugLoggingEnabled())return;
    char line[560];
    snprintf(line,sizeof(line),
        "ogg_hook entry=%ld return=%ld queue=%ld tid=%lu scope=%d owner=%llx scope_epoch=%u scope_boundary=%llu event_epoch=%u event_boundary=%llu ctx=%llx serial=%u seq=%u flags=0x%02x capture=%d quality=%d state=%s result=%d",
        entry_order,return_order,queue_order,static_cast<unsigned long>(thread_id),scope.active,
        static_cast<unsigned long long>(scope.owner),scope.epoch,
        static_cast<unsigned long long>(scope.boundary),entry.epoch,
        static_cast<unsigned long long>(entry.boundary),
        static_cast<unsigned long long>(context),serial,sequence,flags,entry.capture,quality,
        state,static_cast<int>(result));
    // This uses the independent, nonblocking diagnostic lane. It never reserves
    // space in or increments the loss counter for the audio capture queue.
    QueueDiagnostic(line);
}
static void Log(const char* message) {
    HistoryLog(message);
}
static bool Directories(const std::wstring& path) {
    return EnsureDirectory(path);
}
static spotify::DiscoveryResult DiscoverTargets(HMODULE module,spotify::HookTargets& targets) {
    auto* base=reinterpret_cast<unsigned char*>(module);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)
        return spotify::DiscoveryResult::InvalidImage;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64)
        return spotify::DiscoveryResult::InvalidImage;
    return spotify::DiscoverHookTargets(base,nt->OptionalHeader.SizeOfImage,targets);
}

static const char* DiscoveryFailure(spotify::DiscoveryResult result) {
    switch(result) {
        case spotify::DiscoveryResult::InvalidImage:return "invalid PE image";
        case spotify::DiscoveryResult::MissingTarget:return "required instruction anchor missing";
        case spotify::DiscoveryResult::AmbiguousTarget:return "instruction anchor is ambiguous";
        case spotify::DiscoveryResult::InconsistentLayout:return "target layout failed structural checks";
        case spotify::DiscoveryResult::Found:return "none";
    }
    return "unknown discovery failure";
}

static void* DiscoveredTarget(HMODULE module,const spotify::HookTargets& targets,
                              spotify::TargetKind kind) {
    const auto rva=spotify::TargetRva(targets,kind);
    return rva?reinterpret_cast<unsigned char*>(module)+rva:nullptr;
}
static int32_t Hook(void* sync,OggPage* page) {
    auto callback=audio_callbacks.Enter();
    const LONG entry_order=InterlockedIncrement(&ogg_hook_entries);
    const auto scope=decoder_context;
    const DWORD thread_id=GetCurrentThreadId();
    const auto entry=BeginEvent(false);
    int32_t result=original(sync,page);
    const LONG return_order=InterlockedIncrement(&ogg_hook_returns);
    InterlockedIncrement(&calls);
    const bool capture=entry.capture;
    const int quality=quality_enabled.load(std::memory_order_relaxed);
    const bool valid=result>0 && page && page->header && page->header_length>=27 && page->header_length<=282 &&
       (page->body_length==0 || page->body) &&
       page->body_length>=0 && page->body_length<=65025 &&
       page->header_length+page->body_length==result && result<=65307;
    const bool has_ogg_header=valid && std::memcmp(page->header,"OggS",4)==0;
    const uint32_t serial=has_ogg_header?ReadLe32(page->header+14):0xffffffffu;
    const uint32_t sequence=has_ogg_header?ReadLe32(page->header+18):0xffffffffu;
    const uint8_t flags=valid?page->header[5]:0xff;
    if(!valid) {
        if(capture||quality)TraceOggHookPage(entry_order,return_order,0,thread_id,scope,entry,
            reinterpret_cast<uintptr_t>(sync),serial,sequence,flags,quality,"invalid_page",result);
        return result;
    }
    if(!capture && !quality) {
        TraceOggHookPage(entry_order,return_order,0,thread_id,scope,entry,
            reinterpret_cast<uintptr_t>(sync),serial,sequence,flags,quality,"not_queued",result);
        return result;
    }
    InterlockedIncrement(&pages);
    LONG queue_order=0;
    const char* queue_state="stale";
    AcquireSRWLockExclusive(&queue_lock);
    if(!EventCurrent(entry))queue_state="stale";
    else if(count==capacity) {InterlockedIncrement(&dropped);queue_state="queue_full";}
    else {
        Slot& s=queue[tail]; s.context=reinterpret_cast<uintptr_t>(sync);
        s.length=unsigned(result); s.time=Now(); s.kind=0; s.epoch=entry.epoch; s.capture=capture;s.owner=entry.owner;s.loss=entry.loss;s.boundary=entry.boundary;
        std::memcpy(s.bytes,page->header,page->header_length);
        if(page->body_length) std::memcpy(s.bytes+page->header_length,page->body,page->body_length);
        queue_order=InterlockedIncrement(&ogg_hook_queued);
        tail=(tail+1)%capacity; ++count;queue_state="queued";
    }
    ReleaseSRWLockExclusive(&queue_lock); SetEvent(event);
    TraceOggHookPage(entry_order,return_order,queue_order,thread_id,scope,entry,
        reinterpret_cast<uintptr_t>(sync),serial,sequence,flags,quality,queue_state,result);
    return result;
}
static void* FormatHook(void* out,const void* input) {
    auto callback=audio_callbacks.Enter();
    // Verified ABI: input is {const byte*, size_t}; result is {uint32_t kind, bool valid}.
    // Read only a short diagnostic prefix. The original detector chooses the format.
    void* result=original_sniff(out,input);
    const auto* view=static_cast<const uintptr_t*>(input);
    unsigned n=unsigned(std::min<uintptr_t>(view[1],16));
    AcquireSRWLockExclusive(&queue_lock);
    if(count==capacity) InterlockedIncrement(&dropped);
    else {
        Slot& s=queue[tail]; s.kind=1; s.length=n; s.input_length=view[1]; s.time=Now();
        std::memcpy(&s.format,out,4); s.matched=static_cast<unsigned char*>(out)[4]!=0;
        if(n) std::memcpy(s.bytes,reinterpret_cast<const void*>(view[0]),n);
        tail=(tail+1)%capacity; ++count;
    }
    ReleaseSRWLockExclusive(&queue_lock); SetEvent(event); return result;
}
static bool Pop(Slot& s) {
    AcquireSRWLockExclusive(&queue_lock);
    bool ready=count!=0;
    if(ready) { s=queue[head]; head=(head+1)%capacity; --count; }
    ReleaseSRWLockExclusive(&queue_lock); return ready;
}
static void FlacEvent(void* context,const CaptureEvent& entry,unsigned kind,const void* bytes=nullptr,size_t n=0,unsigned status=0) {
    const bool capture=entry.capture;
    if(!capture && !quality_enabled.load(std::memory_order_relaxed)) return;
    AcquireSRWLockExclusive(&queue_lock);
    if(!EventCurrent(entry)){ReleaseSRWLockExclusive(&queue_lock);return;}
    do {
        unsigned amount=unsigned(std::min(n,size_t(65307)));
        if(count==capacity) { InterlockedIncrement(&dropped); break; }
        Slot& s=queue[tail]; s.context=reinterpret_cast<uintptr_t>(context); s.kind=kind;
        s.capture=capture; s.input_length=amount;
        s.length=(!capture && kind==3) ? std::min(amount,42u) : amount;
        s.time=Now(); s.format=status; s.epoch=entry.epoch;s.owner=entry.owner;s.loss=entry.loss;s.boundary=entry.boundary;
        if(s.length) memcpy(s.bytes,bytes,s.length);
        tail=(tail+1)%capacity; ++count;
        n-=amount;
        if(n) bytes=static_cast<const unsigned char*>(bytes)+amount;
    } while(n);
    ReleaseSRWLockExclusive(&queue_lock); SetEvent(event);
}
static void* FlacInitHook(void* context,void* result) {
    auto callback=audio_callbacks.Enter();
    const auto entry=BeginEvent(true);
    FlacEvent(context,entry,2);
    return original_flac_init(context,result);
}
static int FlacReadHook(void* context,unsigned char* buffer,size_t* length) {
    auto callback=audio_callbacks.Enter();
    const auto entry=BeginEvent(true);
    size_t requested=*length;
    int status=original_flac_read(context,buffer,length);
    if(status==0 && *length && *length<=requested) FlacEvent(context,entry,3,buffer,*length);
    else if(status==1 && !*length) FlacEvent(context,entry,6);
    else if(status!=0 || *length>requested) FlacEvent(context,entry,5,nullptr,0,unsigned(status));
    return status;
}
static int FlacFrameHook(void* context,const void* frame,const void* pcm) {
    auto callback=audio_callbacks.Enter();
    const auto entry=BeginEvent(true);
    int status=original_flac_frame(context,frame,pcm);
    if(status==0) FlacEvent(context,entry,4,frame,32);
    else FlacEvent(context,entry,5,nullptr,0,unsigned(status));
    return status;
}
static void FlacErrorHook(void* context,unsigned error) {
    auto callback=audio_callbacks.Enter();
    const auto entry=BeginEvent(true);
    original_flac_error(context,error);
    FlacEvent(context,entry,5,nullptr,0,error);
}
struct Capture {
    CaptureOwner owner;
    Stream stream;
    uint64_t dump_stream_id=0;
    uint32_t epoch=0;
    bool lossless=false,metadata=false;
    FlacInfo flac;
    FlacCoverage coverage;
    double born=0,finished=0;
    CaptureRetention retention;
    uint64_t bytes=0;
    CompressedBuffer data;
    double Duration() const { return lossless && flac.rate ? double(coverage.samples)/flac.rate : stream.Duration(); }
    Quality Encoding() const { return lossless ? Quality{Codec::Flac,flac.rate,flac.channels,flac.bits,0} : Quality{Codec::Vorbis,stream.rate,stream.channels,0,stream.bitrate}; }
};
struct Heard {
    Media media; double start=0,finish=0; uint64_t generation=0;
    std::string identity;bool exact_identity=false;
    size_t retention_conflicts=0;
};
static Catalog CatalogFor(const Media& media) {
    Catalog c; c.title=media.title; c.album=media.album; c.track=media.track;
    c.album_artist=media.album_artist;
    c.all_artists=media.artist;
    c.artist=media.album_artist.empty() ? media.artist : media.album_artist;
    EnrichCatalog(media,c);
    if(!c.album_artist.empty()) c.artist=c.album_artist;
    if(_wcsicmp(c.artist.c_str(),L"Various Artists")==0) c.kind=MediaKind::VariousArtists;
    return c;
}
enum class Publication { Saved,Skipped,Canceled,Busy,Failed };
struct PublicationResult {
    Publication state=Publication::Failed;
    std::wstring path;
    std::string message;
    bool retryable=false;
    std::wstring retry_path;
    std::string retry_identity;
    unsigned generation=0;
    PublicationResult()=default;
    PublicationResult(Publication result_state,std::wstring result_path,std::string result_message,
                      bool can_retry=false,std::wstring retry_destination={},
                      std::string retry_uri={},unsigned settings_generation=0)
        :state(result_state),path(std::move(result_path)),message(std::move(result_message)),
         retryable(can_retry),retry_path(std::move(retry_destination)),
         retry_identity(std::move(retry_uri)),generation(settings_generation) {}
};
static bool WriteSidecar(const std::wstring& path,const void* data,size_t size) {
    if(path.empty()||!data||!size||size>MAXDWORD)return false;
    auto parent=path.substr(0,path.find_last_of(L'\\'));
    if(!Directories(parent))return false;
    HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_WRITE|DELETE,FILE_SHARE_READ,nullptr,
                            CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_EXISTS;
    DWORD written=0;bool ok=WriteFile(file,data,DWORD(size),&written,nullptr)&&written==size;
    if(!ok)MarkOpenFileForDeletion(file);
    CloseHandle(file);return ok;
}
static PublicationResult Publish(Capture& c,const Heard& heard,unsigned expected_epoch,
                                 bool enforce_generation=false,unsigned expected_generation=0) {
    SYSTEMTIME t; GetSystemTime(&t); wchar_t stamp[64];
    swprintf(stamp,64,L"%04u-%02u-%02uT%02u:%02u:%02uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    auto catalog=CatalogFor(heard.media);
    const std::string expected_identity=
        ValidSpotifyUri(catalog.spotify_uri,"track")||ValidSpotifyUri(catalog.spotify_uri,"episode")
            ?catalog.spotify_uri:std::string{};
    auto settings=GetSettings();
    if(settings.capture_epoch!=expected_epoch || (enforce_generation&&settings.generation!=expected_generation)||
       !settings.downloads || !(c.lossless ? settings.flac : settings.ogg))
        return {Publication::Canceled,{},"settings changed"};
    const std::wstring& active_template=(catalog.kind==MediaKind::Podcast && !settings.podcast_template.empty())
        ?settings.podcast_template:settings.path_template;
    std::wstring destination=OutputPath(settings.root,catalog,c.lossless ? L".flac" : L".ogg",
        settings.music_folder,active_template,settings.normalize_artist_separators,settings.invalid_char_repl);
    destination=ResolveRecordingPath(settings.root,destination,catalog.spotify_uri,settings.output_ext);
    if(destination.empty())return {Publication::Failed,{},"Recording identity conflict; existing files retained"};
    std::wstring flac=destination.substr(0,destination.find_last_of(L'.'))+L".flac";
    {
        char line[1400];snprintf(line,sizeof(line),
            "destination=%s native=%s preset=%s output_ext=%s keep_native=%d embed_cover=%d embed_lyrics=%d save_cover=%d save_lyrics=%d",
            Utf8(destination).c_str(),c.lossless?"flac":"ogg",Utf8(settings.output_preset).c_str(),
            Utf8(settings.output_ext).c_str(),settings.keep_native_original,settings.embed_cover_art,
            settings.embed_lyrics,settings.save_cover_art,settings.save_lyrics);
        LogActivity("publish_path",line);QueueDiagnostic(line);
    }
    Quality quality=c.Encoding(),lossless;
    const auto retry_existing=[&](const std::wstring& path)->PublicationResult {
        PostProcessControl control;
        control.canceled=[generation=settings.generation,epoch=settings.capture_epoch] {
            return !workers_running.load(std::memory_order_acquire)||CaptureEpoch()!=epoch||GetSettings().generation!=generation;
        };
        if(control.canceled())return {Publication::Canceled,path,"settings changed"};
        const auto post=RetryPostProcessExistingFile(path,settings,control,expected_identity);
        if(post.state==PostProcessResult::State::Failed) {
            LogActivity("conversion_retry_failed",post.error);
            return {Publication::Failed,path,post.error,post.retryable,
                post.retryable?path:L"",expected_identity,settings.generation};
        }
        if(post.state==PostProcessResult::State::Converted) {
            LogActivity("conversion_retry_done",Utf8(post.path));
            return {Publication::Saved,post.path,{}};
        }
        return {Publication::Skipped,post.path.empty()?path:post.path,"Already downloaded"};
    };
    if(!c.lossless && ReadQuality(flac,lossless) && lossless.codec==Codec::Flac) {
        Log(("KEEP existing lossless file "+Utf8(flac)).c_str()); return retry_existing(flac);
    }
    SaveDecision decision=DecideSave(destination,quality);
    if(decision==SaveDecision::Skip) {
        Log(("KEEP existing equal, higher or unverified quality "+Utf8(destination)).c_str()); return retry_existing(destination);
    }
    Tags tags;
    tags.fields={{"TITLE",Utf8(heard.media.title)},{"ARTIST",Utf8(heard.media.artist)},
      {"ALBUM",Utf8(heard.media.album)},{"ALBUMARTIST",Utf8(heard.media.album_artist)},
      {"TRACKNUMBER",std::to_string(heard.media.track)},
      {"HISTORY_COMPLETE_LISTEN","1"},{"HISTORY_TRANSCODED","0"},
      {"HISTORY_SAMPLES",std::to_string(c.lossless ? c.coverage.samples : c.stream.samples)},
      {"HISTORY_SAMPLE_RATE",std::to_string(quality.rate)},
      {"HISTORY_BITS_PER_SAMPLE",std::to_string(quality.bits)},
      {"HISTORY_NOMINAL_BITRATE",std::to_string(quality.bitrate)},
      {"HISTORY_MEDIA_DURATION",std::to_string(heard.media.duration)},
      {"HISTORY_SOURCE_PAGES",std::to_string(c.lossless ? c.coverage.frames : c.stream.pages)},
      {"HISTORY_SOURCE_BYTES",std::to_string(c.bytes)},
      {"HISTORY_ELAPSED_SECONDS",std::to_string(heard.finish-heard.start)},
      {"HISTORY_SAVED_UTC",Utf8(stamp)},{"HISTORY_CAPTURE",c.lossless ? "native compressed FLAC input" : "native compressed Ogg pages"}};
    if(settings.embed_cover_art) {
        tags.cover=heard.media.cover;
        tags.mime=heard.media.cover_extension==L".png" ? "image/png" : "image/jpeg";
    }
    if(!heard.media.genre.empty())tags.fields.push_back({"GENRE",Utf8(heard.media.genre)});
    EnrichTags(heard.media,tags);
    if(ValidSpotifyUri(catalog.spotify_uri,"track")||ValidSpotifyUri(catalog.spotify_uri,"episode")) {
        for(const auto& field:tags.fields)if(field.first=="SPOTIFY_URI"&&field.second!=catalog.spotify_uri)
            return {Publication::Failed,{},"Metadata identity changed before publication"};
        tags.fields.erase(std::remove_if(tags.fields.begin(),tags.fields.end(),[](const auto& field){
            return field.first=="SPOTIFY_URI"||field.first=="SPOTIFY_ALBUM_URI";
        }),tags.fields.end());
        tags.fields.push_back({"SPOTIFY_URI",catalog.spotify_uri});
        if(ValidSpotifyUri(catalog.spotify_album_uri,"album"))tags.fields.push_back({"SPOTIFY_ALBUM_URI",catalog.spotify_album_uri});
    }
    std::string lyrics;
    for(auto it=tags.fields.begin();it!=tags.fields.end();) {
        if(it->first=="LYRICS") {
            lyrics=it->second;
            if(!settings.embed_lyrics){it=tags.fields.erase(it);continue;}
        }
        ++it;
    }
    std::vector<uint8_t> tagged; std::string error;
    auto source=c.data.Flatten();
    bool tagged_ok=c.lossless ? TagFlac(source,tags,tagged,error) : TagOgg(source,tags,tagged,error);
    if(!tagged_ok) { Log(("native tagging failed: "+error).c_str()); return {Publication::Failed,{},error}; }
    auto latest=GetSettings();
    if(latest.generation!=settings.generation || !latest.downloads || !(c.lossless ? latest.flac : latest.ogg)) return {Publication::Canceled,{},"settings changed"};
    if(!Directories(destination.substr(0,destination.find_last_of(L'\\')))) return {Publication::Failed,{},"output directory could not be created"};
    // Publish the complete tagged native capture before optional sidecars and
    // conversion. Conflicts preserve the existing file and may retry conversion.
    auto publication=PublishBytes(destination,quality,tagged.data(),tagged.size());
    if(publication==FilePublication::Skipped) return retry_existing(destination);
    if(publication==FilePublication::Busy)
        return {Publication::Busy,destination,"destination is busy",false,{}, {},settings.generation};
    if(publication==FilePublication::Failed) {
        Log("final publication failed; existing file retained");
        return {Publication::Failed,{},"final publication failed"};
    }
    std::string line=(publication==FilePublication::Upgraded ? "UPGRADED " : "SAVED ")+Utf8(destination);
    Log(line.c_str());

    if(settings.save_cover_art && !heard.media.cover.empty()) {
        std::wstring sidecar=destination.substr(0,destination.find_last_of(L'\\')+1)+
            (heard.media.cover_extension==L".png"?L"cover.png":L"cover.jpg");
        if(!WriteSidecar(sidecar,heard.media.cover.data(),heard.media.cover.size()))
            Log("cover sidecar could not be written");
    }
    if(settings.save_lyrics && !lyrics.empty()) {
        bool synced=false;
        for(size_t i=0;i+6<lyrics.size();++i) {
            if(lyrics[i]=='[' && lyrics[i+1]>='0'&&lyrics[i+1]<='9' &&
               lyrics[i+2]>='0'&&lyrics[i+2]<='9' && lyrics[i+3]==':' &&
               lyrics[i+4]>='0'&&lyrics[i+4]<='9' && lyrics[i+5]>='0'&&lyrics[i+5]<='9') {
                synced=true;break;
            }
        }
        auto lyric_path=destination.substr(0,destination.find_last_of(L'.'))+(synced?L".lrc":L".txt");
        if(!WriteSidecar(lyric_path,lyrics.data(),lyrics.size()))
            Log("lyrics sidecar could not be written");
    }

    PostProcessControl post_control;
    post_control.canceled=[epoch=settings.capture_epoch,generation=settings.generation] {
        return !workers_running.load(std::memory_order_acquire)||CaptureEpoch()!=epoch||
            GetSettings().generation!=generation;
    };
    auto post=PostProcessPublishedFile(destination,settings,heard.media.cover,heard.media.cover_extension,
        post_control,expected_identity);
    if(post.state==PostProcessResult::State::Failed) {
        auto detail="post-processing failed: "+post.error+" native="+Utf8(destination)+" output="+Utf8(post.path);
        LogActivity("failed",detail);QueueDiagnostic(detail.c_str());
        return {Publication::Failed,post.path,post.error,post.retryable,
            post.retryable?destination:L"",expected_identity,settings.generation};
    }
    if(!post.error.empty()) {
        auto detail="post-processing warning: "+post.error+" native="+Utf8(destination);
        LogActivity("warning",detail);QueueDiagnostic(detail.c_str());
    }
    {
        auto final_path=post.path.empty()?destination:post.path;
        auto detail="publication complete native="+Utf8(destination)+" final="+Utf8(final_path);
        LogActivity("publish_done",detail);QueueDiagnostic(detail.c_str());
    }
    return {Publication::Saved,post.path.empty()?destination:post.path,{}};

}
struct SaveJob {
    std::unique_ptr<Capture> capture;Heard heard;unsigned epoch=0;size_t reserved=0;
    unsigned retry_generation=0,retry_attempts=0;
};
struct RetryJob {
    std::wstring path,title,artist,album;
    std::string identity;
    unsigned epoch=0,generation=0,attempts=0;
    uint64_t due=0;
};
static BoundedQueue<SaveJob,2> saves;
using RetryQueue=RetrySchedule<RetryJob,32,1024*1024>;
using PublicationRetryQueue=RetrySchedule<SaveJob,2,512ull*1024*1024>;
static HANDLE save_event=nullptr;
static std::atomic<size_t> publishing_reserved{0};
static std::atomic<unsigned> saved_count{0};
static hooks::InitController audio_init;
static size_t RetryMemory(const RetryJob& job) {
    size_t bytes=sizeof(job);
    const auto add=[&](size_t amount) {
        if(amount>1024*1024-bytes)return false;
        bytes+=amount;return true;
    };
    const auto wide=[&](size_t capacity) {
        return capacity<=(1024*1024-bytes)/sizeof(wchar_t)&&add(capacity*sizeof(wchar_t));
    };
    if(!wide(job.path.capacity())||!wide(job.title.capacity())||!wide(job.artist.capacity())||
       !wide(job.album.capacity())||!add(job.identity.capacity()))return 1024*1024+1;
    return bytes;
}
static Media RetryStatusMedia(const RetryJob& job) {
    Media media;media.title=job.title;media.artist=job.artist;media.album=job.album;return media;
}
static bool RetryPathEnabled(const Settings& settings,const std::wstring& path) {
    if(path.size()>=5&&_wcsicmp(path.c_str()+path.size()-5,L".flac")==0)return settings.flac;
    if(path.size()>=4&&_wcsicmp(path.c_str()+path.size()-4,L".ogg")==0)return settings.ogg;
    return false;
}
static bool QueueTransientRetry(RetryQueue& retries,const PublicationResult& result,
                                const SaveJob& source) {
    const auto settings=GetSettings();
    if(!result.retryable||result.retry_path.empty()||
       (!ValidSpotifyUri(result.retry_identity,"track")&&!ValidSpotifyUri(result.retry_identity,"episode"))||
       settings.generation!=result.generation||settings.capture_epoch!=source.epoch||
       !settings.downloads||!RetryPathEnabled(settings,result.retry_path)||
       !ConversionRequested(settings,result.retry_path))return false;
    RetryJob retry;retry.path=result.retry_path;retry.identity=result.retry_identity;
    retry.title=source.heard.media.title;retry.artist=source.heard.media.artist;
    retry.album=source.heard.media.album;retry.epoch=source.epoch;retry.generation=result.generation;
    retry.due=GetTickCount64()+transient_retry_delay_ms(0);
    const auto bytes=RetryMemory(retry);
    const auto due=retry.due;
    if(bytes>1024*1024||!retries.Push(std::move(retry),bytes,due))return false;
    return true;
}
static bool PublicationRetrySettingsCurrent(const SaveJob& job,const Settings& settings) {
    return job.retry_generation==settings.generation&&job.epoch==settings.capture_epoch&&
        settings.downloads&&(job.capture->lossless?settings.flac:settings.ogg);
}
static bool CanQueuePublicationRetry(const PublicationResult& result,const SaveJob& source,
                                     const Settings& settings) {
    return result.state==Publication::Busy&&result.generation==settings.generation&&
        source.epoch==settings.capture_epoch&&settings.downloads&&source.capture&&
        (source.capture->lossless?settings.flac:settings.ogg);
}
static bool RetrySettingsCurrent(const RetryJob& retry,const Settings& settings) {
    if(retry.generation!=settings.generation||retry.epoch!=settings.capture_epoch||!settings.downloads)return false;
    return RetryPathEnabled(settings,retry.path)&&ConversionRequested(settings,retry.path);
}
static DWORD WINAPI SaveWorker(LPVOID) {
    RetryQueue retries;
    PublicationRetryQueue publication_retries;
    while(workers_running.load(std::memory_order_acquire)) {
        auto settings=GetSettings();
        for(size_t i=0;i<publication_retries.Size();) {
            const auto& pending=publication_retries.At(i);
            if(PublicationRetrySettingsCurrent(pending,settings)){++i;continue;}
            const auto reserved=pending.reserved;
            try {
                SetClassicTrackStatus(pending.heard.media,"ERROR","Canceled: settings changed");
                LogActivity("publication_retry_canceled","settings changed before retry");
            } catch(...) {}
            publication_retries.Discard(i);publishing_reserved.fetch_sub(reserved);
        }
        for(size_t i=0;i<retries.Size();) {
            if(RetrySettingsCurrent(retries.At(i),settings)){++i;continue;}
            auto canceled=retries.At(i);
            SetClassicTrackStatus(RetryStatusMedia(canceled),"ERROR","Canceled: settings changed",canceled.path);
            LogActivity("conversion_retry_canceled",Utf8(canceled.path));
            retries.Discard(i);
        }
        RetryJob retry;
        size_t retry_bytes=0;
        if(retries.TakeDue(GetTickCount64(),retry,retry_bytes)) {
            const auto media=RetryStatusMedia(retry);
            bool retry_requeued=false;
            try {
                settings=GetSettings();
                if(!RetrySettingsCurrent(retry,settings)) {
                    SetClassicTrackStatus(media,"ERROR","Canceled: settings changed",retry.path);
                    LogActivity("conversion_retry_canceled",Utf8(retry.path));
                    retries.ReleaseInFlight(retry_bytes);continue;
                }
                const unsigned attempt=retry.attempts+1;
                SetClassicTrackStatus(media,"CONVERTING",
                    "Retrying conversion ("+std::to_string(attempt)+" of "+std::to_string(transient_retry_limit)+")",retry.path);
                PostProcessControl control;
                control.canceled=[epoch=retry.epoch,generation=retry.generation] {
                    const auto current=GetSettings();
                    return !workers_running.load(std::memory_order_acquire)||CaptureEpoch()!=epoch||
                        current.generation!=generation||!current.downloads;
                };
                const auto result=RetryPostProcessExistingFile(retry.path,settings,control,retry.identity);
                // ProcessFile verifies the locked source and any existing or
                // newly converted output before returning success.
                if(result.state==PostProcessResult::State::Converted||result.state==PostProcessResult::State::Skipped) {
                    if(result.state==PostProcessResult::State::Converted&&CaptureEpoch()==retry.epoch)++saved_count;
                    SetClassicTrackStatus(media,"DONE",{},result.path);
                    LogActivity("conversion_retry_done",Utf8(result.path));
                    retries.ReleaseInFlight(retry_bytes);continue;
                }
                const bool retry_again=result.state==PostProcessResult::State::Failed&&result.retryable&&
                    retry.attempts+1<transient_retry_limit&&RetrySettingsCurrent(retry,GetSettings());
                if(retry_again) {
                    ++retry.attempts;
                    retry.due=GetTickCount64()+transient_retry_delay_ms(retry.attempts);
                    if(retries.Requeue(std::move(retry),retry_bytes,retry.due)) {
                        retry_requeued=true;
                        const auto& queued=retries.At(retries.Size()-1);
                        SetClassicTrackStatus(media,"CONVERTING",
                            "File is busy; retry scheduled ("+std::to_string(attempt+1)+" of "+
                            std::to_string(transient_retry_limit)+")",queued.path);
                        LogActivity("conversion_retry_queued",Utf8(queued.path));
                        continue;
                    }
                }
                const std::string message=result.state==PostProcessResult::State::Failed?
                    (result.error.empty()?"Conversion retry failed":result.error):
                    "Existing output identity cannot be verified; native original retained";
                SetClassicTrackStatus(media,"ERROR",message,retry.path);
                LogActivity("conversion_retry_failed",message+" native="+Utf8(retry.path)+
                    " output="+Utf8(result.path));
            } catch(...) {
                if(retry_requeued) {
                    try {LogActivity("conversion_retry_queued","status update failed after retry was retained");} catch(...) {}
                    continue;
                }
                try {
                    SetClassicTrackStatus(media,"ERROR","Conversion retry exception; native original retained",retry.path);
                    LogActivity("conversion_retry_failed","exception path="+Utf8(retry.path));
                } catch(...) {}
            }
            retries.ReleaseInFlight(retry_bytes);
            continue;
        }
        SaveJob job;
        bool publication_retry_in_flight=false;
        size_t publication_retry_bytes=0;
        if(publication_retries.TakeDue(GetTickCount64(),job,publication_retry_bytes))
            publication_retry_in_flight=true;
        else if(!saves.Pop(job)) {
            const auto due=retries.NextDue();
            const auto publication_due=publication_retries.NextDue();
            const auto now=GetTickCount64();
            const auto next_due=std::min(due,publication_due);
            const DWORD wait=next_due==UINT64_MAX?200u:DWORD(std::min<uint64_t>(200,next_due>now?next_due-now:0));
            WaitForSingleObject(save_event,wait);continue;
        }
        std::string identity=Utf8(job.heard.media.artist)+" - "+Utf8(job.heard.media.title);
        bool publication_retry_requeued=false;
        try {
            {
                auto settings=GetSettings();
                char line[1400];snprintf(line,sizeof(line),
                    "%s capture=%s capture_duration=%.3f media_duration=%.3f pages_or_frames=%u bytes=%llu listen_wall=%.3f epoch=%u current_epoch=%u preset=%s ffmpeg=%s",
                    identity.c_str(),job.capture->lossless?"flac":"ogg",job.capture->Duration(),job.heard.media.duration,
                    job.capture->lossless?job.capture->coverage.frames:job.capture->stream.pages,
                    static_cast<unsigned long long>(job.capture->bytes),job.heard.finish-job.heard.start,
                    job.epoch,settings.capture_epoch,Utf8(settings.output_preset).c_str(),
                    settings.ffmpeg_path.empty()?"<auto>":Utf8(settings.ffmpeg_path).c_str());
                LogActivity("publish",line);QueueDiagnostic(line);
            }
            SetClassicTrackStatus(job.heard.media,"CONVERTING","Converting...");
            auto result=Publish(*job.capture,job.heard,job.epoch,
                publication_retry_in_flight,job.retry_generation);
            if(result.state==Publication::Saved || (result.state==Publication::Skipped && !result.path.empty())) {
                auto settings=GetSettings();
                if(result.state==Publication::Saved && settings.capture_epoch==job.epoch) ++saved_count;
                SetClassicTrackStatus(job.heard.media,"DONE",result.message,result.path);
                LogActivity("finished",identity+(result.state==Publication::Skipped?" (already downloaded)":""));
            } else if(result.state==Publication::Failed) {
                if(QueueTransientRetry(retries,result,job)) {
                    SetClassicTrackStatus(job.heard.media,"CONVERTING","File is busy; retry scheduled (1 of "+
                        std::to_string(transient_retry_limit)+")",result.retry_path);
                    LogActivity("conversion_retry_queued",identity+" path="+Utf8(result.retry_path));
                } else {
                    const auto message=result.message.empty()?std::string("Publication failed"):result.message;
                    SetClassicTrackStatus(job.heard.media,"ERROR",message,
                        result.retryable&&!result.retry_path.empty()?result.retry_path:result.path);
                    LogActivity("failed",identity+" ("+message+")");
                }
            } else if(result.state==Publication::Busy) {
                const auto latest=GetSettings();
                bool queued=false;
                if(job.retry_attempts<transient_retry_limit&&CanQueuePublicationRetry(result,job,latest)) {
                    const unsigned attempt=job.retry_attempts+1;
                    job.retry_generation=result.generation;job.retry_attempts=attempt;
                    const auto due=GetTickCount64()+transient_retry_delay_ms(attempt-1);
                    const auto retry_bytes=job.reserved;
                    if(publication_retry_in_flight)
                        queued=publication_retries.Requeue(std::move(job),publication_retry_bytes,due);
                    else
                        queued=publication_retries.Push(std::move(job),retry_bytes,due);
                    if(queued)publication_retry_requeued=true;
                    if(queued) {
                        const auto& retained=publication_retries.At(publication_retries.Size()-1);
                        SetClassicTrackStatus(retained.heard.media,
                            "CONVERTING","Recording file busy; retry scheduled ("+std::to_string(attempt)+" of "+
                            std::to_string(transient_retry_limit)+")",result.path);
                        LogActivity("publication_retry_queued",identity+" path="+Utf8(result.path));
                    }
                }
                if(!queued) {
                    const auto current=GetSettings();
                    const bool canceled=result.generation!=current.generation||job.epoch!=current.capture_epoch||
                        !current.downloads||!(job.capture->lossless?current.flac:current.ogg);
                    if(canceled) {
                        SetClassicTrackStatus(job.heard.media,"ERROR","Canceled: settings changed",result.path);
                        LogActivity("publication_retry_canceled",identity+" path="+Utf8(result.path));
                    } else {
                        SetClassicTrackStatus(job.heard.media,"ERROR","Recording file remained busy; capture not published",result.path);
                        LogActivity("publication_retry_failed",identity+" path="+Utf8(result.path));
                    }
                }
            } else {
                SetClassicTrackStatus(job.heard.media,"ERROR","Canceled: settings changed",result.path);
                LogActivity("failed",identity+" (settings changed)");
            }
        } catch(...) {
            if(publication_retry_requeued) {
                try {LogActivity("publication_retry_queued","status update failed after capture was retained");} catch(...) {}
            } else {
                try {
                    SetClassicTrackStatus(job.heard.media,"ERROR","Publication exception");
                    LogActivity("failed",identity+" (publication exception)");
                } catch(...) {}
            }
        }
        if(!publication_retry_requeued) {
            if(publication_retry_in_flight)publication_retries.ReleaseInFlight(publication_retry_bytes);
            job.capture.reset();publishing_reserved.fetch_sub(job.reserved);
        }
    }
    while(publication_retries.Size()) {
        const auto& pending=publication_retries.At(0);
        const auto reserved=pending.reserved;
        try {
            SetClassicTrackStatus(pending.heard.media,"ERROR","Canceled: publisher stopped");
            LogActivity("publication_retry_canceled","publisher stopped");
        } catch(...) {}
        publication_retries.Discard(0);publishing_reserved.fetch_sub(reserved);
    }
    while(retries.Size()) {
        auto canceled=retries.At(0);
        try {
            SetClassicTrackStatus(RetryStatusMedia(canceled),"ERROR","Canceled: publisher stopped",canceled.path);
            LogActivity("conversion_retry_canceled",Utf8(canceled.path));
        } catch(...) {}
        retries.Discard(0);
    }
    return 0;
}
static DWORD WINAPI Worker(LPVOID) {
    LONG overflow=0;
    // Ogg sync and FLAC decoder pointers occupy distinct namespaces.
    std::map<std::pair<unsigned,uintptr_t>,std::unique_ptr<Capture>> active;
    std::vector<std::unique_ptr<Capture>> ready;
    uint64_t next_dump_stream_id=0;
    auto abort_active=[&](const char* event_name) {
        for(const auto& item:active) {
            const Capture& capture=*item.second;
            QueueStreamDump(capture.dump_stream_id,capture.epoch,capture.lossless,event_name,
                "incomplete",1,nullptr,0,0);
        }
    };
    std::vector<Heard> heard;
    Listen listen; MediaSnapshotReader reader; Media current; PlaybackQualityTracker quality; std::string client_quality;
    double last_media=Now(),next_log=0,next_probe=0;
    uint64_t media_sequence=0;bool observer_started=false,media_unavailable=false;
    uint64_t client_boundary=0;double boundary_time=0;bool instance_seen=false;
    DecodeProbeSnapshot previous_probe{};
    LONG previous_ogg_calls=0,previous_ogg_pages=0;
    unsigned generation=~0u,epoch=~0u; bool enabled=false,current_ignored=false;
    try {
        while(workers_running.load(std::memory_order_acquire)) {
            double now=Now();
            const auto client=client_playback.Read(now);
            if(client.boundary!=client_boundary) {
                client_boundary=client.boundary;boundary_time=now;
                if(!current.title.empty())SetClassicTrackStatus(current,"ERROR","Canceled: client playback boundary");
                abort_active("stream_aborted");
                active.clear();ready.clear();heard.clear();listen={};current={};quality.ClearStreams();
                Log("client playback boundary; pending captures and listening coverage invalidated");
            }
            auto preferences=GetSettings();
            const double playback_rate=ClassicEffectiveSpeed(
                preferences.downloads,preferences.playback_speed,PlaybackSpeedSupported());
            if(preferences.generation!=generation) {
                bool initial=generation==~0u;
                generation=preferences.generation;
                output=preferences.root; memory_limit=size_t(preferences.max_buffered_mib)*1024*1024;
                bool capture=preferences.downloads && (preferences.ogg || preferences.flac);
                if(capture!=enabled || initial || preferences.capture_epoch!=epoch) {
                    abort_active("stream_aborted");
                    active.clear(); ready.clear(); heard.clear(); listen={}; current={};
                    last_media=now;media_unavailable=false;
                    if(!enabled && capture) saved_count=0;
                    enabled=capture;
                    Log(enabled ? "To Disk capture enabled" : "To Disk capture disabled");
                }
                epoch=preferences.capture_epoch;
                const auto preset=Utf8(preferences.output_preset);
                const auto root=Utf8(preferences.root);
                const auto ffmpeg=Utf8(preferences.ffmpeg_path);
                char line[1536];
                snprintf(line,sizeof(line),
                    "generation=%u epoch=%u enabled=%d downloads=%d ogg=%d flac=%d speed_config=%.3f speed_hook=%d speed_effective=%.3f preset=%s ffmpeg=%s root=%s debug=%d",
                    preferences.generation,preferences.capture_epoch,enabled,preferences.downloads,
                    preferences.ogg,preferences.flac,preferences.playback_speed,
                    PlaybackSpeedSupported(),playback_rate,preset.c_str(),
                    ffmpeg.empty()?"<auto>":ffmpeg.c_str(),root.c_str(),preferences.debug_log);
                LogActivity("config",line);
                // Configure telemetry on the worker thread. Decoder callbacks
                // only update relaxed counters, never format diagnostic lines.
                SetPlaybackSpeedProbeEnabled(preferences.debug_log);
                previous_probe=ReadPlaybackSpeedProbe();
                previous_ogg_calls=calls;previous_ogg_pages=pages;
                next_probe=now+2.0;
                if(preferences.debug_log)
                    Log("decoder_probe activated: passive enter/return/input/PCM counters; audio unchanged");
            }
            if(!enabled && !quality_enabled.load(std::memory_order_relaxed)) {
                reader.Stop();observer_started=false;
                PublishPlaybackQuality({});
                Slot discard; while(Pop(discard)) {}
                WaitForSingleObject(event,200); continue;
            }
            if(InterlockedCompareExchange(&dropped,0,0)!=overflow) {
                const LONG observed_loss=InterlockedCompareExchange(&dropped,0,0);
                const uint32_t lost=uint32_t(observed_loss)-uint32_t(overflow);
                QueueStreamDump(0,epoch==~0u?0:epoch,false,"capture_queue_loss","incomplete",
                    lost,nullptr,0,lost);
                abort_active("stream_aborted");
                AcquireSRWLockExclusive(&queue_lock);
                overflow=observed_loss;head=tail;count=0;
                ReleaseSRWLockExclusive(&queue_lock);
                if(!current.title.empty())SetClassicTrackStatus(current,"ERROR","Canceled: capture queue overflow");
                active.clear(); ready.clear(); heard.clear(); listen.Invalidate(); quality.ClearStreams();
                Log("capture queue overflow; streams and listening coverage invalidated");
            }
            Slot s;
            while(Pop(s)) {
                if(s.kind==1) {
                    char prefix[33]={}; for(unsigned i=0;i<s.length;i++) snprintf(prefix+2*i,3,"%02x",s.bytes[i]);
                    char line[180]; snprintf(line,sizeof(line),"FORMAT kind=%u matched=%d input_bytes=%llu prefix=%s",
                      s.format,s.matched,static_cast<unsigned long long>(s.input_length),prefix); Log(line); continue;
                }
                if(s.epoch!=epoch||s.loss!=overflow||s.boundary!=client_boundary)continue;
                const auto key=std::make_pair(s.kind>=2?1u:0u,s.context);
                if(quality_enabled.load(std::memory_order_relaxed)) {
                    switch(s.kind) {
                        case 0: quality.Ogg(s.context,s.bytes,s.length,s.time); break;
                        case 2: quality.FlacBegin(s.context,s.time); break;
                        case 3: quality.FlacBytes(s.context,s.bytes,s.length,size_t(s.input_length)); break;
                        case 4: quality.FlacFrame(s.context,s.bytes,s.length); break;
                        case 5: quality.FlacEnd(s.context,true); break;
                        case 6: quality.FlacEnd(s.context,false); break;
                    }
                }
                if(!enabled || !s.capture || s.epoch!=epoch) continue;
                if(s.kind>=2) {
                    auto found=active.find(key);
                    if(s.kind==2) {
                        if(found!=active.end()) {
                            const Capture& prior=*found->second;
                            QueueStreamDump(prior.dump_stream_id,prior.epoch,true,"flac_restarted",
                                "incomplete",1,nullptr,0,0);
                        }
                        auto c=std::make_unique<Capture>(); c->lossless=true; c->born=s.time;
                        c->epoch=s.epoch;c->dump_stream_id=++next_dump_stream_id;
                        c->owner.Accept(s.owner);
                        QueueStreamDump(c->dump_stream_id,c->epoch,true,"flac_init","observed",
                            s.format,nullptr,0,0);
                        active[key]=std::move(c); Log("FLAC decoder initialized; capture begins at compressed offset zero"); continue;
                    }
                    if(found==active.end() || !found->second->lossless) continue;
                    Capture& c=*found->second;
                    if(!c.owner.Accept(s.owner)){
                        QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_rejected","rejected",1,nullptr,0,0);
                        active.erase(found);Log("FLAC decoder owner conflict; stream discarded");continue;
                    }
                    if(s.kind==5) {
                        QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_error","error",s.format,nullptr,0,0);
                        active.erase(found); Log("FLAC decoder error; stream discarded"); continue;
                    }
                    if(s.kind==3) {
                        QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_read","read",0,
                            s.bytes,s.length,s.input_length);
                        size_t reserved=publishing_reserved.load(); for(const auto& item:active) reserved+=item.second->data.capacity();
                        for(const auto& item:ready) reserved+=item->data.capacity();
                        size_t available=reserved<memory_limit ? memory_limit-reserved : 0;
                        if(!c.data.Append(s.bytes,s.length,available)) {
                            QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_rejected","rejected",2,nullptr,0,0);
                            active.erase(found); Log("FLAC capture memory limit; stream discarded"); continue;
                        }
                        c.bytes+=s.length;
                        if(!c.metadata) {
                            size_t audio=0; auto parsed=ParseFlacMetadata(c.data,c.flac,audio);
                            if(parsed==FlacParse::Invalid) {
                                QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_rejected","rejected",3,nullptr,0,0);
                                active.erase(found); Log("FLAC metadata invalid; stream discarded"); continue;
                            }
                            if(parsed==FlacParse::Valid) {
                                c.metadata=true; char line[180]; snprintf(line,sizeof(line),"FLAC STREAMINFO rate=%u channels=%u bits=%u total=%llu metadata_bytes=%zu",c.flac.rate,c.flac.channels,c.flac.bits,static_cast<unsigned long long>(c.flac.total_samples),audio); Log(line);
                            }
                        }
                        continue;
                    }
                    if(s.kind==4) {
                        QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_frame","observed",s.format,nullptr,0,0);
                        uint32_t frame[6]; uint64_t number; memcpy(frame,s.bytes,24); memcpy(&number,s.bytes+24,8);
                        if(frame[5]==0) number=uint32_t(number);
                        if(!c.metadata || !c.coverage.Frame(c.flac,frame[0],frame[1],frame[2],frame[4],frame[5],number)) {
                            QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_rejected","rejected",4,nullptr,0,0);
                            active.erase(found); Log("FLAC decoded-frame gap or format mismatch; stream discarded"); continue;
                        }
                        if(!c.coverage.Complete(c.flac)) continue;
                        QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_complete","complete",
                            c.coverage.frames,nullptr,0,c.bytes);
                    } else if(s.kind!=6 || !c.coverage.Complete(c.flac)) {
                        if(s.kind==6)QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_eof","truncated",0,nullptr,0,c.bytes);
                        else QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_rejected","rejected",5,nullptr,0,0);
                        active.erase(found); Log("FLAC truncated before total samples; discarded"); continue;
                    }
                    if(s.kind==6)QueueStreamDump(c.dump_stream_id,c.epoch,true,"flac_eof","complete",0,nullptr,0,c.bytes);
                    c.finished=s.time;
                    char line[180]; snprintf(line,sizeof(line),"FLAC COMPLETE frames=%u samples=%llu bytes=%llu duration=%.6f",c.coverage.frames,static_cast<unsigned long long>(c.coverage.samples),static_cast<unsigned long long>(c.bytes),c.Duration()); Log(line);
                    ready.push_back(std::move(found->second)); active.erase(found); continue;
                }
                Page page;
                auto found=active.find(key);
                if(!ParsePage(s.bytes,s.length,page)) {
                    if(found!=active.end()) {
                        const Capture& capture=*found->second;
                        QueueStreamDump(capture.dump_stream_id,capture.epoch,false,"ogg_page","rejected",
                            0xffffffffu,s.bytes,s.length,s.length);
                    }
                    active.erase(key); Log("invalid Ogg page rejected"); continue;
                }

                if(found!=active.end()&&!found->second->owner.Accept(s.owner)) {
                    const Capture& capture=*found->second;
                    QueueStreamDump(capture.dump_stream_id,capture.epoch,false,"ogg_page","rejected",
                        0xfffffffeu,s.bytes,s.length,s.length);
                    active.erase(found);Log("Ogg decoder owner conflict; stream discarded");continue;
                }
                if(page.vorbis_start) {
                    // A decoder may replay its identification/BOS page after
                    // buffering. Preserve an in-progress capture when this is
                    // the same logical stream and an already-consumed sequence.
                    if(found!=active.end() && !found->second->lossless &&
                       found->second->stream.active &&
                       page.serial==found->second->stream.serial &&
                       page.sequence<found->second->stream.next) {
                        const Capture& capture=*found->second;
                        QueueStreamDump(capture.dump_stream_id,capture.epoch,false,"ogg_page","replayed",
                            static_cast<unsigned>(Result::Replay),s.bytes,s.length,s.length);
                        if(DebugLoggingEnabled())Log("Ogg replayed BOS page ignored");
                        continue;
                    }
                    if(found!=active.end()) {
                        const Capture& prior=*found->second;
                        QueueStreamDump(prior.dump_stream_id,prior.epoch,false,"ogg_restarted",
                            "incomplete",1,nullptr,0,0);
                    }
                    auto c=std::make_unique<Capture>(); c->born=s.time;
                    c->epoch=s.epoch;c->dump_stream_id=++next_dump_stream_id;
                    c->owner.Accept(s.owner);
                    active[key]=std::move(c);
                    found=active.find(key);
                    char line[512]; snprintf(line,sizeof(line),
                        "ogg_bos ctx=%llx seq=%u serial=%u rate=%u channels=%u media=%s pos=%.3f raw=%.3f duration=%.3f rate_effective=%.3f",
                        static_cast<unsigned long long>(s.context),page.sequence,page.serial,page.rate,page.channels,
                        Utf8(current.title).c_str(),current.position,current.raw_position,current.duration,playback_rate);
                    LogActivity("capture",line);QueueDiagnostic(line);
                }
                if(found==active.end()) continue;
                Capture& c=*found->second;
                const auto expected_seq=c.stream.next;
                const auto expected_serial=c.stream.serial;
                const auto prior_samples=c.stream.samples;
                const auto prior_pages=c.stream.pages;
                Result result=c.stream.Push(s.bytes,s.length);
                if(result==Result::Replay) {
                    QueueStreamDump(c.dump_stream_id,c.epoch,false,"ogg_page","replayed",
                        static_cast<unsigned>(result),s.bytes,s.length,s.length);
                    if(DebugLoggingEnabled()) {
                        char line[512];snprintf(line,sizeof(line),
                            "ogg_replay ctx=%llx seq=%u expected=%u serial=%u flags=0x%02x granule=%lld pages=%u bytes=%llu",
                            static_cast<unsigned long long>(s.context),page.sequence,expected_seq,page.serial,page.flags,
                            static_cast<long long>(page.granule),prior_pages,static_cast<unsigned long long>(c.bytes));
                        Log(line);
                    }
                    continue;
                }
                if(result==Result::Invalid || result==Result::Ignore) {
                    QueueStreamDump(c.dump_stream_id,c.epoch,false,"ogg_page","rejected",
                        static_cast<unsigned>(result),s.bytes,s.length,s.length);
                    char line[1024];snprintf(line,sizeof(line),
                        "ogg_rejected result=%s ctx=%llx seq=%u expected_seq=%u serial=%u expected_serial=%u flags=0x%02x granule=%lld prior_samples=%lld pages=%u bytes=%llu media=%s pos=%.3f raw=%.3f duration=%.3f rate=%.3f",
                        result==Result::Invalid?"invalid":"ignore",
                        static_cast<unsigned long long>(s.context),page.sequence,expected_seq,page.serial,expected_serial,page.flags,
                        static_cast<long long>(page.granule),static_cast<long long>(prior_samples),prior_pages,
                        static_cast<unsigned long long>(c.bytes),Utf8(current.title).c_str(),current.position,current.raw_position,
                        current.duration,playback_rate);
                    LogActivity("failed",line);QueueDiagnostic(line);
                    active.erase(found); continue;
                }
                QueueStreamDump(c.dump_stream_id,c.epoch,false,"ogg_page","accepted",
                    static_cast<unsigned>(result),s.bytes,s.length,s.length);
                size_t reserved=publishing_reserved.load();
                for(const auto& item:active) reserved+=item.second->data.capacity();
                for(const auto& item:ready) reserved+=item->data.capacity();
                size_t available=reserved<memory_limit ? memory_limit-reserved : 0;
                if(!c.data.Append(s.bytes,s.length,available)) {
                    QueueStreamDump(c.dump_stream_id,c.epoch,false,"ogg_capture_rejected","rejected",2,nullptr,0,0);
                    active.erase(found);
                    Log("capture memory limit reached; track discarded without scratch files"); continue;
                }
                c.bytes+=s.length;
                if(result==Result::Complete) {
                    c.finished=s.time;
                    char line[768]; snprintf(line,sizeof(line),
                      "ogg_eos ctx=%llx pages=%u bytes=%llu stream_duration=%.6f capture_seconds=%.3f media=%s media_pos=%.3f raw=%.3f media_duration=%.3f rate=%.3f ready_before=%zu",
                      static_cast<unsigned long long>(s.context),c.stream.pages,static_cast<unsigned long long>(c.bytes),
                      c.stream.Duration(),c.finished-c.born,Utf8(current.title).c_str(),current.position,current.raw_position,
                      current.duration,playback_rate,ready.size());
                    LogActivity("capture",line);QueueDiagnostic(line);
                    ready.push_back(std::move(found->second)); active.erase(found);
                }
            }
            const MediaReadOptions media_options{preferences.generation,epoch,playback_rate,enabled};
            if(!observer_started)observer_started=reader.Start(media_options);
            Media media;double observed_at=0;
            auto observation=observer_started
                ?reader.TryRead(media,observed_at,media_sequence,media_options):MediaRead::Pending;
            std::string listen_key;
            if(observation==MediaRead::Ready) {
                // Client state corroborates the SMTC sample; never attach an ID
                // to stale labels or silently fall back after ID support was seen.
                if(client.boundary!=client_playback.Token()||observed_at<boundary_time||
                   (client.seen&&!client.available)||
                   (instance_seen&&client.instance.empty()))observation=MediaRead::Failed;
                else if(client.seen&&!ClientPlaybackAgrees(client,Utf8(media.title),Utf8(media.artist),
                    Utf8(media.album),media.position,media.duration,observed_at,playback_rate))
                    observation=MediaRead::Pending;
                else {
                    listen_key=media.Key();
                    if(!client.instance.empty()) {
                        if(!instance_seen){listen={};instance_seen=true;}
                        listen_key=client.media.uri+"/"+client.instance;
                    }
                    if(client.paused==1||client.buffering==1)media.playing=false;
                }
            }
            // Discard a result if settings changed concurrently with polling.
            if(GetSettings().generation!=generation)continue;
            if(observation==MediaRead::Ready) {
                last_media=observed_at;media_unavailable=false;
                quality.Media(media.Key(),media.title,media.position,media.duration,observed_at,media.playing);
                client_quality=ReadClientPlaybackQuality(media);
            }
            if(enabled && observation==MediaRead::Ready) {
                std::string previous=listen.identity; double previous_start=listen.start_time;
                const auto previous_instance=listen.generation;
                const auto previous_phase=listen.phase;
                bool was_eligible=listen.eligible;const bool prior_playing=listen.playing;
                double prior_position=listen.last_position,prior_duration=listen.duration,prior_time=listen.last_time;
                if(DebugLoggingEnabled()) {
                    char line[1200];snprintf(line,sizeof(line),
                        "media_sample title=%s artist=%s key=%s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f playing=%d prior=%.3f prior_duration=%.3f wall_delta=%.3f speed_config=%.3f speed_hook=%d rate=%.3f eligible=%d active=%zu ready=%zu",
                        Utf8(media.title).c_str(),Utf8(media.artist).c_str(),media.Key().c_str(),media.position,media.raw_position,
                        media.duration,media.timeline_age,media.playing,prior_position,prior_duration,observed_at-prior_time,
                        preferences.playback_speed,PlaybackSpeedSupported(),playback_rate,listen.eligible,active.size(),ready.size());
                    Log(line);
                }
                std::string done=listen.Observe(listen_key,media.position,media.duration,media.playing,observed_at,playback_rate,!client.instance.empty());
                const MetadataInstance prior_metadata{previous,previous_instance,!client.instance.empty()};
                const MetadataInstance latest_metadata{listen.identity,listen.generation,!client.instance.empty()};
                if(!listen.transient)for(auto& completed:heard)
                    RefreshListenMetadata(completed.media,media,
                        {completed.identity,completed.generation,completed.exact_identity},latest_metadata,true);
                if(previous_instance!=listen.generation || previous_phase!=listen.phase || previous!=listen.identity) {
                    char line[512];snprintf(line,sizeof(line),
                        "listen_instance generation=%llu previous=%llu phase=%u previous_phase=%u relabeled=%d completed=%llu start=%.3f wrap=%.3f",
                        static_cast<unsigned long long>(listen.generation),static_cast<unsigned long long>(previous_instance),
                        unsigned(listen.phase),unsigned(previous_phase),previous!=listen.identity && previous_instance==listen.generation,
                        static_cast<unsigned long long>(done.empty()?0:listen.completed_generation),listen.start_time,listen.wrap_time);
                    Log(line);
                }
                if(was_eligible && !listen.eligible && done.empty()) {
                    const auto identity=Utf8(current.artist)+" - "+Utf8(current.title);
                    char line[1400]; snprintf(line,sizeof(line),
                        "%s (incomplete listen reason=%s prior=%.3f/%.3f new=%.3f raw=%.3f/%.3f timeline_age=%.3f delta=%.3f expected=%.3f tolerance=%.3f rate=%.3f playing_before=%d playing_now=%d key_changed=%d)",
                        identity.c_str(),ListenRejectName(listen.reject),prior_position,prior_duration,
                        media.position,media.raw_position,media.duration,media.timeline_age,observed_at-prior_time,
                        listen.last_expected,listen.last_tolerance,listen.last_rate,prior_playing,media.playing,
                        previous!=listen.identity);
                    LogActivity("failed",line);QueueDiagnostic(line);
                }
                if(!done.empty()) {
                    if(current_ignored) {
                        SetClassicTrackStatus(current,"IGNORED","Ignored");
                        LogActivity("finished",Utf8(current.artist)+" - "+Utf8(current.title)+" (ignored)");
                    } else {
                        auto completed_media=current;
                        // A pause at the natural end completes this same instance.
                        // Include its final corroborated metadata; a transition or
                        // repeat must never label the preceding listen.
                        RefreshListenMetadata(completed_media,media,prior_metadata,latest_metadata,true);
                        completed_media.duration=listen.completed_duration;
                        heard.push_back({std::move(completed_media),listen.completed_start,listen.completed_time,
                            listen.completed_generation,done,prior_metadata.exact});
                        char line[1024];snprintf(line,sizeof(line),
                            "%s - %s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f elapsed_wall=%.3f rate=%.3f ready=%zu active=%zu generation=%llu confirmation_delay=%.3f",
                            Utf8(current.artist).c_str(),Utf8(current.title).c_str(),prior_position,current.raw_position,
                            listen.completed_duration,current.timeline_age,listen.completed_time-listen.completed_start,
                            playback_rate,ready.size(),active.size(),static_cast<unsigned long long>(listen.completed_generation),
                            observed_at-listen.completed_time);
                        LogActivity("listen_complete",line);QueueDiagnostic(line);
                    }
                }
                if((previous_instance!=listen.generation || previous!=listen.identity) && listen.eligible) {
                    current_ignored=ClassicCurrentIgnored();
                    SetClassicTrackStatus(media,current_ignored?"IGNORED":"IN_PROGRESS",current_ignored?"Ignored":"Downloading...");
                    char line[1400];snprintf(line,sizeof(line),
                        "%s - %s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f playing=%d speed_config=%.3f speed_hook=%d speed_effective=%.3f start_window=%.3f active=%zu ready=%zu epoch=%u",
                        Utf8(media.artist).c_str(),Utf8(media.title).c_str(),media.position,media.raw_position,media.duration,
                        media.timeline_age,media.playing,preferences.playback_speed,PlaybackSpeedSupported(),playback_rate,
                        std::max(1.5,playback_rate*0.75),active.size(),ready.size(),epoch);
                    LogActivity("started",line);QueueDiagnostic(line);
                } else if(previous==listen.identity && listen.eligible) {
                    current_ignored=ClassicCurrentIgnored();
                }
                if(was_eligible && !listen.eligible && done.empty()) {
                    if(!current.title.empty())SetClassicTrackStatus(current,"ERROR",
                        std::string("Canceled: ")+ListenRejectName(listen.reject));
                }
                if(previous_instance!=listen.generation) {
                    Log(("TRACK "+Utf8(media.artist)+" - "+Utf8(media.title)).c_str());
                    CleanupCaptureTransition(active,ready,heard,previous_start,
                        listen.start_time,!done.empty(),Now());
                }
                if(listen.transient) {if(previous_phase!=listen.phase)Log("timeline reset at natural end; waiting for title or repeat confirmation");}
                else if(!RefreshListenMetadata(current,std::move(media),prior_metadata,latest_metadata))current=std::move(media);
            } else if(observation==MediaRead::Failed || Now()-last_media>3) {
                const bool was_eligible=listen.eligible;
                listen.Invalidate(); quality.Media({},L"",0,0,Now());client_quality.clear();
                if(was_eligible) {
                    char line[768];snprintf(line,sizeof(line),
                        "%s - %s (media snapshot unavailable or stale rate=%.3f active=%zu ready=%zu epoch=%u sequence=%llu state=%s last_sample_age=%.3f)",
                        Utf8(current.artist).c_str(),Utf8(current.title).c_str(),playback_rate,active.size(),ready.size(),epoch,
                        static_cast<unsigned long long>(media_sequence),observation==MediaRead::Failed?"failed":"pending",Now()-last_media);
                    LogActivity("failed",line);QueueDiagnostic(line);
                } else if(!media_unavailable)QueueDiagnostic("Spotify media snapshot unavailable or stale while no eligible listen was active");
                media_unavailable=true;
            }
            if(!enabled) {
                PublishPlaybackQuality(quality.Snapshot(client_quality));
                WaitForSingleObject(event,50);continue;
            }
            now=Now();
            PublishPlaybackQuality(quality.Snapshot(client_quality));
            // Apply concurrent menu changes before considering publication.
            if(GetSettings().generation!=generation||InterlockedCompareExchange(&dropped,0,0)!=overflow||
               client_boundary!=client_playback.Token()||(client_boundary&1)) {WaitForSingleObject(event,50);continue;}
            const auto shared_captures=RejectSharedCaptures(ready,heard);
            size_t heard_index=0;
            for(auto h=heard.begin();h!=heard.end();++heard_index) {
                auto selection=SelectCapture(active,ready,*h,now);
                if(shared_captures[heard_index]) {
                    selection.state=CaptureAssociation::Ambiguous;
                    selection.matches+=shared_captures[heard_index];
                }
                const auto match=selection.index,matches=selection.matches;
                if(selection.state==CaptureAssociation::Ready) {
                    SaveJob job;job.reserved=ready[match]->data.capacity();job.epoch=epoch;
                    job.capture=std::move(ready[match]);job.heard=*h;
                    publishing_reserved.fetch_add(job.reserved);
                    if(saves.Push(std::move(job)))SetEvent(save_event);
                    else {
                        SetClassicTrackStatus(h->media,"ERROR","Publication queue full");
                        size_t reserved=job.reserved;job.capture.reset();publishing_reserved.fetch_sub(reserved);
                        LogActivity("failed","publication queue full; completed candidate discarded");
                    }
                    ready.erase(ready.begin()+match); h=heard.erase(h);
                } else if(selection.state==CaptureAssociation::Ambiguous ||
                          selection.state==CaptureAssociation::Missing) {
                    SetClassicTrackStatus(h->media,"ERROR",selection.state==CaptureAssociation::Ambiguous?"Ambiguous audio stream association":"Completed audio stream was not found");
                    std::string candidates;
                    for(size_t i=0;i<ready.size() && i<6;i++) {
                        char item[160];snprintf(item,sizeof(item),"%s%.3fs@%+.3fs",
                            i?",":"",ready[i]->Duration(),ready[i]->born-h->start);
                        candidates+=item;
                    }
                    char line[1400];snprintf(line,sizeof(line),
                        "%s - %s (stream association failed matches=%zu ready=%zu retained_active=%zu shared_listen_matches=%zu retention_conflicts=%zu media_duration=%.3f listen_start=%.3f listen_finish=%.3f wait=%.3f candidates=[%s])",
                        Utf8(h->media.artist).c_str(),Utf8(h->media.title).c_str(),matches,ready.size(),
                        selection.pending,shared_captures[heard_index],selection.retention_conflicts,h->media.duration,h->start,h->finish,now-h->finish,candidates.c_str());
                    LogActivity("failed",line);QueueDiagnostic(line);
                    h=heard.erase(h);
                } else ++h;
            }
            PruneRetainedCaptures(active,ready,heard,now);
            ready.erase(std::remove_if(ready.begin(),ready.end(),[now](const auto& c){return now-c->finished>600;}),ready.end());
            for(auto it=active.begin();it!=active.end();) {
                if(now-it->second->born>21600) it=active.erase(it); else ++it;
            }
            // Periodic, read-only source-of-stall probe on existing worker.
            if(enabled && preferences.debug_log && now>=next_probe) {
                const auto snap=ReadPlaybackSpeedProbe();
                const auto tick=static_cast<std::uint64_t>(GetTickCount64());
                const auto delta=[](std::uint64_t a,std::uint64_t b) {
                    return a>=b ? a-b : std::uint64_t{0};
                };
                const auto age=snap.last_exit_ms && tick>=snap.last_exit_ms ?
                    static_cast<long long>(tick-snap.last_exit_ms) : -1ll;
                const LONG ogg_calls=calls,ogg_pages=pages;
                const auto inflight=delta(snap.enters,snap.exits);
                char probe_line[1400];
                std::snprintf(probe_line,sizeof(probe_line),
                    "decoder_probe observation=%s title=%s enter=%llu exit=%llu inflight=%llu enter_delta=%llu exit_delta=%llu "
                    "last_exit_age_ms=%lld pcm_decoded_delta=%llu pcm_kept_delta=%llu encoded_reported_delta=%llu "
                    "zero_pcm_delta=%llu no_encoded_delta=%llu bad_output_delta=%llu "
                    "ogg_calls_delta=%ld ogg_pages_delta=%ld last_capacity=%llu last_pcm=%llu last_kept=%llu "
                    "last_encoded_before=%llu last_encoded_after=%llu last_flags=%llu",
                    DescribeDecodeProbe(previous_probe,snap,tick),Utf8(current.title).c_str(),
                    static_cast<unsigned long long>(snap.enters),
                    static_cast<unsigned long long>(snap.exits),
                    static_cast<unsigned long long>(inflight),
                    static_cast<unsigned long long>(delta(snap.enters,previous_probe.enters)),
                    static_cast<unsigned long long>(delta(snap.exits,previous_probe.exits)),
                    age,
                    static_cast<unsigned long long>(delta(snap.decoded_samples,previous_probe.decoded_samples)),
                    static_cast<unsigned long long>(delta(snap.reported_samples,previous_probe.reported_samples)),
                    static_cast<unsigned long long>(delta(snap.compressed_units,previous_probe.compressed_units)),
                    static_cast<unsigned long long>(delta(snap.no_pcm_calls,previous_probe.no_pcm_calls)),
                    static_cast<unsigned long long>(delta(snap.no_input_change_calls,previous_probe.no_input_change_calls)),
                    static_cast<unsigned long long>(delta(snap.invalid_output_calls,previous_probe.invalid_output_calls)),
                    static_cast<long>(ogg_calls-previous_ogg_calls),
                    static_cast<long>(ogg_pages-previous_ogg_pages),
                    static_cast<unsigned long long>(snap.last_capacity),
                    static_cast<unsigned long long>(snap.last_produced),
                    static_cast<unsigned long long>(snap.last_kept),
                    static_cast<unsigned long long>(snap.last_encoded_before),
                    static_cast<unsigned long long>(snap.last_encoded_after),
                    static_cast<unsigned long long>(snap.last_flags));
                Log(probe_line);
                previous_probe=snap;
                previous_ogg_calls=ogg_calls;
                previous_ogg_pages=ogg_pages;
                next_probe=now+2.0;
            }
            if(now>=next_log) {
                size_t buffered=0;
                for(const auto& item:active) buffered+=item.second->data.capacity();
                for(const auto& item:ready) buffered+=item->data.capacity();
                char line[900]; snprintf(line,sizeof(line),
                  "status calls=%ld pages=%ld dropped=%ld active=%zu ready=%zu heard=%zu saved=%u title=%s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f eligible=%d pending=%d rate=%.3f speed_config=%.3f speed_hook=%d buffered=%zu",
                  calls,pages,dropped,active.size(),ready.size(),heard.size(),saved_count.load(),Utf8(current.title).c_str(),
                  current.position,current.raw_position,current.duration,current.timeline_age,listen.eligible,listen.pending_start,
                  playback_rate,preferences.playback_speed,PlaybackSpeedSupported(),buffered); Log(line); next_log=now+5;
            }
            WaitForSingleObject(event,50);
        }
    } catch(...) { Log("history worker stopped after an exception; playback remains with original parser"); }
    reader.Stop(); // Shared observer state outlives any outstanding COM call.
    PublishPlaybackQuality({});
    workers_running.store(false,std::memory_order_release);
    if(save_event) SetEvent(save_event);
    MH_DisableHook(target);
    MH_DisableHook(sniff_target);
    for(auto address:flac_targets) if(address) MH_DisableHook(address);
    return 0;
}
}

void StartAudioHistory(HMODULE spotify_module,HMODULE proxy) {
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    if(!audio_init.TryBegin(now)) return;
    (void)proxy;
    auto preferences=GetSettings(); output=preferences.root;
    quality_enabled.store(preferences.menu,std::memory_order_relaxed);
    if(output.empty()) { Log("save location unavailable; history initialization will retry"); audio_init.Retry(now); return; }
    spotify::HookTargets discovered{};
    const auto discovery=DiscoverTargets(spotify_module,discovered);
    if(discovery!=spotify::DiscoveryResult::Found) {
        char message[220];snprintf(message,sizeof(message),
            "Spotify audio target discovery failed: %s; capture disabled",
            DiscoveryFailure(discovery));Log(message);
        audio_init.MarkUnsupported();return;
    }
    target=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::OggPageSeek);
    sniff_target=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::FormatSniff);
    flac_targets[0]=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::FlacInit);
    flac_targets[1]=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::FlacRead);
    flac_targets[2]=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::FlacFrame);
    flac_targets[3]=DiscoveredTarget(spotify_module,discovered,spotify::TargetKind::FlacError);
    auto release_resources=[] {
        if(event) { CloseHandle(event); event=nullptr; }
        if(save_event) { CloseHandle(save_event); save_event=nullptr; }
        if(queue) { HeapFree(GetProcessHeap(),0,queue); queue=nullptr; }
        head=tail=count=0;
        original=nullptr; original_sniff=nullptr; original_flac_init=nullptr;
        original_flac_read=nullptr; original_flac_frame=nullptr; original_flac_error=nullptr;
        target=nullptr; sniff_target=nullptr;
        for(auto& address:flac_targets) address=nullptr;
    };
    save_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    queue=static_cast<Slot*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Slot)*capacity));
    event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!save_event || !queue || !event) {
        Log("audio queue resources unavailable; initialization will retry"); release_resources(); audio_init.Retry(now); return;
    }
    workers_running.store(true,std::memory_order_release);
    HANDLE publisher=CreateThread(nullptr,0,SaveWorker,nullptr,0,nullptr);
    if(!publisher) {
        workers_running.store(false,std::memory_order_release);
        Log("publication worker startup failed; initialization will retry"); release_resources(); audio_init.Retry(now); return;
    }
    SetThreadPriority(publisher,THREAD_PRIORITY_BELOW_NORMAL);
    HANDLE worker=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    if(!worker) {
        workers_running.store(false,std::memory_order_release); SetEvent(save_event);
        const bool stopped=WaitForSingleObject(publisher,5000)==WAIT_OBJECT_0;CloseHandle(publisher);
        if(stopped) {Log("audio worker startup failed; initialization will retry");release_resources();audio_init.Retry(now);}
        else {Log("audio worker startup failed and publisher did not quiesce; state preserved");audio_init.MarkUnsupported();}
        return;
    }
    auto close_threads=[&] {CloseHandle(publisher);CloseHandle(worker);publisher=nullptr;worker=nullptr;};
    void* created_targets[6]={};unsigned created_count=0;
    auto rollback=[&] {
        hooks::RollbackStatus result;
        auto disabled=[](MH_STATUS value){return value==MH_OK||value==MH_ERROR_DISABLED||value==MH_ERROR_NOT_CREATED;};
        auto removed=[](MH_STATUS value){return value==MH_OK||value==MH_ERROR_NOT_CREATED;};
        for(unsigned i=0;i<created_count;i++)result.ObserveDisable(disabled(MH_DisableHook(created_targets[i])));
        for(unsigned waited=0;audio_callbacks.Active()&&waited<5000;++waited)Sleep(1);
        result.ObserveQuiescence(audio_callbacks.Active()==0);
        if(result.quiescent)for(unsigned i=0;i<created_count;i++)result.ObserveRemove(removed(MH_RemoveHook(created_targets[i])));
        if(!result.CanRelease()){close_threads();return false;}
        workers_running.store(false,std::memory_order_release);SetEvent(event);SetEvent(save_event);
        const bool worker_stopped=WaitForSingleObject(worker,5000)==WAIT_OBJECT_0;
        const bool publisher_stopped=WaitForSingleObject(publisher,5000)==WAIT_OBJECT_0;
        close_threads();
        if(!worker_stopped||!publisher_stopped)return false;
        release_resources();return true;
    };
    auto create_hook=[&](void* address,void* callback,void** trampoline) {
        MH_STATUS result=MH_CreateHook(address,callback,trampoline);
        if(result==MH_OK)created_targets[created_count++]=address;
        return result;
    };
    MH_STATUS status=MH_Initialize(); if(status==MH_ERROR_ALREADY_INITIALIZED) status=MH_OK;
    if(status==MH_OK) status=create_hook(target,reinterpret_cast<void*>(Hook),reinterpret_cast<void**>(&original));
    if(status==MH_OK) status=create_hook(sniff_target,reinterpret_cast<void*>(FormatHook),reinterpret_cast<void**>(&original_sniff));
    void* flac_callbacks[]={reinterpret_cast<void*>(FlacInitHook),reinterpret_cast<void*>(FlacReadHook),reinterpret_cast<void*>(FlacFrameHook),reinterpret_cast<void*>(FlacErrorHook)};
    void** flac_originals[]={reinterpret_cast<void**>(&original_flac_init),reinterpret_cast<void**>(&original_flac_read),reinterpret_cast<void**>(&original_flac_frame),reinterpret_cast<void**>(&original_flac_error)};
    for(unsigned i=0;status==MH_OK && i<4;i++) {
        status=create_hook(flac_targets[i],flac_callbacks[i],flac_originals[i]);
        if(status==MH_OK) status=MH_QueueEnableHook(flac_targets[i]);
    }
    if(status==MH_OK) status=MH_QueueEnableHook(target);
    if(status==MH_OK) status=MH_QueueEnableHook(sniff_target);
    bool apply_attempted=false;
    if(status==MH_OK) {apply_attempted=true;status=MH_ApplyQueued();}
    if(status!=MH_OK) {
        char message[200];snprintf(message,sizeof(message),"audio hook installation failed: %s",MH_StatusToString(status));Log(message);
        if(apply_attempted) {
            // MH_ApplyQueued may have published some detours before reporting a
            // failure. A thread can be between the detour entry and our C++
            // callback counter, so keep every trampoline and callback resource
            // alive for the process lifetime after disabling the hooks.
            for(unsigned i=0;i<created_count;i++) MH_DisableHook(created_targets[i]);
            workers_running.store(false,std::memory_order_release);SetEvent(event);SetEvent(save_event);
            WaitForSingleObject(worker,5000);WaitForSingleObject(publisher,5000);close_threads();
            Log("audio hook activation failed; disabled hook state preserved for callback safety");
            audio_init.MarkUnsupported();
        } else if(rollback()){Log("audio hook rollback complete; retry scheduled");audio_init.Retry(now);}
        else {Log("audio hook rollback incomplete; callback state preserved and retries disabled");audio_init.MarkUnsupported();}
        return;
    }
    close_threads();
    audio_init.Activate();
    Log("native history active; dynamic Spotify targets; memory capture; embedded metadata/art; complete listens only");
}
