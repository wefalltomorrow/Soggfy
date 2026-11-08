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
// Prototype only. Holds the default data stream against writes and its file
// identity/local ancestor names against rename. Alternate streams and file
// attributes/DACL are NOT immutable under these sharing flags. Filesystem metadata
// preservation is unfinished; this must not publish production recordings.
// The owner must validate payload and output first,
// and hold its settings-generation publication fence throughout Commit.
class MetadataRefreshTransaction {
    HANDLE source_=INVALID_HANDLE_VALUE;
    HANDLE staged_=INVALID_HANDLE_VALUE;
    std::vector<HANDLE> directories_;
    std::wstring path_,staging_path_;
    bool used_=false,published_=false,ready_=false;
public:
    MetadataRefreshTransaction()=default;
    ~MetadataRefreshTransaction();
    MetadataRefreshTransaction(const MetadataRefreshTransaction&)=delete;
    MetadataRefreshTransaction& operator=(const MetadataRefreshTransaction&)=delete;
    bool Open(const std::wstring& path);
    HANDLE Source()const{return source_;}
    bool Read(std::vector<uint8_t>& bytes)const;
    // Perform potentially slow disk work before acquiring a publication guard.
    bool Stage(const std::vector<uint8_t>& validated_output);
    // Only bounded namespace operations remain here. The caller holds its
    // publication guard and the callback must not acquire the settings lock.
    RefreshTransactionResult CommitStaged(
        const std::function<bool(RefreshTransactionStage)>& still_current);
    // Convenience for standalone transaction tests; production uses two phases.
    RefreshTransactionResult Commit(const std::vector<uint8_t>& validated_output,
        const std::function<bool(RefreshTransactionStage)>& still_current);
};
}
