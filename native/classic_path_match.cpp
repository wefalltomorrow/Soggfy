#include "classic_path_match.h"
#include "library_layout.h"
#include <algorithm>
#include <array>
#include <cwctype>
#include <regex>
#include <vector>

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
std::wstring Lower(std::wstring value) {
    for(auto& c:value)c=wchar_t(towlower(c));
    return value;
}
std::wstring ExtensionRegex(const std::wstring& output_extension,bool any_audio_extension) {
    std::wstring ext=output_extension;
    if(!ext.empty()&&ext.front()==L'.')ext.erase(ext.begin());
    ext=Lower(ext);

    if(any_audio_extension) {
        std::vector<std::wstring> extensions={
            L"mp3",L"m4a",L"mp4",L"ogg",L"opus",L"flac",L"aac",L"wav"
        };
        if(!ext.empty()&&std::find(extensions.begin(),extensions.end(),ext)==extensions.end())
            extensions.push_back(ext);
        std::wstring out=L"(?:";
        for(size_t i=0;i<extensions.size();++i) {
            if(i)out+=L"|";
            out+=RegexEscape(extensions[i]);
        }
        out+=L")";
        return out;
    }

    if(!ext.empty())return RegexEscape(ext);
    return L"(?:ogg|flac)";
}
bool SameInsensitive(const std::wstring& a,const std::wstring& b) {
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i)
        if(towlower(a[i])!=towlower(b[i]))return false;
    return true;
}
void AddUnique(std::vector<std::wstring>& values,std::wstring value) {
    if(value.empty())return;
    for(const auto& existing:values)if(SameInsensitive(existing,value))return;
    values.push_back(std::move(value));
}
// Spotify track rows join their artist list with ", ", while the native
// metadata collector and file publisher use "; ".  Accept both exact
// representations without weakening the track title or artist identity.
std::wstring PublishedSemicolonArtists(const ClassicPathQuery& q) {
    if(q.artist.empty())return {};
    const auto& all=q.all_artists;
    const size_t prefix=q.artist.size();
    if(all.size()<=prefix+2 ||
       !SameInsensitive(all.substr(0,prefix),q.artist) ||
       all.compare(prefix,2,L", ")!=0)return {};
    std::wstring remaining=all.substr(prefix+2);
    ReplaceAll(remaining,L", ",L"; ");
    return q.artist+L"; "+remaining;
}
std::vector<std::wstring> LegacyArtistNames(const ClassicPathQuery& q) {
    std::vector<std::wstring> artists;
    AddUnique(artists,q.artist);
    AddUnique(artists,q.all_artists);
    AddUnique(artists,PublishedSemicolonArtists(q));

    auto add_normalized=[&](std::wstring value) {
        if(value.empty())return;
        ReplaceAll(value,L" / ",L", ");
        AddUnique(artists,value);
    };
    add_normalized(q.artist);
    add_normalized(q.all_artists);

    // Old Sprinkles generated {all_artist_names} by replacing every slash with
    // ", " before path escaping. Keep that exact historical form too.
    auto add_all_artist_style=[&](std::wstring value) {
        if(value.empty())return;
        ReplaceAll(value,L"/",L", ");
        AddUnique(artists,value);
    };
    add_all_artist_style(q.artist);
    add_all_artist_style(q.all_artists);
    return artists;
}
}

std::wstring BuildClassicPathRegex(const ClassicPathQuery& q,
                                   const std::wstring& path_template,
                                   const std::wstring& output_extension,
                                   bool normalize_artist_separators,
                                   const std::wstring& invalid_char_replacement,
                                   bool any_audio_extension) {
    std::wstring pattern=path_template;
    if(pattern.empty()) {
        // Smart-layout fallback. Album/title are stable enough for status
        // detection; the artist directory stays permissive because media-session
        // album-artist text may differ from a playlist row's first artist.
        return L"^(?:Music\\\\)?Artists\\\\[^\\\\]+\\\\"+
            RegexEscape(EscapePathValue(q.album,L"-",L"Unknown Album"))+
            L"\\\\(?:\\d+ - )?"+
            RegexEscape(EscapePathValue(q.title,L"-",L"Untitled"))+
            L"\\.(?:"+ExtensionRegex(output_extension,any_audio_extension)+L")$";
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
                else if(token==L"{all_artist_names}") {
                    const auto exact=RegexEscape(EscapePathValue(all,invalid_char_replacement,L"Unknown Artist"));
                    auto published=PublishedSemicolonArtists(q);
                    if(normalize_artist_separators)ReplaceAll(published,L" / ",L", ");
                    if(published.empty()||SameInsensitive(published,all))escaped+=exact;
                    else escaped+=L"(?:"+exact+L"|"+
                        RegexEscape(EscapePathValue(published,invalid_char_replacement,L"Unknown Artist"))+L")";
                }
                else if(token==L"{album_name}")escaped+=RegexEscape(EscapePathValue(q.album,invalid_char_replacement,L"Unknown Album"));
                else if(token==L"{track_num}")escaped+=L"\\d+";
                else if(token==L"{track_num_2}")escaped+=L"\\d{2}";
                else if(token==L"{disc_num}")escaped+=L"\\d+";
                else if(token==L"{release_year}")escaped+=L"\\d{4}";
                else if(token==L"{release_date}")escaped+=L"\\d{4}(?:-\\d{2}(?:-\\d{2})?)?";
                else if(token==L"{multi_disc_paren}")escaped+=L"(?: \\(CD \\d+\\))?";
                else if(token==L"{playlist_name}"||token==L"{context_name}")escaped+=L"[^\\\\]+";
                else if(token==L"{context_index}")escaped+=L"\\d+";
                else if(token==L"{ext}")escaped+=ExtensionRegex(output_extension,any_audio_extension);
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
        escaped+=L"\\.(?:"+ExtensionRegex(output_extension,any_audio_extension)+L")";
    return L"^"+escaped+L"$";
}

bool ClassicPathMatches(const std::wstring& relative_path,
                        const ClassicPathQuery& query,
                        const std::wstring& path_template,
                        const std::wstring& output_extension,
                        bool normalize_artist_separators,
                        const std::wstring& invalid_char_replacement,
                        bool any_audio_extension) {
    try {
        return std::regex_match(relative_path,
            std::wregex(BuildClassicPathRegex(query,path_template,output_extension,
                                              normalize_artist_separators,invalid_char_replacement,
                                              any_audio_extension),
                        std::regex_constants::ECMAScript|std::regex_constants::icase));
    } catch(...) {
        return false;
    }
}

std::wstring BuildLegacySoggfyFlatRegex(const ClassicPathQuery& q) {
    if(q.title.empty())return L"(?!)";

    const std::array<std::wstring,4> replacement_modes={L"unicode",L"-",L"_",L""};
    std::vector<std::wstring> stems;
    const auto artists=LegacyArtistNames(q);
    for(const auto& mode:replacement_modes) {
        const auto title=EscapePathValue(q.title,mode,L"Untitled");
        for(const auto& artist:artists) {
            const auto escaped_artist=EscapePathValue(artist,mode,L"Unknown Artist");
            AddUnique(stems,escaped_artist+L" - "+title);
        }
    }
    if(stems.empty())return L"(?!)";

    std::wstring out=L"^(?:.*\\\\)?(?:";
    for(size_t i=0;i<stems.size();++i) {
        if(i)out+=L"|";
        out+=RegexEscape(stems[i]);
    }
    out+=L")\\.(?:mp3|m4a|mp4|ogg|opus|flac|aac|wav)$";
    return out;
}

bool ClassicLegacyFlatPathMatches(const std::wstring& relative_path,
                                  const ClassicPathQuery& query) {
    try {
        return std::regex_match(relative_path,
            std::wregex(BuildLegacySoggfyFlatRegex(query),
                        std::regex_constants::ECMAScript|std::regex_constants::icase));
    } catch(...) {
        return false;
    }
}
}
