#pragma once
#include "metadata_refresh_transaction.h"
#include "ogg_tags.h"
#include "validation_worker_trust.h"
namespace history {
enum class MetadataPreparation { Rejected, Unchanged, Staged };
// Offline prototype. The caller opens and retains the transaction throughout.
// worker must be the trusted same-build helper. This only stages verified text
// changes; publication still requires the settings guard and a filesystem
// preservation policy. Existing artwork is retained, incoming artwork ignored.
MetadataPreparation PrepareMetadataRefresh(MetadataRefreshTransaction& transaction,
    const std::string& uri,const Tags& incoming,const std::wstring& worker,
    std::string& error,const std::function<bool()>& canceled={});
inline MetadataPreparation PrepareMetadataRefresh(MetadataRefreshTransaction& transaction,
    const std::string& uri,const Tags& incoming,const ValidationWorkerTrust& worker,
    std::string& error,const std::function<bool()>& canceled={}) {
    return PrepareMetadataRefresh(transaction,uri,incoming,worker.Path(),error,canceled);
}
}
