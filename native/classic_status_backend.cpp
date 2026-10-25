#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "classic_ui_backend.h"
#include "classic_path_match.h"
#include "history_settings.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <regex>

namespace history {
namespace {
constexpr ULONGLONG kIndexLifetime=10000,kRecentLifetime=120000;
constexpr size_t kMaxIndexedFiles=100000,kPatternCacheSize=512;
std::atomic<unsigned> status_revision{0};
std::mutex query_mutex;
struct Recent {
    std::wstring title,artist,album,configuration;
    std::string status,message;
    std::wstring path;
    ULONGLONG time=0;
};
std::mutex recent_mutex;
std::array<Recent,64> recent{};
size_t recent_next=0;
std::wstring Key(std::initializer_list<std::wstring> fields) {
    std::wstring key;
    for(const auto& value:fields)key+=std::to_wstring(value.size())+L":"+value;
    return key;
}
std::wstring Configuration(const Settings& s) {
    return Key({s.root,s.path_template,s.podcast_template,s.output_ext,s.invalid_char_repl,
        s.normalize_artist_separators?L"1":L"0"});
}
std::wstring ComparableArtist(std::wstring value) {
    for(size_t at=0;(at=value.find(L" / ",at))!=std::wstring::npos;at+=2)value.replace(at,3,L", ");
    for(auto& c:value)c=wchar_t(towlower(c));
    return value;
}
bool SameText(const std::wstring& a,const std::wstring& b) {return _wcsicmp(a.c_str(),b.c_str())==0;}
bool ArtistMatches(const ClassicTrackQuery& q,const Recent& entry) {
    // Whole artist names only: "Air" must not match "Air Supply".
    const auto live=ComparableArtist(entry.artist);
    const auto first=ComparableArtist(q.artist),all=ComparableArtist(q.all_artists);
    return !live.empty()&&(first==live||all==live);
}
bool FilePresent(const std::wstring& path) {
    HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BY_HANDLE_FILE_INFORMATION info{};
    const bool valid=GetFileInformationByHandle(file,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&
        (info.nFileSizeHigh||info.nFileSizeLow);
    CloseHandle(file);return valid;
}
bool InRoot(const std::wstring& path,const std::wstring& root) {
    if(root.empty()||path.empty())return false;
    try {
        auto base=std::filesystem::absolute(root).lexically_normal().wstring();
        auto target=std::filesystem::absolute(path).lexically_normal().wstring();
        while(!base.empty()&&(base.back()==L'\\'||base.back()==L'/'))base.pop_back();
        return target.size()>base.size()&&_wcsnicmp(target.c_str(),base.c_str(),base.size())==0&&
            (target[base.size()]==L'\\'||target[base.size()]==L'/');
    } catch(...) {return false;}
}
struct IndexedFile {
    std::wstring relative,display;
};
std::wstring index_root;
unsigned index_revision=0;
uint64_t index_generation=0;
ULONGLONG index_time=0;
std::shared_ptr<const std::vector<IndexedFile>> index_files=
    std::make_shared<const std::vector<IndexedFile>>();

std::shared_ptr<const std::vector<IndexedFile>> FileIndex(const std::wstring& configured_root,ULONGLONG now,ClassicStatusMetrics& metrics) {
    const unsigned revision=ClassicStatusRevision();
    const bool fresh=index_root==configured_root&&index_revision==revision&&now>=index_time&&now-index_time<kIndexLifetime;
    if(fresh) {
        auto snapshot=index_files;
        return snapshot;
    }

    ++metrics.scans;
    const auto scan_started=GetTickCount64();
    std::vector<IndexedFile> built;
    built.reserve(4096);
    // Original Soggfy walks the actual output folder; our previous MinGW
    // std::filesystem walker silently ignored Windows enumeration failures.
    // Use the Win32 Unicode FindFirstFileW API that works with both ordinary
    // and extended-length paths, while preserving the same recursive lookup.
    std::wstring root=configured_root;
    while(root.size()>3 && (root.back()==L'\\' || root.back()==L'/'))root.pop_back();
    struct Dir { std::wstring relative; unsigned depth; };
    std::vector<Dir> stack;
    DWORD scan_error=ERROR_SUCCESS;
    std::size_t visited_dirs=0;
    const DWORD root_attrs=root.empty()?INVALID_FILE_ATTRIBUTES:
        GetFileAttributesW(WindowsPath(root).c_str());
    if(root_attrs!=INVALID_FILE_ATTRIBUTES && (root_attrs&FILE_ATTRIBUTE_DIRECTORY))
        stack.push_back({L"",0});
    else
        scan_error=GetLastError();
    while(!stack.empty() && built.size()<kMaxIndexedFiles) {
        auto dir=std::move(stack.back());stack.pop_back();
        const std::wstring folder=root+(dir.relative.empty()?L"":L"\\"+dir.relative);
        const auto search=WindowsPath(folder+L"\\*");
        WIN32_FIND_DATAW data{};
        HANDLE finder=FindFirstFileW(search.c_str(),&data);
        if(finder==INVALID_HANDLE_VALUE) {
            const DWORD error=GetLastError();
            if(error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND && scan_error==ERROR_SUCCESS)
                scan_error=error;
            continue;
        }
        ++visited_dirs;
        do {
            const std::wstring name(data.cFileName);
            if(name==L"." || name==L"..")continue;
            const auto relative=dir.relative.empty()?name:dir.relative+L"\\"+name;
            if(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) {
                if(!(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&dir.depth<12)
                    stack.push_back({relative,dir.depth+1});
            } else if(built.size()<kMaxIndexedFiles) {
                built.push_back({relative,root+L"\\"+relative});
            }
        } while(FindNextFileW(finder,&data) && built.size()<kMaxIndexedFiles);
        const DWORD error=GetLastError();
        if(error!=ERROR_NO_MORE_FILES && error!=ERROR_SUCCESS && scan_error==ERROR_SUCCESS)
            scan_error=error;
        FindClose(finder);
    }
    if(DebugLoggingEnabled()) {
        char log[320];
        std::snprintf(log,sizeof(log),
            "classic disk index file_count=%llu directory_count=%llu root_exists=%d root_attributes=0x%lx last_error=%lu",
            static_cast<unsigned long long>(built.size()),
            static_cast<unsigned long long>(visited_dirs),
            root_attrs!=INVALID_FILE_ATTRIBUTES && (root_attrs&FILE_ATTRIBUTE_DIRECTORY)?1:0,
            static_cast<unsigned long>(root_attrs),
            static_cast<unsigned long>(scan_error));
        HistoryLog(log);
    }

    index_root=configured_root;
    index_time=GetTickCount64();
    metrics.scan_ms+=index_time-scan_started;
    index_revision=revision;
    ++index_generation;
    index_files=std::make_shared<const std::vector<IndexedFile>>(std::move(built));
    auto snapshot=index_files;
    return snapshot;
}

struct Compiled {
    std::wregex current,flat,original;
    bool legacy=false;
    uint64_t generation=0;
    std::vector<std::wstring> matches;
};
using CacheList=std::list<std::pair<std::wstring,Compiled>>;
CacheList patterns;
std::map<std::wstring,CacheList::iterator> pattern_lookup;
std::wstring cached_configuration;
Compiled& Pattern(const ClassicTrackQuery& q,const Settings& s,ClassicStatusMetrics& metrics) {
    const auto key=Key({std::wstring(q.uri.begin(),q.uri.end()),q.title,q.artist,q.album,q.all_artists});
    const auto found=pattern_lookup.find(key);
    if(found!=pattern_lookup.end()) {
        patterns.splice(patterns.begin(),patterns,found->second);
        return found->second->second;
    }
    const auto started=GetTickCount64();
    Compiled item;
    ClassicPathQuery query{q.title,q.artist,q.album,q.all_artists};
    const auto& path_template=q.uri.rfind("spotify:episode:",0)==0&&!s.podcast_template.empty()?s.podcast_template:s.path_template;
    const auto flags=std::regex_constants::ECMAScript|std::regex_constants::icase;
    item.current=std::wregex(BuildClassicPathRegex(query,path_template,s.output_ext,
        s.normalize_artist_separators,s.invalid_char_repl,true),flags);
    ++metrics.compilations;
    item.legacy=q.uri.rfind("spotify:track:",0)==0;
    if(item.legacy) {
        item.flat=std::wregex(BuildLegacySoggfyFlatRegex(query),flags);
        item.original=std::wregex(BuildClassicPathRegex(query,
            L"{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.{ext}",L"",true,L"unicode",true),flags);
        metrics.compilations+=2;
    }
    metrics.compile_ms+=GetTickCount64()-started;
    if(patterns.size()>=kPatternCacheSize){pattern_lookup.erase(patterns.back().first);patterns.pop_back();}
    patterns.emplace_front(key,std::move(item));pattern_lookup[key]=patterns.begin();
    return patterns.front().second;
}
}
unsigned ClassicStatusRevision() {return status_revision.load(std::memory_order_acquire);}
void SetClassicTrackStatus(const Media& media,const char* status,const std::string& message,const std::wstring& path) {
    if(!status||!*status)return;
    Recent item{media.title,media.artist,media.album,Configuration(GetSettings()),status,message,path,GetTickCount64()};
    {
    std::lock_guard<std::mutex> guard(recent_mutex);
    bool replaced=false;
    for(auto& existing:recent) {
        if(!existing.title.empty()&&SameText(existing.title,item.title)&&SameText(existing.album,item.album)&&
           ComparableArtist(existing.artist)==ComparableArtist(item.artist)) {
            existing=item;replaced=true;break;
        }
    }
    if(!replaced){recent[recent_next]=std::move(item);recent_next=(recent_next+1)%recent.size();}
    }
    if(std::string(status)=="DONE")status_revision.fetch_add(1,std::memory_order_release);
}
std::vector<ClassicTrackResult> QueryClassicTrackStatuses(const std::vector<ClassicTrackQuery>& queries,ClassicStatusMetrics* output_metrics) {
    std::lock_guard<std::mutex> guard(query_mutex);
    ClassicStatusMetrics metrics;
    const auto started=GetTickCount64();
    const auto settings=GetSettings();const auto configuration=Configuration(settings);
    if(cached_configuration!=configuration){patterns.clear();pattern_lookup.clear();cached_configuration=configuration;}
    std::array<Recent,64> live;
    {std::lock_guard<std::mutex> recent_guard(recent_mutex);live=recent;}
    std::shared_ptr<const std::vector<IndexedFile>> files;
    std::vector<ClassicTrackResult> results;results.reserve(queries.size());
    for(const auto& q:queries) {
        ClassicTrackResult result;result.uri=q.uri;
        const Recent* newest=nullptr;
        for(const auto& entry:live) {
            if(entry.configuration==configuration&&!entry.title.empty()&&SameText(q.title,entry.title)&&
               (q.album.empty()||SameText(q.album,entry.album))&&ArtistMatches(q,entry)&&
               started>=entry.time&&started-entry.time<=kRecentLifetime&&(!newest||entry.time>newest->time))newest=&entry;
        }
        // DONE is always reconciled through file matching, including ambiguity.
        if(newest&&newest->status!="DONE") {
            result.status=newest->status;result.message=newest->message;result.path=newest->path;
            results.push_back(std::move(result));continue;
        }
        try {
            auto& compiled=Pattern(q,settings,metrics);
            if(!files)files=FileIndex(settings.root,started,metrics);
            bool fresh=compiled.generation==index_generation;
            if(fresh)for(const auto& path:compiled.matches)if(!FilePresent(path)){fresh=false;break;}
            if(fresh)++metrics.cache_hits;
            else {
                const auto match_started=GetTickCount64();
                compiled.matches.clear();
                auto matches=[&](const std::wstring& path,const std::wregex& regex){++metrics.regex_checks;return std::regex_match(path,regex);};
                for(const auto& file:*files) {
                    if((matches(file.relative,compiled.current)||(compiled.legacy&&
                       (matches(file.relative,compiled.flat)||matches(file.relative,compiled.original))))&&FilePresent(file.display)) {
                        compiled.matches.push_back(file.display);
                        if(compiled.matches.size()==2)break;
                    }
                }
                compiled.generation=index_generation;
                metrics.match_ms+=GetTickCount64()-match_started;
            }
            if(compiled.matches.size()==1){result.status="DONE";result.path=compiled.matches.front();}
            else if(compiled.matches.size()>1){result.status="WARN";result.message="Different tracks mapping to the same file name";}
            // A fresh publication may have richer naming metadata than a UI row.
            // Only use its real path after ordinary ambiguity checking, in this root.
            else if(newest&&newest->status=="DONE"&&InRoot(newest->path,settings.root)&&FilePresent(newest->path)) {
                result.status="DONE";result.path=newest->path;
            }
        } catch(const std::regex_error&) {result.status="WARN";result.message="Invalid path template";}
        results.push_back(std::move(result));
    }
    metrics.elapsed_ms=GetTickCount64()-started;
    if(output_metrics)*output_metrics=metrics;
    if(DebugLoggingEnabled()) {
        char line[320];std::snprintf(line,sizeof(line),
            "classic disk query tracks=%llu scans=%llu compile=%llu regex=%llu hits=%llu scan_ms=%llu compile_ms=%llu match_ms=%llu elapsed_ms=%llu",
            (unsigned long long)queries.size(),(unsigned long long)metrics.scans,(unsigned long long)metrics.compilations,
            (unsigned long long)metrics.regex_checks,(unsigned long long)metrics.cache_hits,(unsigned long long)metrics.scan_ms,
            (unsigned long long)metrics.compile_ms,(unsigned long long)metrics.match_ms,(unsigned long long)metrics.elapsed_ms);
        HistoryLog(line);
    }
    return results;
}
}
