#pragma once
#include "history_settings.h"
#include <string>
#include <vector>
#include <functional>

namespace history {
struct PostProcessResult {
    enum class State { Native,Converted,Skipped,Failed } state=State::Native;
    std::wstring path;
    std::string error;
};
struct PostProcessControl {
    std::function<bool()> canceled;
    // Zero selects a conservative size-based deadline, shared by all stages.
    unsigned timeout_ms=0;
};
bool ConversionRequested(const Settings& settings,const std::wstring& native_path);
PostProcessResult PostProcessPublishedFile(const std::wstring& native_path,const Settings& settings,
                                           const std::vector<unsigned char>& cover,
                                           const std::wstring& cover_extension,const PostProcessControl& control={});
// Retry only from a locked, identified and fully decoded existing native file.
// Existing files are always retained, independently of the new-capture option.
PostProcessResult RetryPostProcessExistingFile(const std::wstring& native_path,const Settings& settings,
                                              const PostProcessControl& control={});
}
