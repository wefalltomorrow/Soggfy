#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>
namespace history {
enum class RefreshTransactionStage { Prepared,SourceMoved };
struct RefreshTransactionResult {
    bool committed=false;
    std::wstring retained_original;
    std::string error;
};
// Prototype only. Holds the original identity and local ancestor directories
// against modification/rename. The owner must validate payload and output first,
// and hold its settings-generation publication fence throughout Commit.
class MetadataRefreshTransaction {
    HANDLE source_=INVALID_HANDLE_VALUE;
    std::vector<HANDLE> directories_;
    std::wstring path_;
    bool used_=false;
public:
    MetadataRefreshTransaction()=default;
    ~MetadataRefreshTransaction();
    MetadataRefreshTransaction(const MetadataRefreshTransaction&)=delete;
    MetadataRefreshTransaction& operator=(const MetadataRefreshTransaction&)=delete;
    bool Open(const std::wstring& path);
    HANDLE Source()const{return source_;}
    bool Read(std::vector<uint8_t>& bytes)const;
    RefreshTransactionResult Commit(const std::vector<uint8_t>& validated_output,
        const std::function<bool(RefreshTransactionStage)>& still_current);
};
}
