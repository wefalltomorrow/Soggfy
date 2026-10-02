#include "classic_path_match.h"
#include "library_layout.h"
#include <cwctype>
#include <regex>

namespace history {
namespace {
void ReplaceAll(std::wstring& text,const std::wstring& from,const std::wstring& to) {
    if(from.empty())return;
    for(size_t at=0;(at=text.find(from,at))!=std::wstring::npos;at+=to.size())
        text.replace(at,from.size(),to);
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
std::wstring ExtensionRegex(const std::wstring& output_extension) {
    if(!output_extension.empty()) {
        std::wstring ext=output_extension;
        if(ext.front()==L'.')ext.erase(ext.begin());
        return RegexEscape(ext);
    }
    return L"(?:ogg|flac)";
}
}

std::wstring BuildClassicPathRegex(const ClassicPathQuery& q,
                                   const std::wstring& path_template,
                                   const std::wstring& output_extension,
                                   bool normalize_artist_separators,
                                   const std::wstring& invalid_char_replacement) {
    std::wstring pattern=path_template;
    if(pattern.empty()) {
        // Smart-layout fallback. Album/title are stable enough for status
        // detection; the artist directory stays permissive because media-session
        // album-artist text may differ from a playlist row's first artist.
        return L"^(?:Music\\\\)?Artists\\\\[^\\\\]+\\\\"+
            RegexEscape(EscapePathValue(q.album,L"-",L"Unknown Album"))+
            L"\\\\(?:\\d+ - )?"+
            RegexEscape(EscapePathValue(q.title,L"-",L"Untitled"))+
            L"\\.(?:"+ExtensionRegex(output_extension)+L")$";
    }

    std::wstring artist=q.artist;
    std::wstring all=q.all_artists.empty()?q.artist:q.all_artists;
    if(normalize_artist_separators) {
        ReplaceAll(artist,L" / ",L", ");
        ReplaceAll(all,L" / ",L", ");
    }

    constexpr wchar_t multi_marker[]=L"\u0001MULTIDISC\u0001";
    ReplaceAll(pattern,L"{multi_disc_path}",multi_marker);

    std::wstring escaped;escaped.reserve(pattern.size()*2);
    for(size_t i=0;i<pattern.size();) {
        if(pattern[i]==L'{') {
            auto close=pattern.find(L'}',i+1);
            if(close!=std::wstring::npos) {
                const auto token=pattern.substr(i,close-i+1);
                if(token==L"{track_name}")escaped+=RegexEscape(EscapePathValue(q.title,invalid_char_replacement,L"Untitled"));
                else if(token==L"{artist_name}")escaped+=RegexEscape(EscapePathValue(artist,invalid_char_replacement,L"Unknown Artist"));
                else if(token==L"{all_artist_names}")escaped+=RegexEscape(EscapePathValue(all,invalid_char_replacement,L"Unknown Artist"));
                else if(token==L"{album_name}")escaped+=RegexEscape(EscapePathValue(q.album,invalid_char_replacement,L"Unknown Album"));
                else if(token==L"{track_num}")escaped+=L"\\d+";
                else if(token==L"{track_num_2}")escaped+=L"\\d{2}";
                else if(token==L"{disc_num}")escaped+=L"\\d+";
                else if(token==L"{release_year}")escaped+=L"\\d{4}";
                else if(token==L"{release_date}")escaped+=L"\\d{4}(?:-\\d{2}(?:-\\d{2})?)?";
                else if(token==L"{multi_disc_paren}")escaped+=L"(?: \\(CD \\d+\\))?";
                else if(token==L"{playlist_name}"||token==L"{context_name}")escaped+=L"[^\\\\]+";
                else if(token==L"{context_index}")escaped+=L"\\d+";
                else if(token==L"{ext}")escaped+=ExtensionRegex(output_extension);
                else escaped+=L"[^\\\\]+";
                i=close+1;continue;
            }
        }
        if(pattern.compare(i,wcslen(multi_marker),multi_marker)==0) {
            escaped+=L"(?:\\\\CD \\d+)?";
            i+=wcslen(multi_marker);continue;
        }
        const wchar_t c=pattern[i++];
        if(c==L'/'||c==L'\\')escaped+=L"\\\\";
        else escaped+=RegexEscape(std::wstring(1,c));
    }

    if(pattern.find(L"{ext}")==std::wstring::npos)
        escaped+=L"\\.(?:"+ExtensionRegex(output_extension)+L")";
    return L"^"+escaped+L"$";
}

bool ClassicPathMatches(const std::wstring& relative_path,
                        const ClassicPathQuery& query,
                        const std::wstring& path_template,
                        const std::wstring& output_extension,
                        bool normalize_artist_separators,
                        const std::wstring& invalid_char_replacement) {
    try {
        return std::regex_match(relative_path,
            std::wregex(BuildClassicPathRegex(query,path_template,output_extension,
                                              normalize_artist_separators,invalid_char_replacement),
                        std::regex_constants::ECMAScript|std::regex_constants::icase));
    } catch(...) {
        return false;
    }
}
}
