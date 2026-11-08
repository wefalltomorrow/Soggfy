#include "metadata_refresh_preparation.h"
#include "metadata_refresh_verification.h"
#include "validation_process.h"
namespace history {
MetadataPreparation PrepareMetadataRefresh(MetadataRefreshTransaction& transaction,
    const std::string& uri,const Tags& incoming,const std::wstring& worker,
    std::string& error,const std::function<bool()>& canceled) {
    error.clear();
    auto fail=[&](const char* message){error=message;return MetadataPreparation::Rejected;};
    auto stop=[&](){try{return canceled&&canceled();}catch(...){return true;}};
    if(stop())return fail("metadata refresh canceled");
    std::vector<uint8_t> source,output;
    if(!transaction.Read(source))return fail("metadata refresh source read failed");
    MetadataInspection inspected;
    if(!InspectMetadataRefreshSource(source,uri,inspected))
        return fail("metadata refresh source identity or structure rejected");
    Tags delta;
    const auto plan=PlanMetadataRefresh(inspected.comments,uri,incoming,delta);
    if(plan==MetadataPlanState::Rejected)return fail("metadata refresh delta rejected");
    if(stop())return fail("metadata refresh canceled");
    // A no-op does not authorize publication or claim the audio was decoded.
    if(plan==MetadataPlanState::Unchanged)return MetadataPreparation::Unchanged;
    const auto format=inspected.format==NativeRecordingFormat::Ogg?
        ValidationFormat::Vorbis:ValidationFormat::Flac;
    ValidationReply decoded;
    if(!ValidateRecordingProcess(transaction.Source(),worker,format,decoded,error,canceled))
        return MetadataPreparation::Rejected;
    if(!ValidReply(decoded,format)||decoded.samples!=inspected.samples||
       decoded.rate!=inspected.rate||decoded.channels!=inspected.channels||decoded.bits!=inspected.bits)
        return fail("metadata refresh decoded coverage disagrees with source");
    if(stop())return fail("metadata refresh canceled");
    const auto tagger=format==ValidationFormat::Vorbis?TagOgg:TagFlac;
    if(!tagger(source,delta,output,error))return MetadataPreparation::Rejected;
    if(output.size()>ValidationInputLimit||!VerifyMetadataRefreshOutput(source,uri,incoming,output))
        return fail("metadata refresh output verification failed");
    if(stop())return fail("metadata refresh canceled");
    if(!transaction.Stage(output))return fail("metadata refresh staging failed");
    // Publication must recheck cancellation after staging under its own fence.
    return MetadataPreparation::Staged;
}
}
