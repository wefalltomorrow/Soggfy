#pragma once
#include <string>
#include <vector>

namespace history {
struct DurableClassicStatus {
    std::wstring title,artist,album,path;
    std::string status,message;
};

// A downloaded check is revalidated against the actual file during lookup.
// Only terminal states are persisted; IN_PROGRESS never survives a restart.
void PersistClassicTrackStatus(const std::wstring& root,const DurableClassicStatus& status);
std::vector<DurableClassicStatus> FindPersistedClassicTrackStatuses(
    const std::wstring& root,const std::wstring& title);
}
