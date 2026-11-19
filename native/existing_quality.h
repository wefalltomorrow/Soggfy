#pragma once
#include "library_layout.h"
#include "recording_identity.h"
#include <string>
namespace history {
bool ReadQuality(const std::wstring& file,Quality& out);
enum class SaveDecision { NewFile, Upgrade, Skip };
SaveDecision DecideSave(const std::wstring& path,const Quality& incoming);
RecordingIdentity ReadRecordingIdentity(const std::wstring& path);
#ifdef _WIN32
// Read identity from an already locked handle so callers can bind validation
// to the exact file object they will process, rather than reopen its path.
RecordingIdentity ReadRecordingIdentityFromHandle(void* file);
#endif
bool RecordingIdentityCompatible(const std::wstring& path,const std::string& uri,bool allow_legacy=true);
std::wstring ResolveRecordingPath(const std::wstring& root,const std::wstring& canonical,
    const std::string& uri,const std::wstring& output_extension);
}
