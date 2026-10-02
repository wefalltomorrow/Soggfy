#pragma once
#include <string>

namespace history {
struct ClassicPathQuery {
    std::wstring title,artist,album,all_artists;
};
std::wstring BuildClassicPathRegex(const ClassicPathQuery& query,
                                   const std::wstring& path_template,
                                   const std::wstring& output_extension,
                                   bool normalize_artist_separators,
                                   const std::wstring& invalid_char_replacement,
                                   bool any_audio_extension=false);
bool ClassicPathMatches(const std::wstring& relative_path,
                        const ClassicPathQuery& query,
                        const std::wstring& path_template,
                        const std::wstring& output_extension,
                        bool normalize_artist_separators,
                        const std::wstring& invalid_char_replacement,
                        bool any_audio_extension=false);
std::wstring BuildLegacySoggfyFlatRegex(const ClassicPathQuery& query);
bool ClassicLegacyFlatPathMatches(const std::wstring& relative_path,
                                  const ClassicPathQuery& query);
}
