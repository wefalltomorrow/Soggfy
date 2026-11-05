#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace history {
enum class MediaKind { Music, Compilation, Soundtrack, VariousArtists, Audiobook, Podcast, MusicVideo, Concert };
struct Catalog {
    MediaKind kind=MediaKind::Music;
    std::wstring artist,album_artist,all_artists,album,title,release_date,author,series,book,show;
    std::string spotify_uri,spotify_album_uri;
    unsigned track=0,disc=0,total_discs=0,release_year=0,episode_year=0;
};
std::wstring EscapePathValue(const std::wstring& value,const std::wstring& replacement_mode=L"unicode",
                             const wchar_t* fallback=L"_");
std::wstring RelativePath(const Catalog& item,const std::wstring& extension);
std::wstring RelativePathTemplate(const Catalog& item,const std::wstring& extension,
                                  const std::wstring& pattern,bool normalize_artist_separators=true,
                                  const std::wstring& invalid_char_replacement=L"unicode");
std::wstring OutputPath(const std::wstring& root,const Catalog& item,const std::wstring& extension,
                        bool music_folder=false,const std::wstring& pattern=L"",
                        bool normalize_artist_separators=true,
                        const std::wstring& invalid_char_replacement=L"unicode");
enum class Codec { Vorbis, Flac };
struct Quality { Codec codec=Codec::Vorbis; unsigned rate=0,channels=0,bits=0,bitrate=0; };
bool HigherQuality(const Quality& incoming,const Quality& existing);
// Derive from the configured canonical path, preserving custom hierarchy.
std::vector<std::wstring> DuplicatePaths(const std::wstring& root,const std::wstring& canonical,const std::string& uri);
}
