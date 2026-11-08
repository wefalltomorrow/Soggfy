#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "validation_protocol.h"
namespace history {
// The caller retains an exclusively read-shared disk handle from inspection
// through publication. This function does not reopen paths or authorize writes.
// worker must be the trusted same-build absolute executable path.
bool ValidateRecordingProcess(HANDLE source,const std::wstring& worker,
    ValidationFormat format,ValidationReply& output,std::string& error,
    const std::function<bool()>& canceled={},DWORD timeout_ms=60000);
}
