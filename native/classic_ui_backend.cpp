#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "classic_ui_backend.h"
#include "history_settings.h"
#include "library_layout.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <regex>

namespace history {
namespace {
struct Recent {
    std::string identity,status,message;
    std::wstring path;
    ULONGLONG time=0;
};
SRWLOCK recent_lock=SRWLOCK_INIT;
std::array<Recent,64> recent{};
size_t recent_next=0;

std::string QueryIdentity(const ClassicTrackQuery& q) {
    return Utf8(q.title)+"\x1f"+Utf8(q.all_artists.empty()?q.artist:q.all_artists)+"\x1f"+Utf8(q.album);
}

std::wstring RegexEscape(const std::wstring& value) {
    static const std::wstring special=LR"(\.^$|()[]{}*+?)";
    std::wstring out;out.reserve(value.size()*2);
    for(wchar_t c:value) {
        if(special.find(c)!=std::wstring::npos)out.push_back(L'\\');
        out.push_back(c);
    }
    return out;
}

void ReplaceAll(std::wstring& text,const std::wstring& from,const std::wstring& to) {
    if(from.empty())return;
    for(size_t at=0;(at=text.find(from,at))!=std::wstring::npos;at+=to.size())
        text.replace(at,from.size(),to);
}

std::wstring FinalExtensionRegex(const Settings& settings) {
    if(!settings.output_ext.empty()) {
        std::wstring ext=settings.output_ext;
        if(ext.front()==L'.')ext.erase(ext.begin());
        return RegexEscape(ext);
    }
    return L"(?:ogg|flac)";
}

std::wstring BuildTemplateRegex(const ClassicTrackQuery& q,const Settings& settings) {
    std::wstring pattern=settings.path_template;
    if(pattern.empty()) {
        // Smart-layout fallback. Album and title are strong anchors; the artist
        // folder is intentionally permissive because Windows media-session
        // album-artist text can differ from a playlist row's first artist.
        return L"^(?:Music\\\\)?Artists\\\\[^\\\\]+\\\\"+
            RegexEscape(EscapePathValue(q.album,L"-",L"Unknown Album"))+
            L"\\\\(?:\\d+ - )?"+
            RegexEscape(EscapePathValue(q.title,L"-",L"Untitled"))+
            L"\\.(?:"+FinalExtensionRegex(settings)+L")$";
    }

    std::wstring artist=q.artist;
    std::wstring all=q.all_artists.empty()?q.artist:q.all_artists;
    if(settings.normalize_artist_separators) {
        ReplaceAll(all,L" / ",L", ");
        ReplaceAll(artist,L" / ",L", ");
    }

    // Preserve multi_disc_path as a structural placeholder before escaping.
    constexpr wchar_t multi_marker[]=L"\u0001MULTIDISC\u0001";
    ReplaceAll(pattern,L"{multi_disc_path}",multi_marker);

    std::wstring escaped;escaped.reserve(pattern.size()*2);
    for(size_t i=0;i<pattern.size();) {
        if(pattern[i]==L'{' ) {
            auto close=pattern.find(L'}',i+1);
            if(close!=std::wstring::npos) {
                auto token=pattern.substr(i,close-i+1);
                if(token==L"{track_name}")escaped+=RegexEscape(EscapePathValue(q.title,settings.invalid_char_repl,L"Untitled"));
                else if(token==L"{artist_name}")escaped+=RegexEscape(EscapePathValue(artist,settings.invalid_char_repl,L"Unknown Artist"));
                else if(token==L"{all_artist_names}")escaped+=RegexEscape(EscapePathValue(all,settings.invalid_char_repl,L"Unknown Artist"));
                else if(token==L"{album_name}")escaped+=RegexEscape(EscapePathValue(q.album,settings.invalid_char_repl,L"Unknown Album"));
                else if(token==L"{track_num}")escaped+=L"\\d+";
                else if(token==L"{track_num_2}")escaped+=L"\\d{2}";
                else if(token==L"{disc_num}")escaped+=L"\\d+";
                else if(token==L"{release_year}")escaped+=L"\\d{4}";
                else if(token==L"{release_date}")escaped+=L"\\d{4}(?:-\\d{2}(?:-\\d{2})?)?";
                else if(token==L"{multi_disc_paren}")escaped+=L"(?: \\(CD \\d+\\))?";
                else if(token==L"{playlist_name}"||token==L"{context_name}")escaped+=L"[^\\\\]+";
                else if(token==L"{context_index}")escaped+=L"\\d+";
                else if(token==L"{ext}")escaped+=FinalExtensionRegex(settings);
                else escaped+=L"[^\\\\]+";
                i=close+1;continue;
            }
        }
        if(pattern.compare(i,wcslen(multi_marker),multi_marker)==0) {
            escaped+=L"(?:\\\\CD \\d+)?";
            i+=wcslen(multi_marker);continue;
        }
        wchar_t c=pattern[i++];
        if(c==L'/'||c==L'\\')escaped+=L"\\\\";
        else escaped+=RegexEscape(std::wstring(1,c));
    }

    if(pattern.find(L"{ext}")==std::wstring::npos) {
        std::wstring ext=FinalExtensionRegex(settings);
        escaped+=L"\\.(?:"+ext+L")";
    }
    return L"^"+escaped+L"$";
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
    item.identity=media.Key();
    item.status=status;
    item.message=message;
    item.path=path;
    item.time=GetTickCount64();
    AcquireSRWLockExclusive(&recent_lock);
    // Update an existing identity first so a DONE replaces CONVERTING.
    bool replaced=false;
    for(auto& existing:recent) {
        if(!existing.identity.empty()&&existing.identity==item.identity) {
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
        const auto identity=QueryIdentity(q);
        const Recent* newest=nullptr;
        for(const auto& entry:live) {
            if(entry.identity==identity && now>=entry.time && now-entry.time<=120000 &&
               (!newest||entry.time>newest->time))newest=&entry;
        }
        if(newest && newest->status!="DONE") {
            result.status=newest->status;result.message=newest->message;result.path=newest->path;
            results.push_back(std::move(result));continue;
        }
        try {
            pending.push_back({q,std::wregex(BuildTemplateRegex(q,settings),
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

bool RevealClassicTrack(const std::wstring& path) {
    if(path.empty())return false;
    std::wstring args=L"/select,\""+path+L"\"";
    auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"explorer.exe",
        args.c_str(),nullptr,SW_SHOWNORMAL));
    return result>32;
}
}
