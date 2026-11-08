#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace history {
// Offline deployment foundation. The digest must come from the trusted build,
// never from a sidecar writable alongside the helper or from the file itself.
// Keep this object alive throughout validation. It pins the checked bytes and
// ancestor names; Path is exposed only after a matching SHA-256 and identity
// check. It does not authenticate DLL dependencies or confer a sandbox.
class ValidationWorkerTrust {
    HANDLE file_=INVALID_HANDLE_VALUE;
    std::vector<HANDLE> directories_;
    std::wstring path_;
    bool attempted_=false;
public:
    ValidationWorkerTrust()=default;
    ~ValidationWorkerTrust();
    ValidationWorkerTrust(const ValidationWorkerTrust&)=delete;
    ValidationWorkerTrust& operator=(const ValidationWorkerTrust&)=delete;
    bool Open(const std::wstring& path,const std::string& build_sha256,std::string& error);
    const std::wstring& Path()const{return path_;}
};
}
