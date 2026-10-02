#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include "classic_ui_backend.h"
#include "classic_path_match.h"
#include "history_settings.h"
#include "library_layout.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <regex>

namespace history {
namespace {
struct Recent {
    std::wstring title,artist,album;
    std::string status,message;
    std::wstring path;
    ULONGLONG time=0;
};
SRWLOCK recent_lock=SRWLOCK_INIT;
std::array<Recent,64> recent{};
size_t recent_next=0;
std::atomic<bool> current_ignored{false};
void ReplaceAll(std::wstring& text,const std::wstring& from,const std::wstring& to) {
    if(from.empty())return;
    for(size_t at=0;(at=text.find(from,at))!=std::wstring::npos;at+=to.size())
        text.replace(at,from.size(),to);
}

std::wstring ComparableArtist(std::wstring value) {
    ReplaceAll(value,L" / ",L", ");
    for(auto& c:value)c=wchar_t(towlower(c));
    return value;
}
bool SameText(const std::wstring& a,const std::wstring& b) {
    return _wcsicmp(a.c_str(),b.c_str())==0;
}
bool ArtistMatches(const ClassicTrackQuery& q,const Recent& entry) {
    const auto live=ComparableArtist(entry.artist);
    if(live.empty())return true;
    const auto first=ComparableArtist(q.artist);
    const auto all=ComparableArtist(q.all_artists);
    if(first==live||all==live)return true;
    return (!first.empty() && live.find(first)!=std::wstring::npos) ||
           (!all.empty() && all.find(live)!=std::wstring::npos);
}

std::wstring DisplayPath(const std::filesystem::path& path) {
    auto value=path.wstring();
    if(value.rfind(L"\\\\?\\UNC\\",0)==0)return L"\\\\"+value.substr(8);
    if(value.rfind(L"\\\\?\\",0)==0)return value.substr(4);
    return value;
}
}

void SetClassicTrackStatus(const Media& media,const char* status,const std::string& message,
                           const std::wstring& path) {
    if(!status||!*status)return;
    Recent item;
    item.title=media.title;
    item.artist=media.artist;
    item.album=media.album;
    item.status=status;
    item.message=message;
    item.path=path;
    item.time=GetTickCount64();
    AcquireSRWLockExclusive(&recent_lock);
    // Update an existing identity first so a DONE replaces CONVERTING.
    bool replaced=false;
    for(auto& existing:recent) {
        if(!existing.title.empty()&&SameText(existing.title,item.title)&&
           SameText(existing.album,item.album)&&
           ComparableArtist(existing.artist)==ComparableArtist(item.artist)) {
            existing=std::move(item);replaced=true;break;
        }
    }
    if(!replaced) {
        recent[recent_next]=std::move(item);
        recent_next=(recent_next+1)%recent.size();
    }
    ReleaseSRWLockExclusive(&recent_lock);
}

std::vector<ClassicTrackResult> QueryClassicTrackStatuses(const std::vector<ClassicTrackQuery>& queries) {
    std::vector<ClassicTrackResult> results;
    results.reserve(queries.size());
    auto settings=GetSettings();
    const ULONGLONG now=GetTickCount64();

    // Snapshot recent live states so filesystem walking never holds the lock.
    std::array<Recent,64> live;
    AcquireSRWLockShared(&recent_lock);live=recent;ReleaseSRWLockShared(&recent_lock);

    struct Pending {
        ClassicTrackQuery query;
        std::wregex regex;
        std::wstring match;
        unsigned matches=0;
    };
    std::vector<Pending> pending;

    for(const auto& q:queries) {
        ClassicTrackResult result;result.uri=q.uri;
        const Recent* newest=nullptr;
        for(const auto& entry:live) {
            if(!entry.title.empty() && SameText(q.title,entry.title) && SameText(q.album,entry.album) &&
               ArtistMatches(q,entry) && now>=entry.time && now-entry.time<=120000 &&
               (!newest||entry.time>newest->time))newest=&entry;
        }
        if(newest && newest->status!="DONE") {
            result.status=newest->status;result.message=newest->message;result.path=newest->path;
            results.push_back(std::move(result));continue;
        }
        try {
            ClassicPathQuery path_query{q.title,q.artist,q.album,q.all_artists};
            pending.push_back({q,std::wregex(BuildClassicPathRegex(path_query,settings.path_template,
                               settings.output_ext,settings.normalize_artist_separators,
                               settings.invalid_char_repl),
                               std::regex_constants::ECMAScript|std::regex_constants::icase),{},0});
        } catch(...) {
            result.status="WARN";result.message="Invalid path template";
            results.push_back(std::move(result));
        }
    }

    if(!pending.empty()) {
        std::error_code ec;
        const auto root=std::filesystem::path(WindowsPath(settings.root));
        size_t visited=0;
        if(std::filesystem::exists(root,ec) && !ec) {
            std::filesystem::recursive_directory_iterator it(
                root,std::filesystem::directory_options::skip_permission_denied,ec),end;
            for(;it!=end && !ec && visited<100000;it.increment(ec)) {
                if(ec)break;
                if(it.depth()>12){it.disable_recursion_pending();continue;}
                if(!it->is_regular_file(ec)||ec){ec.clear();continue;}
                ++visited;
                auto full=it->path().wstring();
                auto base=root.wstring();
                if(full.size()<=base.size())continue;
                size_t offset=base.size();
                if(full[offset]==L'\\'||full[offset]==L'/')++offset;
                auto relative=full.substr(offset);
                for(auto& p:pending) {
                    if(p.matches<2 && std::regex_match(relative,p.regex)) {
                        ++p.matches;
                        if(p.matches==1)p.match=DisplayPath(it->path());
                    }
                }
            }
        }

        for(auto& p:pending) {
            ClassicTrackResult result;result.uri=p.query.uri;
            if(p.matches==1) {
                result.status="DONE";result.path=p.match;
            } else if(p.matches>1) {
                result.status="WARN";result.message="Different tracks mapping to the same file name";
            }
            results.push_back(std::move(result));
        }
    }
    return results;
}


namespace {
struct M3USaveJob {
    std::wstring suggested;
    std::string playlist;
    std::vector<ClassicM3UEntry> entries;
};
static std::string CleanM3UText(std::string value) {
    for(char& c:value)if(c=='\r'||c=='\n')c=' ';
    return value;
}
static std::string Utf8Path(const std::wstring& value) {
    if(value.empty())return {};
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),
                                 nullptr,0,nullptr,nullptr);
    if(size<=0)return {};
    std::string out(size_t(size),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),
                           out.data(),size,nullptr,nullptr)!=size)return {};
    return out;
}
static DWORD WINAPI SaveM3UWorker(LPVOID param) {
    std::unique_ptr<M3USaveJob> job(static_cast<M3USaveJob*>(param));
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return 0;

    IFileSaveDialog* dialog=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,
                                IID_IFileSaveDialog,reinterpret_cast<void**>(&dialog));
    if(SUCCEEDED(hr)&&dialog) {
        COMDLG_FILTERSPEC types[]={{L"M3U8 playlist",L"*.m3u8"},{L"M3U playlist",L"*.m3u"}};
        dialog->SetFileTypes(2,types);
        dialog->SetDefaultExtension(L"m3u8");
        dialog->SetTitle(L"Soggfy — Generate M3U");
        if(!job->suggested.empty())dialog->SetFileName(job->suggested.c_str());

        auto settings=GetSettings();
        IShellItem* initial=nullptr;
        if(!settings.root.empty() &&
           SUCCEEDED(SHCreateItemFromParsingName(settings.root.c_str(),nullptr,IID_IShellItem,
                                                reinterpret_cast<void**>(&initial)))) {
            dialog->SetFolder(initial);
            initial->Release();
        }

        if(SUCCEEDED(dialog->Show(nullptr))) {
            IShellItem* item=nullptr;
            PWSTR selected=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))&&item) {
                if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&selected))&&selected) {
                    std::filesystem::path output(selected);
                    std::filesystem::path parent=output.parent_path();
                    std::string data="#EXTM3U\n#PLAYLIST:"+CleanM3UText(job->playlist)+"\n\n";
                    for(const auto& entry:job->entries) {
                        data+="#EXTINF:"+std::to_string(entry.duration_seconds)+","+
                              CleanM3UText(entry.artist)+" - "+CleanM3UText(entry.title)+"\n";
                        std::error_code ec;
                        auto relative=std::filesystem::proximate(std::filesystem::path(entry.path),parent,ec);
                        std::wstring value=ec?entry.path:relative.wstring();
                        std::replace(value.begin(),value.end(),L'\\',L'/');
                        data+=Utf8Path(value)+"\n\n";
                    }
                    HANDLE file=CreateFileW(WindowsPath(output.wstring()).c_str(),GENERIC_WRITE,0,nullptr,
                                            CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                    if(file!=INVALID_HANDLE_VALUE) {
                        DWORD written=0;
                        bool ok=data.size()<=MAXDWORD &&
                            WriteFile(file,data.data(),DWORD(data.size()),&written,nullptr) &&
                            written==data.size();
                        CloseHandle(file);
                        if(!ok) {
                            DeleteFileW(WindowsPath(output.wstring()).c_str());
                            HistoryLog("failed to write generated M3U playlist");
                        } else {
                            HistoryLog(("generated M3U tracks="+std::to_string(job->entries.size())).c_str());
                        }
                    } else HistoryLog("failed to create generated M3U playlist");
                    CoTaskMemFree(selected);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();
    return 0;
}
}

bool SaveClassicM3U(const std::wstring& suggested_filename,const std::string& playlist_name,
                    const std::vector<ClassicM3UEntry>& entries) {
    if(suggested_filename.size()>240||playlist_name.size()>4096||entries.size()>10000)return false;
    auto job=std::make_unique<M3USaveJob>();
    job->suggested=suggested_filename;
    if(job->suggested.empty())job->suggested=L"Spotify.m3u8";
    if(job->suggested.size()<5 ||
       _wcsicmp(job->suggested.c_str()+job->suggested.size()-5,L".m3u8")!=0)
        job->suggested+=L".m3u8";
    job->playlist=playlist_name;
    job->entries=entries;
    HANDLE thread=CreateThread(nullptr,0,SaveM3UWorker,job.get(),0,nullptr);
    if(!thread)return false;
    job.release();
    CloseHandle(thread);
    return true;
}

void SetClassicCurrentIgnored(bool ignored) {
    current_ignored.store(ignored,std::memory_order_release);
}
bool ClassicCurrentIgnored() {
    return current_ignored.load(std::memory_order_acquire);
}

bool RevealClassicTrack(const std::wstring& path) {
    if(path.empty())return false;
    std::wstring args=L"/select,\""+path+L"\"";
    auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"explorer.exe",
        args.c_str(),nullptr,SW_SHOWNORMAL));
    return result>32;
}
}
