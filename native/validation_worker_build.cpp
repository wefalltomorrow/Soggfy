#include "validation_worker_trust.h"
#include <validation_worker_digest.h>
namespace history {
bool ValidationWorkerTrust::OpenBuildPinned(const std::wstring& path,std::string& error) {
    return Open(path,ValidationWorkerBuildSha256,error);
}
}
