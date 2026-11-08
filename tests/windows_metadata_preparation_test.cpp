#include "../native/metadata_refresh_preparation.h"
#include "../native/metadata_refresh_inspection.h"
#include "../native/validation_protocol.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
using namespace history;
using Bytes=std::vector<uint8_t>;
static Bytes Read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int wmain(int argc,wchar_t** argv) {
    if(argc==5) {
        // Well-formed but incorrect coverage must not authorize staging.
        ValidationReply reply;reply.format=uint32_t(std::wcstoul(argv[4],nullptr,10));
        reply.samples=1;reply.units=1;reply.rate=44100;reply.channels=2;
        reply.bits=reply.format==uint32_t(ValidationFormat::Flac)?16:0;
        HANDLE pipe=reinterpret_cast<HANDLE>(uintptr_t(_wcstoui64(argv[2],nullptr,10)));
        DWORD written=0;
        return WriteFile(pipe,&reply,sizeof(reply),&written,nullptr)&&written==sizeof(reply)?0:1;
    }
    assert(argc==4);
    ValidationWorkerTrust trusted;std::string trust_error;
    assert(trusted.OpenBuildPinned(argv[3],trust_error));
    const fs::path original=argv[1],expected=argv[2];
    const auto before=Read(original),after=Read(expected);
    wchar_t temporary[32768]{},self[32768]{};
    assert(GetTempPathW(32768,temporary)&&GetModuleFileNameW(nullptr,self,32768));
    const fs::path root=fs::path(temporary)/(L"soggfy-prepare-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    assert(fs::create_directory(root));const auto path=root/L"recording";
    const std::string uri="spotify:track:0123456789abcdefghijkl";
    Tags incoming{{{"SPOTIFY_URI",uri},{"TITLE","replace?"},{"ARTIST","replace?"},
        {"DATE","2024-02-29"},{"YEAR","2024"},{"GENRE","new genre"}}, {1,2,3},"image/png"};
    // Successful staging, explicit fixture publication, no-op without a worker,
    // bad identity, unavailable helper, mismatched coverage, and cancellation.
    for(unsigned scenario=0;scenario<8;++scenario) {
        assert(fs::copy_file(scenario==2?expected:original,path));
        {
            MetadataRefreshTransaction transaction;assert(transaction.Open(path.wstring()));
            std::string error="stale";unsigned calls=0;
            const auto state=PrepareMetadataRefresh(transaction,scenario==3?"spotify:track:abcdefghijkl0123456789":uri,
                incoming,scenario==4||scenario==2?L"missing.exe":scenario==5?self:trusted.Path().c_str(),error,[&] {
                    ++calls;
                    if(scenario==6)return true;
                    if(scenario==7)throw 1;
                    return false;
                });
            if(scenario<2) {
                assert(state==MetadataPreparation::Staged&&error.empty());
                // Read through the held source: its pathname is unchanged.
                assert(fs::file_size(path)==before.size());
                if(scenario==1)assert(transaction.CommitStaged([](RefreshTransactionStage){return true;}).committed);
            } else if(scenario==2) {
                assert(state==MetadataPreparation::Unchanged&&error.empty());
                assert(!transaction.CommitStaged([](RefreshTransactionStage){return true;}).committed);
            } else {
                assert(state==MetadataPreparation::Rejected&&!error.empty());
                assert(!transaction.CommitStaged([](RefreshTransactionStage){return true;}).committed);
                if(scenario==5)assert(error=="metadata refresh decoded coverage disagrees with source");
            }
        }
        assert(Read(path)==(scenario==1||scenario==2?after:before));
        assert(fs::remove(path)&&fs::is_empty(root));
    }
    assert(fs::remove(root));
    std::cout<<"PASS: held-source decode, coverage agreement, verified staging, no-op, cancellation and cleanup\n";
    return 0;
}
