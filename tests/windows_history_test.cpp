#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../native/file_publication.h"
#include "../native/history_settings.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <filesystem>
using namespace history;
static void check(bool ok,const char* label) { if(!ok) { std::fprintf(stderr,"FAIL: %s error=%lu\n",label,GetLastError()); std::exit(1); } }
static std::vector<unsigned char> read(const std::wstring& p) {
    HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"open test fixture"); std::vector<unsigned char> b(GetFileSize(h,nullptr)); DWORD n;
    check(ReadFile(h,b.data(),DWORD(b.size()),&n,nullptr) && n==b.size(),"read fixture"); CloseHandle(h); return b;
}
int main() {
    wchar_t exe[2048]; GetModuleFileNameW(nullptr,exe,2048); std::wstring dir=exe; dir.resize(dir.find_last_of(L'\\'));
    auto ini=dir+L"\\SpotifyHistory.ini"; DeleteFileW(ini.c_str());
    check(GetFileAttributesW(ini.c_str())==INVALID_FILE_ATTRIBUTES,"INI absent before initialization");
    InitSettings(GetModuleHandleW(nullptr));
    check(GetFileAttributesW(ini.c_str())!=INVALID_FILE_ATTRIBUTES,"missing INI created beside DLL");
    auto defaults=GetSettings();
    check(!defaults.downloads && !defaults.menu && defaults.classic_ui && defaults.ogg && defaults.flac &&
          defaults.metadata && defaults.log && !defaults.debug_log,
          "generated INI loads complete default switches");
    check(defaults.save_location.empty() && defaults.max_buffered_mib==500,
          "generated INI uses default Music location and 500 MiB buffer");
    check(GetPrivateProfileIntW(L"History",L"StopAfter",UINT(-1),ini.c_str())==UINT(-1),"generated INI omits history limit");
    auto low=dir+L"\\low.ogg",high=dir+L"\\high.ogg",target=dir+L"\\published.ogg";
    Quality qlow,qhigh,qflac; check(ReadQuality(low,qlow) && qlow.bitrate==160000,"real CRC Vorbis low bitrate header");
    check(ReadQuality(high,qhigh) && qhigh.bitrate==320000,"real CRC Vorbis high bitrate header");
    check(ReadQuality(dir+L"\\existing.flac",qflac) && qflac.rate==44100 && qflac.channels==2 && qflac.bits==16,"FLAC STREAMINFO quality");
    check(DecideSave(high,qlow)==SaveDecision::Skip && DecideSave(high,qhigh)==SaveDecision::Skip,"equal and higher file skipped");
    check(DecideSave(dir+L"\\unknown.ogg",qhigh)==SaveDecision::Skip,"unknown existing content preserved");
    auto lo=read(low),hi=read(high); DeleteFileW(target.c_str());
    const std::string a="spotify:track:"+std::string(22,'A'),b="spotify:track:"+std::string(22,'B');
    const auto tagged=read(dir+L"\\A-160000.ogg"),better=read(dir+L"\\A-320000.ogg"),foreign=read(dir+L"\\B-320000.ogg");
    for(size_t n=0;n<tagged.size();++n)check(ParseRecordingIdentity(tagged.data(),n).state==IdentityState::Unknown,"truncated Ogg identity never authoritative");
    auto damaged=tagged;damaged.back()^=1;
    check(ParseRecordingIdentity(damaged.data(),damaged.size()).state==IdentityState::Unknown,"bad metadata page CRC rejected");
    for(const auto* ext:{L".flac",L".mp3"})check(ReadRecordingIdentity(dir+L"\\A"+ext).uri==a,"native and converted identity tags");
    check(PublishBytes(target,qlow,tagged.data(),tagged.size())==FilePublication::Saved,"tagged initial capture");
    check(PublishBytes(target,qhigh,foreign.data(),foreign.size())==FilePublication::Failed&&read(target)==tagged,"different track cannot replace or skip as same recording");
    HANDLE upgradeLock=CreateFileW(target.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(upgradeLock!=INVALID_HANDLE_VALUE,"lock upgrade destination");
    check(PublishBytes(target,qhigh,better.data(),better.size())==FilePublication::Failed&&read(target)==tagged,"locked destination preserves old recording");
    CloseHandle(upgradeLock);
    const auto duplicates=DuplicatePaths(dir,target,b);
    check(duplicates.size()==3&&ResolveRecordingPath(dir,target,b,L"mp3")==duplicates[0],"explicit conflict routes to first duplicate suffix");
    std::filesystem::create_directories(std::filesystem::path(duplicates[0]).parent_path());
    check(PublishBytes(duplicates[0],qlow,tagged.data(),tagged.size())==FilePublication::Saved,"occupied short suffix fixture");
    check(ResolveRecordingPath(dir,target,b,L"mp3")==duplicates[1],"short suffix collision escalates");
    check(PublishBytes(duplicates[1],qhigh,foreign.data(),foreign.size())==FilePublication::Saved,"different identity stored separately");
    check(ResolveRecordingPath(dir,target,b,L"mp3")==duplicates[1],"same track reuses matching duplicate");
    DeleteFileW(duplicates[0].c_str());
    check(ResolveRecordingPath(dir,target,b,L"mp3")==duplicates[1],"matching longer suffix preferred to newly vacant short suffix");
    DeleteFileW(duplicates[1].c_str());
    const auto converted=duplicates[1].substr(0,duplicates[1].find_last_of(L'.'))+L".mp3";
    check(CopyFileW((dir+L"\\B.mp3").c_str(),converted.c_str(),TRUE)!=0,"converted duplicate fixture");
    check(ResolveRecordingPath(dir,target,b,L"mp3")==duplicates[1],"converted duplicate retains its namespace");
    DeleteFileW(converted.c_str());
    for(const auto& duplicate:duplicates)check(PublishBytes(duplicate,qlow,tagged.data(),tagged.size())==FilePublication::Saved,"occupy all suffixes");
    check(ResolveRecordingPath(dir,target,b,L"mp3").empty(),"exhausted suffixes preserve every file");
    for(const auto& duplicate:duplicates)DeleteFileW(duplicate.c_str());
    check(PublishBytes(target,qhigh,better.data(),better.size())==FilePublication::Upgraded&&read(target)==better,"same identity upgrades under destination lock");
    DeleteFileW(target.c_str());
    check(PublishBytes(target,qlow,lo.data(),lo.size())==FilePublication::Saved,"first completed file published");
    check(ResolveRecordingPath(dir,target,b,L"mp3")==target,"legacy header-only file stays canonical");
    check(PublishBytes(target,qhigh,better.data(),better.size())==FilePublication::Failed&&read(target)==lo,"unreadable identity cannot authorize tagged upgrade");
    check(PublishBytes(target,qlow,hi.data(),hi.size())==FilePublication::Skipped && read(target)==lo,"duplicate skip performs no overwrite");
    check(PublishBytes(target,qhigh,hi.data(),0)==FilePublication::Failed && read(target)==lo,"failed upgrade preserves previous file");
    check(PublishBytes(target,qhigh,hi.data(),hi.size())==FilePublication::Upgraded && read(target)==hi,"verified higher quality replaces lower quality");
    check(PublishBytes(target,qlow,lo.data(),lo.size())==FilePublication::Skipped && read(target)==hi,"downgrade prevented");
    WIN32_FIND_DATAW data; HANDLE files=FindFirstFileW((target+L".complete-*").c_str(),&data);
    check(files==INVALID_HANDLE_VALUE,"no completed staging files remain"); if(files!=INVALID_HANDLE_VALUE) FindClose(files);
    const auto playlist=dir+L"\\export.m3u8";
    check(ReplaceCompleteFile(playlist,lo.data(),lo.size()),"new export saved");
    HANDLE exportLock=CreateFileW(playlist.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(exportLock!=INVALID_HANDLE_VALUE,"lock existing export");
    check(!ReplaceCompleteFile(playlist,hi.data(),hi.size())&&read(playlist)==lo,"failed export preserves old bytes");
    CloseHandle(exportLock);
    check(ReplaceCompleteFile(playlist,hi.data(),hi.size())&&read(playlist)==hi,"complete export replaces old bytes");
    check(!ReplaceCompleteFile(playlist,lo.data(),0)&&read(playlist)==hi,"invalid export preserves old file");
    files=FindFirstFileW((playlist+L".export-*").c_str(),&data);
    check(files==INVALID_HANDLE_VALUE,"no export staging files remain");if(files!=INVALID_HANDLE_VALUE)FindClose(files);
    DeleteFileW(playlist.c_str());
    check(SetDownloads(true) && OggEnabled(),"Downloads enabled immediately");
    check(SetOgg(false) && !OggEnabled(),"Ogg disable immediately bypasses capture");
    check(SetOgg(true) && SetDownloads(false) && !OggEnabled(),"Downloads off persists and bypasses capture");
    unsigned epoch=GetSettings().capture_epoch;
    check(SetDownloads(true) && SetDownloads(false) && GetSettings().capture_epoch==epoch+2 && CaptureEpoch()==epoch+2,"missed off-on transitions still advance capture epoch");
    check(FlushSettings(5000),"background settings persisted");
    check(GetPrivateProfileIntW(L"To Disk",L"Downloads",9,ini.c_str())==0,"INI persists disabled setting");
    auto root=dir+L"\\Pending-"+std::to_wstring(GetCurrentProcessId()); epoch=GetSettings().capture_epoch;
    check(SetSaveLocation(root) && GetSettings().root==root,"save directory persistence");
    check(GetFileAttributesW(root.c_str())==INVALID_FILE_ATTRIBUTES && GetSettings().capture_epoch==epoch,"save location does not precreate folders or invalidate capture");
    check(FlushSettings(5000),"background save location persisted");
    DeleteFileW(target.c_str());
    puts("PASS: Windows quality parsing, duplicate skip, locked upgrade, downgrade protection, failure preservation and settings persistence");
}
