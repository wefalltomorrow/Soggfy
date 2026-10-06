#define WIN32_LEAN_AND_MEAN
#include "metadata_bridge.h"
#include "cef_identity.h"
#include "hook_init_state.h"
#include "hook_installation.h"
#include "hook_rollback.h"
#include "rich_metadata.h"
#include "playback_quality.h"
#include "cached_metadata.h"
#include "history_settings.h"
#include "playback_speed.h"
#include "classic_ui_backend.h"
#include "vendor/minhook/include/MinHook.h"
#include "../build/metadata_script.h"
#include "../build/soggfy_ui_script.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <new>
namespace history { namespace {
struct Base {size_t size;void(*add)(Base*);int(*release)(Base*);int(*one)(Base*);int(*any)(Base*);};
struct String {wchar_t* str;size_t length;void(*dtor)(wchar_t*);};
template<size_t N> struct Object {Base base;void* methods[N];};
using Client=Object<19>;using Display=Object<13>;using Load=Object<4>;using Browser=Object<21>;using Frame=Object<26>;
static void Release(void* p) {if(p)static_cast<Base*>(p)->release(static_cast<Base*>(p));}
static int HexDigit(wchar_t c) {
 if(c>=L'0'&&c<=L'9')return int(c-L'0');
 if(c>=L'a'&&c<=L'f')return int(c-L'a')+10;
 if(c>=L'A'&&c<=L'F')return int(c-L'A')+10;
 return -1;
}
static std::string PercentDecode(const wchar_t* text,size_t length) {
 std::string out;out.reserve(length);
 for(size_t i=0;i<length;++i) {
  wchar_t c=text[i];
  if(c==L'%'&&i+2<length) {
   int hi=HexDigit(text[i+1]),lo=HexDigit(text[i+2]);
   if(hi>=0&&lo>=0){out.push_back(char((hi<<4)|lo));i+=2;continue;}
  }
  if(c>127)return {};
  out.push_back(char(c));
 }
 return out;
}
static std::string PercentEncode(const std::string& value) {
 static const char hex[]="0123456789ABCDEF";
 std::string out;out.reserve(value.size()+16);
 for(unsigned char c:value) {
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out.push_back(char(c));
  else {out.push_back('%');out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}
 }
 return out;
}
static std::wstring WideUtf8(const std::string& value) {
 if(value.empty())return {};
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
 if(count<=0)return {};
 std::wstring out(size_t(count),L'\0');
 if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),count)!=count)return {};
 return out;
}
// CEF's C++ wrapper has private identity fields before the public C structure.
// Replacing that structure breaks GetClient() round trips (CEF UnwrapDerived).
// Intercept callback code instead, preserving every object and its references.
struct CallbackHook {
 SRWLOCK lock=SRWLOCK_INIT;
 std::atomic<void*> target{nullptr},original{nullptr};
 std::atomic<bool> enabled{false};
 bool Install(void* address,void* callback,const char* name) {
  if(!address)return false;
  if(enabled.load(std::memory_order_acquire))return target.load()==address;
  if(!TryAcquireSRWLockExclusive(&lock))return false;
  auto installed=target.load();MH_STATUS status=MH_OK;
  if(installed&&installed!=address){ReleaseSRWLockExclusive(&lock);return false;}
  if(!installed){
   void* trampoline=nullptr;status=MH_CreateHook(address,callback,&trampoline);
   if(status==MH_OK){original.store(trampoline,std::memory_order_release);target.store(address,std::memory_order_release);}
  }
  // MinHook suspends other threads. Never hold a callback lock while enabling.
  ReleaseSRWLockExclusive(&lock);
  if(status==MH_OK)status=MH_EnableHook(address);
  if(status==MH_OK||status==MH_ERROR_ENABLED){enabled.store(true,std::memory_order_release);return true;}
  char line[180];snprintf(line,sizeof(line),"metadata callback %s unavailable: %s",name,MH_StatusToString(status));HistoryLog(line);
  return false;
 }
 template<class F> F Original()const{return reinterpret_cast<F>(original.load(std::memory_order_acquire));}
};
static CallbackHook display_getter,load_getter,console_callback,loading_callback,load_end_callback;
struct Task {Base base;void(*execute)(Task*);};
struct PollTask {Task task;std::atomic<unsigned> refs{1};};
static Frame* polling_frame=nullptr;static SRWLOCK frame_lock=SRWLOCK_INIT;
static std::wstring UiConfigCode() {
 auto s=GetSettings();
 auto bit=[](bool value){return value?"1":"0";};
 std::string payload;
 auto add=[&](const char* key,const std::string& value){
  if(!payload.empty())payload+='&';
  payload+=key;
  payload+='=';
  payload+=PercentEncode(value);
 };
 add("downloads",bit(s.downloads));
 add("ogg",bit(s.ogg));
 add("flac",bit(s.flac));
 add("metadata",bit(s.metadata));
 add("log",bit(s.log));
 add("debug",bit(s.debug_log));
 add("normalize",bit(s.normalize_artist_separators));
 add("skipDownloaded",bit(s.skip_downloaded_tracks));
 add("skipIgnored",bit(s.skip_ignored_tracks));
 add("embedCover",bit(s.embed_cover_art));
 add("saveCover",bit(s.save_cover_art));
 add("embedLyrics",bit(s.embed_lyrics));
 add("saveLyrics",bit(s.save_lyrics));
 add("saveCanvas",bit(s.save_canvas));
 add("blockTelemetry",bit(s.block_telemetry));
 add("liftQueue",bit(s.lift_add_to_queue));
 add("keepNative",bit(s.keep_native_original));
 const bool speed_supported=PlaybackSpeedSupported();
 add("playbackSpeed",std::to_string(speed_supported?s.playback_speed:1.0));
 add("speedSupported",bit(speed_supported));
 add("speedImmediate",bit(speed_supported&&PlaybackSpeedImmediate()));
 const auto quality_snapshot=ReadPlaybackQuality();
 const auto quality=PlaybackQualityLabels(quality_snapshot);
 auto quality_value=[](const std::wstring& row) {
  const auto at=row.find(L": ");
  return at==std::wstring::npos?row:row.substr(at+2);
 };
 add("qualitySong",Utf8(quality_value(quality[0])));
 add("qualityLevel",Utf8(quality_value(quality[1])));
 add("qualityFormat",Utf8(quality_value(quality[2])));
 add("qualitySample",Utf8(quality_value(quality[3])));
 add("qualityAssociation",Utf8(PlaybackAssociationLabel(quality_snapshot)));
 add("root",Utf8(s.root));
 add("template",Utf8(s.path_template));
 add("podcastTemplate",Utf8(s.podcast_template));
 add("canvasTemplate",Utf8(s.canvas_template));
 add("invalidChars",Utf8(s.invalid_char_repl));
 add("outputPreset",Utf8(s.output_preset));
 add("outputExt",Utf8(s.output_ext));
 add("outputArgs",Utf8(s.output_args));
 add("ffmpegPath",Utf8(s.ffmpeg_path));
 std::wstring wide(payload.begin(),payload.end());
 std::wstring code=L"window.__soggfyNativeConfig=\""+wide+
   L"\";window.__soggfyMetadataEnabled="+(s.metadata?L"true":L"false")+
   L";if(window.__soggfyApplyConfig)window.__soggfyApplyConfig(window.__soggfyNativeConfig);";
 return code;
}
static void ExecuteFrameCode(Frame* frame,const std::wstring& code,const wchar_t* source_name) {
 if(!frame||code.empty()||!reinterpret_cast<int(*)(Frame*)>(frame->methods[0])(frame))return;
 String script={const_cast<wchar_t*>(code.c_str()),code.size(),nullptr};
 String source={const_cast<wchar_t*>(source_name),wcslen(source_name),nullptr};
 reinterpret_cast<void(*)(Frame*,const String*,const String*,int)>(frame->methods[14])(frame,&script,&source,1);
}
static void SyncClassicUi(Frame* preferred=nullptr) {
 Frame* f=preferred;
 if(f)f->base.add(&f->base);
 else {
  AcquireSRWLockShared(&frame_lock);f=polling_frame;if(f)f->base.add(&f->base);ReleaseSRWLockShared(&frame_lock);
 }
 if(f){ExecuteFrameCode(f,UiConfigCode(),L"soggfy-config.js");Release(f);}
}
static void FlushClassicStatusResponse(Frame* frame);
static void FlushAcceleratedAdvance(Frame* frame);
static std::atomic<bool> poll_pending{false};static int(*post_task)(int,Task*)=nullptr;
static void PollAdd(Base* b){++reinterpret_cast<PollTask*>(b)->refs;}
static int PollDrop(Base* b){auto t=reinterpret_cast<PollTask*>(b);if(--t->refs)return 0;delete t;return 1;}
static int PollOne(Base* b){return reinterpret_cast<PollTask*>(b)->refs==1;}
static int PollAny(Base* b){return reinterpret_cast<PollTask*>(b)->refs>0;}
static void PollExecute(Task*){
 Frame* f=nullptr;AcquireSRWLockShared(&frame_lock);f=polling_frame;if(f)f->base.add(&f->base);ReleaseSRWLockShared(&frame_lock);
 if(f&&reinterpret_cast<int(*)(Frame*)>(f->methods[0])(f)){
  const wchar_t code_text[]=L"if(window.__floggfyPoll)window.__floggfyPoll();";
  String code={const_cast<wchar_t*>(code_text),wcslen(code_text),nullptr};const wchar_t name[]=L"floggfy-metadata.js";String source={const_cast<wchar_t*>(name),wcslen(name),nullptr};
  reinterpret_cast<void(*)(Frame*,const String*,const String*,int)>(f->methods[14])(f,&code,&source,1);
  FlushClassicStatusResponse(f);
  FlushAcceleratedAdvance(f);
 }
 Release(f);poll_pending=false;
}
static void SchedulePoll(){
 if(!post_task||poll_pending.exchange(true))return;
 auto t=new(std::nothrow) PollTask;if(!t){poll_pending=false;return;}
 t->task.base={sizeof(Task),PollAdd,PollDrop,PollOne,PollAny};t->task.execute=PollExecute;
 if(!post_task(0,&t->task))poll_pending=false; // The public CEF function consumes the task reference.
}
static MetadataCache cache;static SRWLOCK cache_lock=SRWLOCK_INIT,message_lock=SRWLOCK_INIT;
static hooks::InitController metadata_init;
static hooks::CallbackCounter metadata_callbacks;
static std::atomic<bool> metadata_running{false},metadata_polling{false};
struct Message { ULONGLONG time=0; char data[131073];size_t length=0;};
static std::array<Message,4> messages;static size_t head=0,tail=0,count=0;static HANDLE message_event=nullptr;
static SRWLOCK ui_status_lock=SRWLOCK_INIT;
static std::string ui_status_request,ui_status_response;
static SRWLOCK accelerated_advance_lock=SRWLOCK_INIT;
static std::string accelerated_advance_token;

static std::vector<ClassicTrackQuery> ParseClassicStatusBatch(const std::string& data) {
 std::vector<ClassicTrackQuery> out;
 size_t start=0;
 while(start<=data.size() && out.size()<128) {
  size_t end=data.find(char(0x1e),start);if(end==std::string::npos)end=data.size();
  std::array<std::string,5> fields{};size_t f=0,pos=start;
  while(f<fields.size()) {
   size_t sep=data.find(char(0x1f),pos);
   if(sep==std::string::npos||sep>end)sep=end;
   fields[f++]=data.substr(pos,sep-pos);
   if(sep==end)break;
   pos=sep+1;
  }
  if(f>=4 && !fields[0].empty()) {
   ClassicTrackQuery q;
   q.uri=fields[0];q.title=WideUtf8(fields[1]);q.artist=WideUtf8(fields[2]);q.album=WideUtf8(fields[3]);
   if(f>=5)q.all_artists=WideUtf8(fields[4]);
   if(!q.title.empty())out.push_back(std::move(q));
  }
  if(end==data.size())break;
  start=end+1;
 }
 return out;
}
static std::string SerializeClassicStatuses(const std::vector<ClassicTrackResult>& results) {
 std::string out;
 for(const auto& r:results) {
  if(!out.empty())out.push_back(char(0x1e));
  out+=r.uri;out.push_back(char(0x1f));out+=r.status;out.push_back(char(0x1f));
  out+=Utf8(r.path);out.push_back(char(0x1f));out+=r.message;
 }
 return out;
}
static void QueueClassicStatusRequest(const std::string& request) {
 AcquireSRWLockExclusive(&ui_status_lock);ui_status_request=request;ReleaseSRWLockExclusive(&ui_status_lock);
 if(message_event)SetEvent(message_event);
}
static void ProcessClassicStatusRequest() {
 std::string request;
 AcquireSRWLockExclusive(&ui_status_lock);request.swap(ui_status_request);ReleaseSRWLockExclusive(&ui_status_lock);
 if(request.empty())return;
 auto result=SerializeClassicStatuses(QueryClassicTrackStatuses(ParseClassicStatusBatch(request)));
 AcquireSRWLockExclusive(&ui_status_lock);ui_status_response=std::move(result);ReleaseSRWLockExclusive(&ui_status_lock);
 SchedulePoll();
}
static void FlushClassicStatusResponse(Frame* frame) {
 std::string response;
 AcquireSRWLockExclusive(&ui_status_lock);response.swap(ui_status_response);ReleaseSRWLockExclusive(&ui_status_lock);
 if(response.empty())return;
 auto encoded=PercentEncode(response);
 std::wstring wide(encoded.begin(),encoded.end());
 std::wstring code=L"if(window.__soggfyReceiveStatuses)window.__soggfyReceiveStatuses(decodeURIComponent(\""+wide+L"\"));";
 ExecuteFrameCode(frame,code,L"soggfy-status.js");
}
static void FlushAcceleratedAdvance(Frame* frame) {
 std::string token;
 AcquireSRWLockExclusive(&accelerated_advance_lock);token.swap(accelerated_advance_token);ReleaseSRWLockExclusive(&accelerated_advance_lock);
 if(token.empty())return;
 auto encoded=PercentEncode(token);
 std::wstring wide(encoded.begin(),encoded.end());
 std::wstring code=L"if(window.__soggfyAcceleratedComplete)window.__soggfyAcceleratedComplete(decodeURIComponent(\""+wide+L"\"));";
 ExecuteFrameCode(frame,code,L"soggfy-accelerated-transport.js");
}
static bool Enqueue(const String* value) {
 constexpr wchar_t prefix[]=L"FLOGGFY_METADATA_V1:";constexpr size_t n=sizeof(prefix)/sizeof(*prefix)-1;
 if(!value||value->length<=n||value->length>n+131072||wmemcmp(value->str,prefix,n))return false;
 if(!GetSettings().metadata)return true;
 if(TryAcquireSRWLockExclusive(&message_lock)) {
  if(count<messages.size()) {
   auto& m=messages[tail];m.time=GetTickCount64();m.length=value->length-n;bool valid=true;
   for(size_t i=0;i<m.length;i++){wchar_t c=value->str[n+i];if(c>127){valid=false;break;}m.data[i]=char(c);}
   if(valid){tail=(tail+1)%messages.size();++count;SetEvent(message_event);}
  } ReleaseSRWLockExclusive(&message_lock);
 } return true;
}
static DWORD WINAPI MetadataWorker(LPVOID) {
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
 Message m;
 while(metadata_running.load(std::memory_order_acquire)){
  ProcessClassicStatusRequest();
  if(metadata_polling.load(std::memory_order_acquire))SchedulePoll();
  bool got=false;AcquireSRWLockExclusive(&message_lock);if(count){m=messages[head];head=(head+1)%messages.size();--count;got=true;}ReleaseSRWLockExclusive(&message_lock);
  if(!got){WaitForSingleObject(message_event,1000);continue;}
  RichMetadata metadata;std::string error;if(!ParseRichMetadata(std::string(m.data,m.length),metadata,error))continue;
  metadata.quality_time=m.time;
  try {EnrichStoredMetadata(metadata);}catch(...){HistoryLog("cached metadata unavailable; ordinary tags retained");}
  AcquireSRWLockExclusive(&cache_lock);cache.Put(metadata);ReleaseSRWLockExclusive(&cache_lock);
  HistoryLog(("metadata cached fields="+std::to_string(metadata.fields.size())+" date="+(metadata.fields.count("DATE")?metadata.fields.at("DATE"):std::string{})+" lyrics_bytes="+std::to_string((metadata.fields.count("LYRICS")?metadata.fields.at("LYRICS").size():0))).c_str());
 }
 return 0;
}
static bool CurrentPlayback(const String* text) {
 constexpr wchar_t prefix[]=L"FLOGGFY_PLAYBACK_V1:";
 constexpr size_t n=sizeof(prefix)/sizeof(*prefix)-1;
 if(!text || !text->str || text->length<n || wmemcmp(text->str,prefix,n))return false;
 // Bounded cached-state message only: no file reads, enrichment or API calls.
 try {
  PlaybackQualitySnapshot snapshot;
  if(text->length<=n+4096) {
   std::string payload;payload.reserve(text->length-n);
   for(size_t i=n;i<text->length;++i) {
    if(text->str[i]>127){PublishClientPlaybackQuality({});return true;}
    payload+=char(text->str[i]);
   }
   RichMetadata record;std::string error;
   if(ParsePlaybackMetadata(payload,record,error)) {
    snapshot.identity=PlaybackIdentity(record.kind=="episode"
        ? record.uri : record.title+"\x1f"+record.artist+"\x1f"+record.album);
    snapshot.level=ParsePlaybackLevel(record.playback_quality);
    const int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,record.title.data(),int(record.title.size()),nullptr,0);
    if(length>0 && length<=4096) {
     std::wstring title(size_t(length),L'\0');
     MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,record.title.data(),int(record.title.size()),title.data(),length);
     auto count=std::min<size_t>(title.size(),snapshot.title.size()-1);
     if(count && title[count-1]>=0xd800 && title[count-1]<=0xdbff)--count;
     std::copy_n(title.begin(),count,snapshot.title.begin());
    }
   }
  }
  PublishClientPlaybackQuality(snapshot);
 }catch(...){PublishClientPlaybackQuality({});}
 return true;
}

static bool ParseClassicM3U(const std::string& payload,std::wstring& suggested,std::string& playlist,
                            std::vector<ClassicM3UEntry>& entries) {
 constexpr char rs=0x1e,fs=0x1f;
 size_t first=payload.find(rs);
 if(first==std::string::npos)return false;
 auto header=payload.substr(0,first);
 size_t split=header.find(fs);
 if(split==std::string::npos)return false;
 suggested=WideUtf8(header.substr(0,split));
 playlist=header.substr(split+1);
 if(suggested.empty()||suggested.size()>240||playlist.size()>4096)return false;
 size_t start=first+1;
 while(start<payload.size()&&entries.size()<10000) {
  size_t end=payload.find(rs,start);if(end==std::string::npos)end=payload.size();
  auto record=payload.substr(start,end-start);
  std::array<std::string,4> fields;size_t at=0;bool valid=true;
  for(size_t i=0;i<3;i++) {
   size_t sep=record.find(fs,at);if(sep==std::string::npos){valid=false;break;}
   fields[i]=record.substr(at,sep-at);at=sep+1;
  }
  if(valid) {
   fields[3]=record.substr(at);
   ClassicM3UEntry entry;
   try{entry.duration_seconds=std::max(0L,std::min(86400L,std::stol(fields[0])));}catch(...){entry.duration_seconds=0;}
   entry.artist=fields[1];entry.title=fields[2];entry.path=WideUtf8(fields[3]);
   if(!entry.path.empty())entries.push_back(std::move(entry));
  }
  start=end+1;
 }
 return !entries.empty();
}
static bool ClassicUiMessage(const String* text) {
 constexpr wchar_t prefix[]=L"SOGGFY_UI_V1:";
 constexpr size_t n=sizeof(prefix)/sizeof(*prefix)-1;
 if(!text||!text->str||text->length<=n||text->length>n+1048576||wmemcmp(text->str,prefix,n))return false;
 if(!GetSettings().classic_ui)return true;
 const wchar_t* body=text->str+n;size_t length=text->length-n;
 const wchar_t* equal=std::find(body,body+length,L'=');
 if(equal==body+length)return true;
 std::wstring key(body,size_t(equal-body));
 const auto decoded=PercentDecode(equal+1,size_t(body+length-equal-1));
 bool ok=true;
 auto flag=[&]()->bool{return decoded=="1"||decoded=="true";};
 if(key==L"sync") {}
 else if(key==L"downloads")ok=SetDownloads(flag());
 else if(key==L"ogg")ok=SetOgg(flag());
 else if(key==L"flac")ok=SetFlac(flag());
 else if(key==L"metadata")ok=SetMetadata(flag());
 else if(key==L"log")ok=SetLogging(flag());
 else if(key==L"debug")ok=SetDebugLogging(flag());
 else if(key==L"normalize")ok=SetNormalizeArtistSeparators(flag());
 else if(key==L"skipDownloaded")ok=SetSkipDownloadedTracks(flag());
 else if(key==L"skipIgnored")ok=SetSkipIgnoredTracks(flag());
 else if(key==L"embedCover")ok=SetEmbedCoverArt(flag());
 else if(key==L"saveCover")ok=SetSaveCoverArt(flag());
 else if(key==L"embedLyrics")ok=SetEmbedLyrics(flag());
 else if(key==L"saveLyrics")ok=SetSaveLyrics(flag());
 else if(key==L"saveCanvas")ok=SetSaveCanvas(flag());
 else if(key==L"blockTelemetry")ok=SetBlockTelemetry(flag());
 else if(key==L"liftQueue")ok=SetLiftAddToQueue(flag());
 else if(key==L"keepNative")ok=SetKeepNativeOriginal(flag());
 else if(key==L"playbackSpeed") {
  try{
   ok=SetPlaybackSpeed(std::stod(decoded));
   if(ok)ApplyPlaybackSpeedNow();
  }catch(...){ok=false;}
 }
 else if(key==L"template")ok=SetPathTemplate(WideUtf8(decoded));
 else if(key==L"podcastTemplate")ok=SetPodcastTemplate(WideUtf8(decoded));
 else if(key==L"canvasTemplate")ok=SetCanvasTemplate(WideUtf8(decoded));
 else if(key==L"invalidChars")ok=SetInvalidCharReplacement(WideUtf8(decoded));
 else if(key==L"outputPreset")ok=SetOutputPreset(WideUtf8(decoded));
 else if(key==L"outputExt")ok=SetOutputExtension(WideUtf8(decoded));
 else if(key==L"outputArgs")ok=SetOutputArguments(WideUtf8(decoded));
 else if(key==L"ffmpegPath")ok=SetFFmpegPath(WideUtf8(decoded));
 else if(key==L"root")ok=SetSaveLocation(WideUtf8(decoded));
 else if(key==L"browse"){PickSaveLocation(GetActiveWindow());}
 else if(key==L"ignore_current"){SetClassicCurrentIgnored(flag());return true;}
 else if(key==L"status_batch"){QueueClassicStatusRequest(decoded);return true;}
 else if(key==L"open_folder"){RevealClassicTrack(WideUtf8(decoded));return true;}
 else if(key==L"save_m3u"){
  std::wstring suggested;std::string playlist;std::vector<ClassicM3UEntry> entries;
  if(ParseClassicM3U(decoded,suggested,playlist,entries))SaveClassicM3U(suggested,playlist,entries);
  else HistoryLog("failed to parse classic M3U request");
  return true;
 }
 else if(key==L"canvas"){
  constexpr char fs=0x1f;
  std::array<std::string,5> fields;size_t at=0;bool valid=true;
  for(size_t i=0;i<4;i++){size_t sep=decoded.find(fs,at);if(sep==std::string::npos){valid=false;break;}fields[i]=decoded.substr(at,sep-at);at=sep+1;}
  if(valid){
   fields[4]=decoded.substr(at);unsigned track=0;try{track=unsigned(std::stoul(fields[4]));}catch(...){}
   QueueClassicCanvasDownload(WideUtf8(fields[0]),WideUtf8(fields[1]),WideUtf8(fields[2]),WideUtf8(fields[3]),track);
  }
  return true;
 }
 else return true;
 if(!ok)HistoryLog("classic UI setting could not be saved");
 SyncClassicUi();
 return true;
}
static int Console(Display* self,Browser* b,int level,const String* text,const String* source,int line) {
 auto callback=metadata_callbacks.Enter();
 if(ClassicUiMessage(text)){Release(b);return 1;}
 if(CurrentPlayback(text)){Release(b);return 1;}
 if(text&&text->length<100&&text->length>=15&&wmemcmp(text->str,L"FLOGGFY_STATUS:",15)==0){
  std::wstring status(text->str,text->length);HistoryLog(Utf8(status).c_str());Release(b);return 1;
 }
 if(Enqueue(text)){Release(b);return 1;}
 return console_callback.Original<int(*)(Display*,Browser*,int,const String*,const String*,int)>()(self,b,level,text,source,line);
}
static void Inject(Frame* frame) {
 if(!frame||frame->base.size!=sizeof(Frame))return;
 auto main=reinterpret_cast<int(*)(Frame*)>(frame->methods[15]);
 if(!main||!main(frame))return;
 frame->base.add(&frame->base);AcquireSRWLockExclusive(&frame_lock);auto old=polling_frame;polling_frame=frame;ReleaseSRWLockExclusive(&frame_lock);Release(old);
 auto settings=GetSettings();
 SyncClassicUi(frame);
 // Keep the collector loaded when Classic UI is active so Metadata can be toggled
 // without restarting Spotify. Its tick exits immediately while disabled.
 if(settings.metadata||settings.classic_ui)ExecuteFrameCode(frame,metadata_script,L"floggfy-metadata.js");
 if(settings.classic_ui)ExecuteFrameCode(frame,soggfy_ui_script,L"soggfy-ui.js");
 HistoryLog(settings.classic_ui?"classic Soggfy UI injected into main frame":"metadata script injected into main frame");
}
static void Loading(Load* self,Browser* b,int loading,int back,int forward){
 auto callback=metadata_callbacks.Enter();
 if(!loading&&b&&b->base.size==sizeof(Browser)){auto frame=reinterpret_cast<Frame*(*)(Browser*)>(b->methods[14])(b);Inject(frame);Release(frame);}
 loading_callback.Original<void(*)(Load*,Browser*,int,int,int)>()(self,b,loading,back,forward);
}
static void LoadEnd(Load* self,Browser* b,Frame* f,int status){
 auto callback=metadata_callbacks.Enter();Inject(f);
 load_end_callback.Original<void(*)(Load*,Browser*,Frame*,int)>()(self,b,f,status);
}
static Display* GetDisplay(Client* self){
 auto callback=metadata_callbacks.Enter();auto result=display_getter.Original<Display*(*)(Client*)>()(self);
 if(result&&result->base.size==sizeof(Display))console_callback.Install(result->methods[6],reinterpret_cast<void*>(Console),"console");
 return result;
}
static Load* GetLoad(Client* self){
 auto callback=metadata_callbacks.Enter();auto result=load_getter.Original<Load*(*)(Client*)>()(self);
 if(result&&result->base.size==sizeof(Load)){
  loading_callback.Install(result->methods[0],reinterpret_cast<void*>(Loading),"loading");
  load_end_callback.Install(result->methods[2],reinterpret_cast<void*>(LoadEnd),"load end");
 }
 return result;
}
static Client* ObserveClient(Client* client){
 if(!client||client->base.size!=sizeof(Client)){HistoryLog("metadata client ABI unsupported; collector not attached");return client;}
 const bool display=display_getter.Install(client->methods[4],reinterpret_cast<void*>(GetDisplay),"display getter");
 const bool load=load_getter.Install(client->methods[14],reinterpret_cast<void*>(GetLoad),"load getter");
 HistoryLog(display&&load?"metadata browser callbacks attached; original client preserved":"metadata browser callbacks unavailable; original client preserved");
 return client;
}
using Create=int(*)(const void*,Client*,const String*,const void*,void*,void*);
using Sync=Browser*(*)(const void*,Client*,const String*,const void*,void*,void*);
using View=void*(*)(Client*,const String*,const void*,void*,void*,void*);
static Create original_create;static Sync original_sync;static View original_view;
static int CreateHook(const void* win,Client* client,const String* url,const void* settings,void* extra,void* context){auto callback=metadata_callbacks.Enter();return original_create(win,ObserveClient(client),url,settings,extra,context);}
static Browser* SyncHook(const void* win,Client* client,const String* url,const void* settings,void* extra,void* context){auto callback=metadata_callbacks.Enter();return original_sync(win,ObserveClient(client),url,settings,extra,context);}
static void* ViewHook(Client* client,const String* url,const void* settings,void* extra,void* context,void* delegate){auto callback=metadata_callbacks.Enter();return original_view(ObserveClient(client),url,settings,extra,context,delegate);}
}
void RequestAcceleratedAdvance(const std::string& token) {
 if(token.empty())return;
 AcquireSRWLockExclusive(&accelerated_advance_lock);
 accelerated_advance_token=token;
 ReleaseSRWLockExclusive(&accelerated_advance_lock);
 SchedulePoll();
 HistoryLog("accelerated transport advance queued");
}
std::string ReadClientPlaybackQuality(const Media& media) {
 if(!GetSettings().metadata)return {};
 const auto now=GetTickCount64();
 AcquireSRWLockShared(&cache_lock);
 auto m=cache.Find(Utf8(media.title),Utf8(media.artist),Utf8(media.album),media.duration);
 std::string level;
 if(m && m->quality_time && now>=m->quality_time && now-m->quality_time<=20000)
  level=m->playback_quality;
 ReleaseSRWLockShared(&cache_lock);
 return level;
}
void EnrichTags(const Media& media,Tags& tags) {
 if(!GetSettings().metadata)return;
 AcquireSRWLockShared(&cache_lock);
 auto m=cache.Find(Utf8(media.title),Utf8(media.artist),Utf8(media.album),media.duration);
 if(m)for(const auto& field:m->fields) {
  tags.fields.erase(std::remove_if(tags.fields.begin(),tags.fields.end(),[&](const auto& entry){return entry.first==field.first;}),tags.fields.end());
  tags.fields.push_back(field);
 }
 ReleaseSRWLockShared(&cache_lock);
}
static std::wstring Wide(const std::string& value) {
 if(value.empty())return {};
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
 if(count<=0)return {};
 std::wstring out(size_t(count),L'\0');
 if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),count)!=count)return {};
 return out;
}
static unsigned PositiveNumber(const std::string& value) {
 if(value.empty())return 0;
 unsigned result=0;
 for(char c:value) {
  if(c<'0'||c>'9')return 0;
  unsigned digit=unsigned(c-'0');
  if(result>1000000u)return 0;
  result=result*10u+digit;
 }
 return result;
}
void EnrichCatalog(const Media& media,Catalog& catalog) {
 if(!GetSettings().metadata)return;
 RichMetadata copy;bool found=false;
 AcquireSRWLockShared(&cache_lock);
 if(auto m=cache.Find(Utf8(media.title),Utf8(media.artist),Utf8(media.album),media.duration)){copy=*m;found=true;}
 ReleaseSRWLockShared(&cache_lock);
 if(!found)return;
 auto field=[&](const char* key)->std::string{
  auto it=copy.fields.find(key);return it==copy.fields.end()?std::string{}:it->second;
 };
 if(copy.kind=="episode"||field("MEDIA_KIND")=="episode"){
  catalog.kind=MediaKind::Podcast;
  auto show=Wide(field("SHOW"));if(show.empty())show=Wide(copy.album);
  auto author=Wide(field("AUTHOR"));if(author.empty())author=Wide(copy.artist);
  if(!show.empty()){catalog.show=show;catalog.album=show;}
  if(!author.empty()){catalog.author=author;catalog.artist=author;catalog.album_artist=author;catalog.all_artists=author;}
 }
  auto album_artist=Wide(field("ALBUMARTIST"));if(!album_artist.empty())catalog.album_artist=album_artist;
 auto artists=Wide(field("ARTIST"));if(!artists.empty())catalog.all_artists=artists;
 if(catalog.artist.empty()&&!catalog.album_artist.empty())catalog.artist=catalog.album_artist;
 unsigned track=PositiveNumber(field("TRACKNUMBER"));if(track)catalog.track=track;
 catalog.disc=PositiveNumber(field("DISCNUMBER"));
 catalog.total_discs=PositiveNumber(field("DISCTOTAL"));
 auto date=field("DATE");
 if(!date.empty())catalog.release_date=Wide(date);
 unsigned year=PositiveNumber(field("YEAR"));
 if(!year&&date.size()>=4)year=PositiveNumber(date.substr(0,4));
 if(year>=1000&&year<=9999)catalog.release_year=year;
}
void StartMetadataCollector(HMODULE cef) {
 const auto now=static_cast<std::uint64_t>(GetTickCount64());
 if(!metadata_init.TryBegin(now))return;
 auto settings=GetSettings();
 if(!settings.metadata&&!settings.classic_ui){HistoryLog("CEF bridge disabled: Metadata=0 and Classic UI=0");metadata_init.MarkUnsupported();return;}
 auto address=GetProcAddress(cef,"cef_version_info");int(*version)(int)=nullptr;static_assert(sizeof(version)==sizeof(address));memcpy(&version,&address,sizeof(version));
 if(!version||!cef_compat::IsSupported({version(0),version(1),version(2),version(3)})){
  HistoryLog("metadata CEF identity unsupported; revision is not in the audited compatibility table");metadata_init.MarkUnsupported();return;
 }
 auto post_address=GetProcAddress(cef,"cef_post_task");memcpy(&post_task,&post_address,sizeof(post_task));
 if(!post_task){HistoryLog("metadata collector unsupported: cef_post_task missing");metadata_init.MarkUnsupported();return;}
 message_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(!message_event){HistoryLog("metadata event allocation failed; retry scheduled");metadata_init.Retry(now);return;}
 head=tail=count=0;
 metadata_polling.store(false,std::memory_order_release);metadata_running.store(true,std::memory_order_release);
 HANDLE thread=CreateThread(nullptr,0,MetadataWorker,nullptr,0,nullptr);
 if(!thread){metadata_running.store(false,std::memory_order_release);CloseHandle(message_event);message_event=nullptr;HistoryLog("metadata worker startup failed; retry scheduled");metadata_init.Retry(now);return;}
 auto stop_worker=[&] {
  metadata_polling.store(false,std::memory_order_release);metadata_running.store(false,std::memory_order_release);SetEvent(message_event);
  const bool stopped=WaitForSingleObject(thread,5000)==WAIT_OBJECT_0;CloseHandle(thread);thread=nullptr;
  if(stopped){CloseHandle(message_event);message_event=nullptr;}return stopped;
 };
 auto status=MH_Initialize();if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
 if(status!=MH_OK){if(stop_worker()){HistoryLog("metadata MinHook initialization failed; retry scheduled");metadata_init.Retry(now);}else{HistoryLog("metadata worker did not quiesce; state preserved");metadata_init.MarkUnsupported();}return;}
 const char* names[]={"cef_browser_host_create_browser","cef_browser_host_create_browser_sync","cef_browser_view_create"};
 void* callbacks[]={reinterpret_cast<void*>(CreateHook),reinterpret_cast<void*>(SyncHook),reinterpret_cast<void*>(ViewHook)};
 void** originals[]={reinterpret_cast<void**>(&original_create),reinterpret_cast<void**>(&original_sync),reinterpret_cast<void**>(&original_view)};
 void* created_targets[3]={};unsigned created_count=0;hooks::InstallCounts counts;
 bool queued_any=false;
 for(unsigned i=0;i<3;i++){
  auto target=reinterpret_cast<void*>(GetProcAddress(cef,names[i]));if(!target)continue;
  counts.Found();status=MH_CreateHook(target,callbacks[i],originals[i]);counts.Created(status==MH_OK);
  if(status!=MH_OK){char line[200];snprintf(line,sizeof(line),"metadata hook %s create failed: %s",names[i],MH_StatusToString(status));HistoryLog(line);continue;}
  created_targets[created_count++]=target;
  status=MH_QueueEnableHook(target);
  counts.Enabled(status==MH_OK);
  if(status==MH_OK)queued_any=true;
  else {char line[200];snprintf(line,sizeof(line),"metadata hook %s queue failed: %s",names[i],MH_StatusToString(status));HistoryLog(line);}
 }
 if(counts.Usable()&&queued_any){
  status=MH_ApplyQueued();
  if(status!=MH_OK){
   char line[200];snprintf(line,sizeof(line),"metadata queued hook apply failed: %s",MH_StatusToString(status));HistoryLog(line);
   counts.enabled=0;
  } else HistoryLog("metadata browser hooks enabled in one queued MinHook apply");
 }
 if(!counts.Usable()){
  char line[180];snprintf(line,sizeof(line),"metadata collector unavailable: found=%u created=%u enabled=%u; retry scheduled",counts.found,counts.created,counts.enabled);HistoryLog(line);
  hooks::RollbackStatus rollback;
  auto disabled=[](MH_STATUS value){return value==MH_OK||value==MH_ERROR_DISABLED||value==MH_ERROR_NOT_CREATED;};
  auto removed=[](MH_STATUS value){return value==MH_OK||value==MH_ERROR_NOT_CREATED;};
  for(unsigned i=0;i<created_count;i++)rollback.ObserveDisable(disabled(MH_DisableHook(created_targets[i])));
  for(unsigned waited=0;metadata_callbacks.Active()&&waited<5000;++waited)Sleep(1);
  rollback.ObserveQuiescence(metadata_callbacks.Active()==0);
  if(rollback.quiescent)for(unsigned i=0;i<created_count;i++)rollback.ObserveRemove(removed(MH_RemoveHook(created_targets[i])));
  if(rollback.CanRelease()&&stop_worker())metadata_init.Retry(now);
  else {if(thread)CloseHandle(thread);HistoryLog("metadata rollback incomplete; callback state preserved and retries disabled");metadata_init.MarkUnsupported();}
  return;
 }
 metadata_polling.store(true,std::memory_order_release);SetEvent(message_event);CloseHandle(thread);metadata_init.Activate();
 char line[160];snprintf(line,sizeof(line),"metadata collector active: found=%u created=%u enabled=%u",counts.found,counts.created,counts.enabled);HistoryLog(line);
}
}
