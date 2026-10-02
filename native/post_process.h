#pragma once
#include "history_settings.h"
#include <string>
#include <vector>

namespace history {
struct PostProcessResult {
    enum class State { Native,Converted,Skipped,Failed } state=State::Native;
    std::wstring path;
    std::string error;
};
bool ConversionRequested(const Settings& settings,const std::wstring& native_path);
PostProcessResult PostProcessPublishedFile(const std::wstring& native_path,const Settings& settings,
                                           const std::vector<unsigned char>& cover,
                                           const std::wstring& cover_extension);
}
