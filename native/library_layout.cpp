#include "library_layout.h"
#include <cwctype>
#include <cstdio>
#include <vector>
namespace history {
static bool Invalid(wchar_t c) {
    return c<32 || c==L'<' || c==L'>' || c==L':' || c==L'"' ||
           c==L'/' || c==L'\\' || c==L'|' || c==L'?' || c==L'*';
}
static bool ReservedName(const std::wstring& s) {
    std::wstring base=s.substr(0,s.find(L'.'));
    for(wchar_t& c:base)c=wchar_t(towupper(c));
    return base==L"CON" || base==L"PRN" || base==L"AUX" || base==L"NUL" ||
        (base.size()==4 && (base.substr(0,3)==L"COM" || base.substr(0,3)==L"LPT") &&
         base[3]>=L'1' && base[3]<=L'9');
}
static std::wstring Safe(std::wstring s,const wchar_t* fallback) {
    for(wchar_t& c:s) if(Invalid(c)) c=L'-';
    while(!s.empty() && (s.back()==L'.' || s.back()==L' ')) s.pop_back();
    if(s.empty()) s=fallback;
    if(s.size()>100) s.resize(100);
    if(ReservedName(s)) s=L"_"+s;
    return s;
}
static std::wstring UnicodeSafe(std::wstring s,const wchar_t* fallback) {
    for(wchar_t& c:s) {
        if(c<32) c=L'-';
        else switch(c) {
            case L'\\': c=L'\uFF3C'; break;
            case L'/': c=L'\uFF0F'; break;
            case L':': c=L'\uFF1A'; break;
            case L'*': c=L'\uFF0A'; break;
            case L'?': c=L'\uFF1F'; break;
            case L'"': c=L'\uFF02'; break;
            case L'<': c=L'\uFF1C'; break;
            case L'>': c=L'\uFF1E'; break;
            case L'|': c=L'\uFFE4'; break;
        }
    }
    for(size_t i=0;i<s.size() && s[i]==L' ';++i)s[i]=L'\u2002';
    for(size_t i=s.size();i && s[i-1]==L' ';--i)s[i-1]=L'\u2002';
    for(size_t i=s.size();i && s[i-1]==L'.';--i)s[i-1]=L'\uFF0E';
    if(s.empty())s=fallback;
    if(s.size()>120)s.resize(120);
    if(ReservedName(s))s+=L'\u2002';
    return s;
}
static void ReplaceAll(std::wstring& text,const std::wstring& from,const std::wstring& to) {
    if(from.empty())return;
    for(size_t at=0;(at=text.find(from,at))!=std::wstring::npos;at+=to.size())text.replace(at,from.size(),to);
}
static std::wstring Number(unsigned value,unsigned width=0) {
    if(!value)return {};
    wchar_t buffer[32];
    if(width)swprintf(buffer,32,L"%0*u",int(width),value);
    else swprintf(buffer,32,L"%u",value);
    return buffer;
}
static std::wstring NormalizeArtists(std::wstring value,bool enabled) {
    if(enabled)ReplaceAll(value,L" / ",L", ");
    return value;
}
static std::wstring RenderSegment(std::wstring part,const Catalog& c,const std::wstring& extension,
                                  bool normalize_artist_separators) {
    std::wstring ext=extension;
    if(!ext.empty() && ext.front()==L'.')ext.erase(ext.begin());
    std::wstring artist=c.album_artist.empty()?c.artist:c.album_artist;
    std::wstring all=NormalizeArtists(c.all_artists.empty()?c.artist:c.all_artists,normalize_artist_separators);
    ReplaceAll(part,L"{track_name}",UnicodeSafe(c.title,L"Untitled"));
    ReplaceAll(part,L"{artist_name}",UnicodeSafe(artist,L"Unknown Artist"));
    ReplaceAll(part,L"{all_artist_names}",UnicodeSafe(all,L"Unknown Artist"));
    ReplaceAll(part,L"{album_name}",UnicodeSafe(c.album,L"Unknown Album"));
    ReplaceAll(part,L"{track_num}",Number(c.track));
    ReplaceAll(part,L"{track_num_2}",Number(c.track,2));
    ReplaceAll(part,L"{disc_num}",Number(c.disc));
    ReplaceAll(part,L"{release_year}",Number(c.release_year));
    ReplaceAll(part,L"{multi_disc_paren}",c.total_discs>1&&c.disc?L" (CD "+Number(c.disc)+L")":L"");
    ReplaceAll(part,L"{ext}",UnicodeSafe(ext,L"bin"));
    return UnicodeSafe(part,L"_");
}
static bool EndsWithInsensitive(const std::wstring& value,const std::wstring& suffix) {
    if(suffix.size()>value.size())return false;
    size_t off=value.size()-suffix.size();
    for(size_t i=0;i<suffix.size();++i)
        if(towlower(value[off+i])!=towlower(suffix[i]))return false;
    return true;
}
std::wstring RelativePath(const Catalog& c,const std::wstring& extension) {
    std::vector<std::wstring> parts;
    auto album=Safe(c.album,L"Unknown Album");
    switch(c.kind) {
        case MediaKind::Music: parts={L"Music",L"Artists",Safe(c.artist,L"Unknown Artist"),album}; break;
        case MediaKind::Compilation: parts={L"Music",L"Compilations",album}; break;
        case MediaKind::Soundtrack: parts={L"Music",L"Soundtracks",album}; break;
        case MediaKind::VariousArtists: parts={L"Music",L"Various Artists",album}; break;
        case MediaKind::Audiobook:
            parts={L"Audiobooks",Safe(c.author.empty()?c.artist:c.author,L"Unknown Author")};
            if(!c.series.empty()) parts.push_back(Safe(c.series,L"Series"));
            parts.push_back(Safe(c.book.empty()?c.album:c.book,L"Unknown Book")); break;
        case MediaKind::Podcast:
            parts={L"Podcasts",Safe(c.show.empty()?c.album:c.show,L"Unknown Podcast")};
            if(c.episode_year) parts.push_back(std::to_wstring(c.episode_year));
            break;
        case MediaKind::MusicVideo: parts={L"Music Videos"}; break;
        case MediaKind::Concert: parts={L"Concerts"}; break;
    }
    std::wstring out;
    for(const auto& part:parts) { out+=part; out+=L'\\'; }
    if(c.track) { wchar_t number[20]; swprintf(number,20,L"%02u - ",c.track); out+=number; }
    return out+Safe(c.title,L"Untitled")+extension;
}
std::wstring RelativePathTemplate(const Catalog& c,const std::wstring& extension,
                                  const std::wstring& pattern,bool normalize_artist_separators) {
    if(pattern.empty())return {};
    std::wstring expanded=pattern;
    ReplaceAll(expanded,L"{multi_disc_path}",
               c.total_discs>1&&c.disc?L"\\CD "+Number(c.disc):L"");
    std::vector<std::wstring> parts;
    size_t start=0;
    for(size_t i=0;i<=expanded.size();++i) {
        if(i==expanded.size() || expanded[i]==L'\\' || expanded[i]==L'/') {
            if(i>start) {
                auto raw=expanded.substr(start,i-start);
                if(raw==L"." || raw==L"..")return {};
                parts.push_back(RenderSegment(raw,c,extension,normalize_artist_separators));
            }
            start=i+1;
        }
    }
    if(parts.empty())return {};
    std::wstring out;
    for(size_t i=0;i<parts.size();++i) {
        if(i)out+=L'\\';
        out+=parts[i];
    }
    if(!extension.empty() && !EndsWithInsensitive(out,extension))out+=extension;
    return out;
}
std::wstring OutputPath(const std::wstring& root,const Catalog& item,const std::wstring& extension,
                        bool music_folder,const std::wstring& pattern,bool normalize_artist_separators) {
    auto relative=pattern.empty()?RelativePath(item,extension):
        RelativePathTemplate(item,extension,pattern,normalize_artist_separators);
    if(relative.empty())relative=RelativePath(item,extension);
    if(pattern.empty() && music_folder && relative.rfind(L"Music\\",0)==0) relative.erase(0,6);
    auto base=root;
    while(!base.empty() && (base.back()==L'\\' || base.back()==L'/')) base.pop_back();
    return base+L"\\"+relative;
}
bool HigherQuality(const Quality& incoming,const Quality& existing) {
    if(!incoming.rate || !incoming.channels) return false;
    if(incoming.codec!=existing.codec) return incoming.codec==Codec::Flac && incoming.bits>=16 && incoming.channels==existing.channels && incoming.rate>=existing.rate;
    if(incoming.channels!=existing.channels || !existing.rate) return false;
    if(incoming.codec==Codec::Vorbis) return incoming.bitrate && existing.bitrate &&
        incoming.bitrate>existing.bitrate && incoming.rate>=existing.rate;
    return incoming.bits && existing.bits && incoming.bits>=existing.bits && incoming.rate>=existing.rate &&
        (incoming.bits>existing.bits || incoming.rate>existing.rate);
}
}
