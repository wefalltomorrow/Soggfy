#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include "classic_ui_backend.h"
#include "media_session.h"
#include "history_settings.h"
#include "library_layout.h"
#include "file_publication.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <cwctype>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

namespace history {
namespace { std::atomic<bool> current_ignored{false}; }

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
                    if(ReplaceCompleteFile(output.wstring(),data.data(),data.size()))
                        HistoryLog(("generated M3U tracks="+std::to_string(job->entries.size())).c_str());
                    else HistoryLog("failed to save generated M3U playlist; existing file retained");
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


namespace {
struct CanvasJob {
    std::wstring url,title,artist,album;
    unsigned track=0;
};
static std::atomic<unsigned> canvas_workers{0};
static std::atomic<unsigned> canvas_sequence{0};

static bool DownloadCanvasHttps(const std::wstring& url,const std::wstring& path) {
    URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
    parts.dwSchemeLength=DWORD(-1);parts.dwHostNameLength=DWORD(-1);
    parts.dwUrlPathLength=DWORD(-1);parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),DWORD(url.size()),0,&parts) ||
       (parts.nScheme!=INTERNET_SCHEME_HTTPS&&parts.nScheme!=INTERNET_SCHEME_HTTP) ||
       !parts.lpszHostName||!parts.dwHostNameLength)return false;

    std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    std::wstring target;
    if(parts.lpszUrlPath&&parts.dwUrlPathLength)target.assign(parts.lpszUrlPath,parts.dwUrlPathLength);
    if(parts.lpszExtraInfo&&parts.dwExtraInfoLength)target.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    if(target.empty())target=L"/";

    HINTERNET session=WinHttpOpen(L"Soggfy/3.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return false;
    WinHttpSetTimeouts(session,10000,10000,15000,30000);
    HINTERNET connect=WinHttpConnect(session,host.c_str(),parts.nPort,0);
    if(!connect){WinHttpCloseHandle(session);return false;}
    DWORD flags=parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0;
    HINTERNET request=WinHttpOpenRequest(connect,L"GET",target.c_str(),nullptr,
                                         WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);
    bool ok=false;
    if(request&&WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,
                                   WINHTTP_NO_REQUEST_DATA,0,0,0)&&
       WinHttpReceiveResponse(request,nullptr)) {
        DWORD status=0,size=sizeof(status);
        if(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)&&
           status>=200&&status<300) {
            HANDLE file=CreateFileW(WindowsPath(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                ok=true;size_t total=0;
                for(;;) {
                    DWORD available=0;
                    if(!WinHttpQueryDataAvailable(request,&available)){ok=false;break;}
                    if(!available)break;
                    if(total+available>64u*1024u*1024u){ok=false;break;}
                    std::vector<unsigned char> buffer(std::min<DWORD>(available,256u*1024u));
                    DWORD read=0;
                    if(!WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&read)){ok=false;break;}
                    if(!read)break;
                    DWORD written=0;
                    if(!WriteFile(file,buffer.data(),read,&written,nullptr)||written!=read){ok=false;break;}
                    total+=read;
                }
                CloseHandle(file);
                if(!ok||!total){DeleteFileW(WindowsPath(path).c_str());ok=false;}
            }
        }
    }
    if(request)WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);WinHttpCloseHandle(session);
    return ok;
}
static DWORD WINAPI CanvasWorker(LPVOID param) {
    std::unique_ptr<CanvasJob> job(static_cast<CanvasJob*>(param));
    auto done=[](){canvas_workers.fetch_sub(1,std::memory_order_release);};
    auto settings=GetSettings();
    if(!settings.save_canvas){done();return 0;}

    Catalog catalog;catalog.title=job->title;catalog.artist=job->artist;
    catalog.album_artist=job->artist;catalog.all_artists=job->artist;
    catalog.album=job->album;catalog.track=job->track;
    std::wstring pattern=settings.canvas_template.empty()
        ?L"{artist_name}\\{album_name}\\Canvas\\{track_num}. {track_name}.mp4"
        :settings.canvas_template;
    auto destination=OutputPath(settings.root,catalog,L".mp4",settings.music_folder,pattern,
                                settings.normalize_artist_separators,settings.invalid_char_repl);
    auto slash=destination.find_last_of(L'\\');
    if(slash==std::wstring::npos||!EnsureDirectory(destination.substr(0,slash))){done();return 0;}
    if(GetFileAttributesW(WindowsPath(destination).c_str())!=INVALID_FILE_ATTRIBUTES){done();return 0;}

    auto seq=canvas_sequence.fetch_add(1,std::memory_order_relaxed)+1;
    std::wstring temp=destination+L".part-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(seq);
    if(DownloadCanvasHttps(job->url,temp)) {
        if(MoveFileExW(WindowsPath(temp).c_str(),WindowsPath(destination).c_str(),MOVEFILE_WRITE_THROUGH))
            HistoryLog(("saved canvas "+Utf8(destination)).c_str());
        else DeleteFileW(WindowsPath(temp).c_str());
    } else HistoryLog("canvas download failed");
    done();return 0;
}
}

bool QueueClassicCanvasDownload(const std::wstring& url,const std::wstring& title,
                                const std::wstring& artist,const std::wstring& album,
                                unsigned track) {
    if(url.empty()||url.size()>8192||title.size()>2048||artist.size()>2048||album.size()>2048)return false;
    if(!GetSettings().save_canvas)return true;
    unsigned active=canvas_workers.load(std::memory_order_acquire);
    while(active<2&&!canvas_workers.compare_exchange_weak(active,active+1,std::memory_order_acq_rel)){}
    if(active>=2)return false;
    auto job=std::make_unique<CanvasJob>();
    job->url=url;job->title=title;job->artist=artist;job->album=album;job->track=track;
    HANDLE thread=CreateThread(nullptr,0,CanvasWorker,job.get(),0,nullptr);
    if(!thread){canvas_workers.fetch_sub(1,std::memory_order_release);return false;}
    job.release();CloseHandle(thread);return true;
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
