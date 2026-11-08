#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "validation_protocol.h"
#include "validation_worker_trust.h"
namespace history {
// Successful pixel decoding does not establish provenance or permission to
// replace existing artwork. The caller retains the source lock throughout use.
bool ValidateArtworkProcess(HANDLE source,const std::wstring& worker,
    ValidationFormat format,ArtworkValidationReply& output,std::string& error,
    const std::function<bool()>& canceled={},DWORD timeout_ms=60000);
inline bool ValidateArtworkProcess(HANDLE source,const ValidationWorkerTrust& worker,
    ValidationFormat format,ArtworkValidationReply& output,std::string& error,
    const std::function<bool()>& canceled={},DWORD timeout_ms=60000) {
    return ValidateArtworkProcess(source,worker.Path(),format,output,error,canceled,timeout_ms);
}
// The caller retains an exclusively read-shared disk handle from inspection
// through publication. This function does not reopen paths or authorize writes.
// worker must be the trusted same-build absolute executable path.
bool ValidateRecordingProcess(HANDLE source,const std::wstring& worker,
    ValidationFormat format,ValidationReply& output,std::string& error,
    const std::function<bool()>& canceled={},DWORD timeout_ms=60000);
// The pinned executable remains held through child exit. Deployment must bind
// the digest to this DLL's build before opening the trust object.
inline bool ValidateRecordingProcess(HANDLE source,const ValidationWorkerTrust& worker,
    ValidationFormat format,ValidationReply& output,std::string& error,
    const std::function<bool()>& canceled={},DWORD timeout_ms=60000) {
    return ValidateRecordingProcess(source,worker.Path(),format,output,error,canceled,timeout_ms);
}
}
