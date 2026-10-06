#define WIN32_LEAN_AND_MEAN
#include "audio_history.h"
#include "ogg_history_core.h"
#include "ogg_tags.h"
#include "flac_history_core.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "hook_rollback.h"
#include "metadata_bridge.h"
#include "classic_ui_backend.h"
#include "post_process.h"
#include "playback_quality.h"
#include "playback_speed.h"
#include "async_log.h"
#include "bounded_queue.h"
#include <atomic>
#include "library_layout.h"
#include "existing_quality.h"
#include "file_publication.h"
#include "media_session.h"
#include "spotify_hook_discovery.h"
#include "vendor/minhook/include/MinHook.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace {
using namespace history;
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
struct Slot { uintptr_t context; unsigned length; double time; unsigned kind,epoch; uint64_t input_length;
    uint32_t format; bool matched,capture; unsigned char bytes[65307]; };
constexpr unsigned capacity=128;
Slot* queue;
unsigned head=0,tail=0,count=0;
SRWLOCK queue_lock=SRWLOCK_INIT;
HANDLE event;
volatile LONG dropped=0,calls=0,pages=0;
std::atomic<bool> quality_enabled{false};
std::wstring output;
size_t memory_limit=500*1024*1024;
static double Now() { return double(GetTickCount64())/1000.0; }
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
    int32_t result=original(sync,page);
    InterlockedIncrement(&calls);
    const bool capture=OggEnabled();
    if(!capture && !quality_enabled.load(std::memory_order_relaxed)) return result;
    if(result<=0 || !page || page->header_length<27 || page->header_length>282 ||
       page->body_length<0 || page->body_length>65025 ||
       page->header_length+page->body_length!=result || result>65307) return result;
    InterlockedIncrement(&pages);
    AcquireSRWLockExclusive(&queue_lock);
    if(count==capacity) InterlockedIncrement(&dropped);
    else {
        Slot& s=queue[tail]; s.context=reinterpret_cast<uintptr_t>(sync);
        s.length=unsigned(result); s.time=Now(); s.kind=0; s.epoch=CaptureEpoch(); s.capture=capture;
        std::memcpy(s.bytes,page->header,page->header_length);
        if(page->body_length) std::memcpy(s.bytes+page->header_length,page->body,page->body_length);
        tail=(tail+1)%capacity; ++count;
    }
    ReleaseSRWLockExclusive(&queue_lock); SetEvent(event); return result;
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
static void FlacEvent(void* context,unsigned kind,const void* bytes=nullptr,size_t n=0,unsigned status=0) {
    const bool capture=FlacEnabled();
    if(!capture && !quality_enabled.load(std::memory_order_relaxed)) return;
    AcquireSRWLockExclusive(&queue_lock);
    do {
        unsigned amount=unsigned(std::min(n,size_t(65307)));
        if(count==capacity) { InterlockedIncrement(&dropped); break; }
        Slot& s=queue[tail]; s.context=reinterpret_cast<uintptr_t>(context); s.kind=kind;
        s.capture=capture; s.input_length=amount;
        s.length=(!capture && kind==3) ? std::min(amount,42u) : amount;
        s.time=Now(); s.format=status; s.epoch=CaptureEpoch();
        if(s.length) memcpy(s.bytes,bytes,s.length);
        tail=(tail+1)%capacity; ++count;
        n-=amount;
        if(n) bytes=static_cast<const unsigned char*>(bytes)+amount;
    } while(n);
    ReleaseSRWLockExclusive(&queue_lock); SetEvent(event);
}
static void* FlacInitHook(void* context,void* result) {
    auto callback=audio_callbacks.Enter();
    FlacEvent(context,2);
    return original_flac_init(context,result);
}
static int FlacReadHook(void* context,unsigned char* buffer,size_t* length) {
    auto callback=audio_callbacks.Enter();
    size_t requested=*length;
    int status=original_flac_read(context,buffer,length);
    if(status==0 && *length && *length<=requested) FlacEvent(context,3,buffer,*length);
    else if(status==1 && !*length) FlacEvent(context,6);
    else if(status!=0 || *length>requested) FlacEvent(context,5,nullptr,0,unsigned(status));
    return status;
}
static int FlacFrameHook(void* context,const void* frame,const void* pcm) {
    auto callback=audio_callbacks.Enter();
    int status=original_flac_frame(context,frame,pcm);
    if(status==0) FlacEvent(context,4,frame,32);
    else FlacEvent(context,5,nullptr,0,unsigned(status));
    return status;
}
static void FlacErrorHook(void* context,unsigned error) {
    auto callback=audio_callbacks.Enter();
    original_flac_error(context,error);
    FlacEvent(context,5,nullptr,0,error);
}
struct Capture {
    Stream stream;
    bool lossless=false,metadata=false;
    FlacInfo flac;
    FlacCoverage coverage;
    double born=0,finished=0;
    uint64_t bytes=0;
    CompressedBuffer data;
    double Duration() const { return lossless && flac.rate ? double(coverage.samples)/flac.rate : stream.Duration(); }
    Quality Encoding() const { return lossless ? Quality{Codec::Flac,flac.rate,flac.channels,flac.bits,0} : Quality{Codec::Vorbis,stream.rate,stream.channels,0,stream.bitrate}; }
};
struct Heard { Media media; double start=0,finish=0; };
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
enum class Publication { Saved,Skipped,Failed };
struct PublicationResult {
    Publication state=Publication::Failed;
    std::wstring path;
    std::string message;
};
static bool WriteSidecar(const std::wstring& path,const void* data,size_t size) {
    if(path.empty()||!data||!size||size>MAXDWORD)return false;
    auto parent=path.substr(0,path.find_last_of(L'\\'));
    if(!Directories(parent))return false;
    HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
                            CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_EXISTS;
    DWORD written=0;bool ok=WriteFile(file,data,DWORD(size),&written,nullptr)&&written==size;
    CloseHandle(file);if(!ok)DeleteFileW(WindowsPath(path).c_str());return ok;
}
static PublicationResult Publish(Capture& c,const Heard& heard,unsigned expected_epoch) {
    SYSTEMTIME t; GetSystemTime(&t); wchar_t stamp[64];
    swprintf(stamp,64,L"%04u-%02u-%02uT%02u:%02u:%02uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    auto catalog=CatalogFor(heard.media);
    auto settings=GetSettings();
    if(settings.capture_epoch!=expected_epoch || !settings.downloads || !(c.lossless ? settings.flac : settings.ogg)) return {Publication::Skipped,{},"settings changed"};
    const std::wstring& active_template=(catalog.kind==MediaKind::Podcast && !settings.podcast_template.empty())
        ?settings.podcast_template:settings.path_template;
    std::wstring destination=OutputPath(settings.root,catalog,c.lossless ? L".flac" : L".ogg",
        settings.music_folder,active_template,settings.normalize_artist_separators,settings.invalid_char_repl);
    std::wstring flac=OutputPath(settings.root,catalog,L".flac",
        settings.music_folder,active_template,settings.normalize_artist_separators,settings.invalid_char_repl);
    {
        char line[1400];snprintf(line,sizeof(line),
            "destination=%s native=%s preset=%s output_ext=%s keep_native=%d embed_cover=%d embed_lyrics=%d save_cover=%d save_lyrics=%d",
            Utf8(destination).c_str(),c.lossless?"flac":"ogg",Utf8(settings.output_preset).c_str(),
            Utf8(settings.output_ext).c_str(),settings.keep_native_original,settings.embed_cover_art,
            settings.embed_lyrics,settings.save_cover_art,settings.save_lyrics);
        LogActivity("publish_path",line);QueueDiagnostic(line);
    }
    Quality quality=c.Encoding(),lossless;
    if(!c.lossless && ReadQuality(flac,lossless) && lossless.codec==Codec::Flac) {
        Log(("SKIP existing lossless file "+Utf8(flac)).c_str()); return {Publication::Skipped,flac,"Already downloaded"};
    }
    SaveDecision decision=DecideSave(destination,quality);
    if(decision==SaveDecision::Skip) {
        Log(("SKIP existing equal, higher or unverified quality "+Utf8(destination)).c_str()); return {Publication::Skipped,destination,"Already downloaded"};
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
    if(latest.generation!=settings.generation || !latest.downloads || !(c.lossless ? latest.flac : latest.ogg)) return {Publication::Skipped,{},"settings changed"};
    if(!Directories(destination.substr(0,destination.find_last_of(L'\\')))) return {Publication::Failed,{},"output directory could not be created"};
    // First and only audio disk write: the complete, tagged file. No scratch
    // paths, sidecars, per-song directories, decoder or encoder are involved.
    auto publication=PublishBytes(destination,quality,tagged.data(),tagged.size());
    if(publication==FilePublication::Skipped) return {Publication::Skipped,destination,"Already downloaded"};
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

    auto post=PostProcessPublishedFile(destination,settings,heard.media.cover,heard.media.cover_extension);
    if(post.state==PostProcessResult::State::Failed) {
        auto detail="post-processing failed: "+post.error+" native="+Utf8(destination)+" output="+Utf8(post.path);
        LogActivity("failed",detail);QueueDiagnostic(detail.c_str());
        return {Publication::Failed,post.path,post.error};
    }
    {
        auto final_path=post.path.empty()?destination:post.path;
        auto detail="publication complete native="+Utf8(destination)+" final="+Utf8(final_path);
        LogActivity("publish_done",detail);QueueDiagnostic(detail.c_str());
    }
    return {Publication::Saved,post.path.empty()?destination:post.path,{}};

}
struct SaveJob {std::unique_ptr<Capture> capture;Heard heard;unsigned epoch=0;size_t reserved=0;};
static BoundedQueue<SaveJob,2> saves;
static HANDLE save_event=nullptr;
static std::atomic<size_t> publishing_reserved{0};
static std::atomic<unsigned> saved_count{0};
static std::atomic<bool> workers_running{false};
static hooks::InitController audio_init;
static DWORD WINAPI SaveWorker(LPVOID) {
    while(workers_running.load(std::memory_order_acquire)) {
        SaveJob job;
        if(!saves.Pop(job)){WaitForSingleObject(save_event,200);continue;}
        std::string identity=Utf8(job.heard.media.artist)+" - "+Utf8(job.heard.media.title);
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
            auto result=Publish(*job.capture,job.heard,job.epoch);
            if(result.state==Publication::Saved || (result.state==Publication::Skipped && !result.path.empty())) {
                auto settings=GetSettings();
                if(result.state==Publication::Saved && settings.capture_epoch==job.epoch) ++saved_count;
                SetClassicTrackStatus(job.heard.media,"DONE",result.message,result.path);
                LogActivity("finished",identity+(result.state==Publication::Skipped?" (already downloaded)":""));
            } else if(result.state==Publication::Failed) {
                SetClassicTrackStatus(job.heard.media,"ERROR",result.message.empty()?"Publication failed":result.message,result.path);
                LogActivity("failed",identity+" ("+(result.message.empty()?std::string("publication failed"):result.message)+")");
            } else {
                SetClassicTrackStatus(job.heard.media,"ERROR","Canceled: settings changed");
                LogActivity("failed",identity+" (settings changed)");
            }
        } catch(...) {
            SetClassicTrackStatus(job.heard.media,"ERROR","Publication exception");
            LogActivity("failed",identity+" (publication exception)");
        }
        job.capture.reset();publishing_reserved.fetch_sub(job.reserved);
    }
    return 0;
}
static DWORD WINAPI Worker(LPVOID) {
    LONG overflow=0;
    std::map<uintptr_t,std::unique_ptr<Capture>> active;
    std::vector<std::unique_ptr<Capture>> ready;
    std::vector<Heard> heard;
    Listen listen; MediaReader reader; Media current; PlaybackQualityTracker quality; std::string client_quality;
    double next_media=0,next_log=0;
    unsigned generation=~0u,epoch=~0u; bool enabled=false,current_ignored=false;
    try {
        while(workers_running.load(std::memory_order_acquire)) {
            double now=Now();
            auto preferences=GetSettings();
            const double playback_rate=PlaybackSpeedEffective();
            if(preferences.generation!=generation) {
                bool initial=generation==~0u;
                generation=preferences.generation;
                output=preferences.root; memory_limit=size_t(preferences.max_buffered_mib)*1024*1024;
                bool capture=preferences.downloads && (preferences.ogg || preferences.flac);
                if(capture!=enabled || initial || preferences.capture_epoch!=epoch) {
                    active.clear(); ready.clear(); heard.clear(); listen={}; current={}; next_media=0;
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
            }
            if(!enabled && !quality_enabled.load(std::memory_order_relaxed)) {
                PublishPlaybackQuality({});
                Slot discard; while(Pop(discard)) {}
                WaitForSingleObject(event,200); continue;
            }
            if(dropped!=overflow) {
                overflow=dropped;
                if(!current.title.empty())SetClassicTrackStatus(current,"ERROR","Canceled: capture queue overflow");
                active.clear(); ready.clear(); heard.clear(); listen.eligible=false; quality.ClearStreams();
                Log("capture queue overflow; streams and listening coverage invalidated");
            }
            Slot s;
            while(Pop(s)) {
                if(s.kind==1) {
                    char prefix[33]={}; for(unsigned i=0;i<s.length;i++) snprintf(prefix+2*i,3,"%02x",s.bytes[i]);
                    char line[180]; snprintf(line,sizeof(line),"FORMAT kind=%u matched=%d input_bytes=%llu prefix=%s",
                      s.format,s.matched,static_cast<unsigned long long>(s.input_length),prefix); Log(line); continue;
                }
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
                    auto found=active.find(s.context);
                    if(s.kind==2) {
                        auto c=std::make_unique<Capture>(); c->lossless=true; c->born=s.time;
                        active[s.context]=std::move(c); Log("FLAC decoder initialized; capture begins at compressed offset zero"); continue;
                    }
                    if(found==active.end() || !found->second->lossless) continue;
                    Capture& c=*found->second;
                    if(s.kind==5) { active.erase(found); Log("FLAC decoder error; stream discarded"); continue; }
                    if(s.kind==3) {
                        size_t reserved=publishing_reserved.load(); for(const auto& item:active) reserved+=item.second->data.capacity();
                        for(const auto& item:ready) reserved+=item->data.capacity();
                        size_t available=reserved<memory_limit ? memory_limit-reserved : 0;
                        if(!c.data.Append(s.bytes,s.length,available)) { active.erase(found); Log("FLAC capture memory limit; stream discarded"); continue; }
                        c.bytes+=s.length;
                        if(!c.metadata) {
                            size_t audio=0; auto parsed=ParseFlacMetadata(c.data,c.flac,audio);
                            if(parsed==FlacParse::Invalid) { active.erase(found); Log("FLAC metadata invalid; stream discarded"); continue; }
                            if(parsed==FlacParse::Valid) {
                                c.metadata=true; char line[180]; snprintf(line,sizeof(line),"FLAC STREAMINFO rate=%u channels=%u bits=%u total=%llu metadata_bytes=%zu",c.flac.rate,c.flac.channels,c.flac.bits,static_cast<unsigned long long>(c.flac.total_samples),audio); Log(line);
                            }
                        }
                        continue;
                    }
                    if(s.kind==4) {
                        uint32_t frame[6]; uint64_t number; memcpy(frame,s.bytes,24); memcpy(&number,s.bytes+24,8);
                        if(frame[5]==0) number=uint32_t(number);
                        if(!c.metadata || !c.coverage.Frame(c.flac,frame[0],frame[1],frame[2],frame[4],frame[5],number)) {
                            active.erase(found); Log("FLAC decoded-frame gap or format mismatch; stream discarded"); continue;
                        }
                        if(!c.coverage.Complete(c.flac)) continue;
                    } else if(s.kind!=6 || !c.coverage.Complete(c.flac)) { active.erase(found); Log("FLAC truncated before total samples; discarded"); continue; }
                    c.finished=s.time;
                    char line[180]; snprintf(line,sizeof(line),"FLAC COMPLETE frames=%u samples=%llu bytes=%llu duration=%.6f",c.coverage.frames,static_cast<unsigned long long>(c.coverage.samples),static_cast<unsigned long long>(c.bytes),c.Duration()); Log(line);
                    ready.push_back(std::move(found->second)); active.erase(found); continue;
                }
                Page page; if(!ParsePage(s.bytes,s.length,page)) {
                    active.erase(s.context); Log("invalid Ogg page rejected"); continue;
                }

                auto found=active.find(s.context);
                if(page.vorbis_start) {
                    // A decoder may replay its identification/BOS page after
                    // buffering. Preserve an in-progress capture when this is
                    // the same logical stream and an already-consumed sequence.
                    if(found!=active.end() && !found->second->lossless &&
                       found->second->stream.active &&
                       page.serial==found->second->stream.serial &&
                       page.sequence<found->second->stream.next) {
                        if(DebugLoggingEnabled())Log("Ogg replayed BOS page ignored");
                        continue;
                    }
                    auto c=std::make_unique<Capture>(); c->born=s.time;
                    active[s.context]=std::move(c);
                    found=active.find(s.context);
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
                size_t reserved=publishing_reserved.load();
                for(const auto& item:active) reserved+=item.second->data.capacity();
                for(const auto& item:ready) reserved+=item->data.capacity();
                size_t available=reserved<memory_limit ? memory_limit-reserved : 0;
                if(!c.data.Append(s.bytes,s.length,available)) {
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
            if(!enabled) {
                if(now>=next_media) {
                    Media media;
                    if(reader.Read(media,false,playback_rate)) {quality.Media(media.Key(),media.title,media.position,media.duration,Now(),media.playing);client_quality=ReadClientPlaybackQuality(media);}
                    else {quality.Media({},L"",0,0,Now());client_quality.clear();}
                    next_media=Now()+0.5;
                }
                PublishPlaybackQuality(quality.Snapshot(client_quality));
                WaitForSingleObject(event,50);continue;
            }
            if(now>=next_media) {
                Media media;
                if(reader.Read(media,true,playback_rate)) {
                    quality.Media(media.Key(),media.title,media.position,media.duration,Now(),media.playing);
                    client_quality=ReadClientPlaybackQuality(media);
                    std::string previous=listen.identity; double previous_start=listen.start_time;
                    bool was_eligible=listen.eligible;
                    double prior_position=listen.last_position,prior_duration=listen.duration,prior_time=listen.last_time;
                    if(DebugLoggingEnabled()) {
                        char line[1200];snprintf(line,sizeof(line),
                            "media_sample title=%s artist=%s key=%s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f playing=%d prior=%.3f prior_duration=%.3f wall_delta=%.3f speed_config=%.3f speed_hook=%d rate=%.3f eligible=%d active=%zu ready=%zu",
                            Utf8(media.title).c_str(),Utf8(media.artist).c_str(),media.Key().c_str(),media.position,media.raw_position,
                            media.duration,media.timeline_age,media.playing,prior_position,prior_duration,Now()-prior_time,
                            preferences.playback_speed,PlaybackSpeedSupported(),playback_rate,listen.eligible,active.size(),ready.size());
                        Log(line);
                    }
                    // The RC28 decoder-speed backend deliberately consumes the full
                    // compressed stream while returning fewer PCM samples to Spotify.
                    // SMTC's public timeline does not advance at that accelerated decode
                    // rate, so a complete native stream is the authoritative completion
                    // signal above 1x.
                    //
                    // Match against 'current' (the media identity that was eligible before
                    // this sample), not only the newly-read media. This also catches a very
                    // fast track whose Ogg/FLAC EOS and title transition both happen between
                    // two 500 ms media polls.
                    bool accelerated_capture_complete=false;
                    size_t accelerated_matches=0;
                    if(playback_rate>1.0001 && was_eligible && !previous.empty() &&
                       current.Key()==previous && current.duration>0) {
                        for(const auto& candidate:ready) {
                            if(std::fabs(candidate->Duration()-current.duration)<=1.0 &&
                               candidate->born>=previous_start-20 &&
                               candidate->born<=previous_start+3 &&
                               candidate->finished>=previous_start) {
                                ++accelerated_matches;
                            }
                        }
                        accelerated_capture_complete=accelerated_matches==1;
                    }

                    // Always observe the new media sample so a title transition can arm the
                    // next track immediately. At accelerated rates Listen deliberately
                    // ignores SMTC position/clock discontinuities; capture integrity/EOS is
                    // what proves completion or rejects a seek.
                    std::string done=listen.Observe(media.Key(),media.position,media.duration,
                                                    media.playing,Now(),playback_rate);
                    if(accelerated_capture_complete) {
                        done=previous;
                        // If Spotify still exposes the old title, prevent a second completion
                        // for the same stream. If the title already changed, Observe has
                        // already armed the new identity and it must remain eligible.
                        if(listen.identity==previous) {
                            listen.eligible=false;
                            listen.pending_start=false;
                        }
                        listen.transient=false;
                        listen.reject=ListenReject::None;

                        char line[1024];snprintf(line,sizeof(line),
                            "%s - %s capture_eos=1 media_duration=%.3f rate=%.3f ready=%zu matches=%zu key_changed=%d",
                            Utf8(current.artist).c_str(),Utf8(current.title).c_str(),
                            current.duration,playback_rate,ready.size(),accelerated_matches,
                            previous!=listen.identity);
                        LogActivity("accelerated_complete",line);QueueDiagnostic(line);
                        RequestAcceleratedAdvance(
                            previous+"#"+std::to_string(static_cast<unsigned long long>(previous_start*1000.0)));
                    }
                    if(was_eligible && !listen.eligible && done.empty()) {
                        const auto identity=Utf8(current.artist)+" - "+Utf8(current.title);
                        char line[1400]; snprintf(line,sizeof(line),
                            "%s (incomplete listen reason=%s prior=%.3f/%.3f new=%.3f raw=%.3f/%.3f timeline_age=%.3f delta=%.3f expected=%.3f tolerance=%.3f rate=%.3f playing_before=%d playing_now=%d key_changed=%d)",
                            identity.c_str(),ListenRejectName(listen.reject),prior_position,prior_duration,
                            media.position,media.raw_position,media.duration,media.timeline_age,Now()-prior_time,
                            listen.last_expected,listen.last_tolerance,listen.last_rate,listen.playing,media.playing,
                            previous!=listen.identity);
                        LogActivity("failed",line);QueueDiagnostic(line);
                    }
                    if(!done.empty()) {
                        if(current_ignored) {
                            SetClassicTrackStatus(current,"IGNORED","Ignored");
                            LogActivity("finished",Utf8(current.artist)+" - "+Utf8(current.title)+" (ignored)");
                        } else {
                            heard.push_back({current,previous_start,Now()});
                            char line[1024];snprintf(line,sizeof(line),
                                "%s - %s pos=%.3f raw=%.3f duration=%.3f timeline_age=%.3f elapsed_wall=%.3f rate=%.3f ready=%zu active=%zu completion=%s",
                                Utf8(current.artist).c_str(),Utf8(current.title).c_str(),prior_position,current.raw_position,
                                current.duration,current.timeline_age,Now()-previous_start,playback_rate,ready.size(),active.size(),
                                accelerated_capture_complete?"capture_eos":"timeline");
                            LogActivity("listen_complete",line);QueueDiagnostic(line);
                        }
                    }
                    if(previous!=listen.identity && listen.eligible) {
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
                    if(previous!=listen.identity) {
                        Log(("TRACK "+Utf8(media.artist)+" - "+Utf8(media.title)).c_str());
                        {
                            auto prior=[previous_start,start=listen.start_time](const auto& c) {
                                return c->born>=previous_start-20 && c->born<=previous_start+3 && c->born<start-3;
                            };
                            // A fast Next may create the new decoder inside the
                            // previous track's three-second start window. Keep
                            // that candidate; duration association still rejects
                            // ambiguous or skipped streams at publication.
                            if(done.empty())
                            ready.erase(std::remove_if(ready.begin(),ready.end(),prior),ready.end());
                            for(auto it=active.begin();it!=active.end();) {
                                if(prior(it->second)) it=active.erase(it); else ++it;
                            }
                        }
                    }
                    if(listen.transient) Log("timeline reset at natural end; waiting for matching title");
                    else current=std::move(media);
                } else {
                    const bool was_eligible=listen.eligible;
                    listen.eligible=false; quality.Media({},L"",0,0,Now());
                    if(was_eligible) {
                        char line[768];snprintf(line,sizeof(line),
                            "%s - %s (media snapshot unavailable rate=%.3f active=%zu ready=%zu epoch=%u)",
                            Utf8(current.artist).c_str(),Utf8(current.title).c_str(),playback_rate,active.size(),ready.size(),epoch);
                        LogActivity("failed",line);QueueDiagnostic(line);
                    } else QueueDiagnostic("Spotify media snapshot unavailable while no eligible listen was active");
                }
                next_media=Now()+0.5;
            }
            PublishPlaybackQuality(quality.Snapshot(client_quality));
            // A media-session read may block while the menu changes settings.
            // Apply that generation before considering any publication.
            if(GetSettings().generation!=generation) continue;
            for(auto h=heard.begin();h!=heard.end();) {
                size_t match=ready.size(),matches=0;
                for(size_t i=0;i<ready.size();i++) {
                    Capture& c=*ready[i];
                    if(std::fabs(c.Duration()-h->media.duration)<=1.0 &&
                       c.born>=h->start-20 && c.born<=h->start+3) { match=i; ++matches; }
                }
                if(matches==1) {
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
                } else if(matches>1 || now-h->finish>10) {
                    SetClassicTrackStatus(h->media,"ERROR",matches>1?"Ambiguous audio stream association":"Completed audio stream was not found");
                    std::string candidates;
                    for(size_t i=0;i<ready.size() && i<6;i++) {
                        char item[160];snprintf(item,sizeof(item),"%s%.3fs@%+.3fs",
                            i?",":"",ready[i]->Duration(),ready[i]->born-h->start);
                        candidates+=item;
                    }
                    char line[1400];snprintf(line,sizeof(line),
                        "%s - %s (stream association failed matches=%zu ready=%zu media_duration=%.3f listen_start=%.3f listen_finish=%.3f wait=%.3f candidates=[%s])",
                        Utf8(h->media.artist).c_str(),Utf8(h->media.title).c_str(),matches,ready.size(),
                        h->media.duration,h->start,h->finish,now-h->finish,candidates.c_str());
                    LogActivity("failed",line);QueueDiagnostic(line);
                    h=heard.erase(h);
                } else ++h;
            }
            ready.erase(std::remove_if(ready.begin(),ready.end(),[now](const auto& c){return now-c->finished>600;}),ready.end());
            for(auto it=active.begin();it!=active.end();) {
                if(now-it->second->born>21600) it=active.erase(it); else ++it;
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
