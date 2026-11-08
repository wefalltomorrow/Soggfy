#include "../native/metadata_refresh_transaction.h"
// Initialize the real setters without launching persistence or touching user INI files.
#include "../native/history_settings.cpp"
#include <cassert>
#include <iostream>
#include <filesystem>
namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
void Create(const std::wstring& path,const Bytes& bytes) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    assert(file!=INVALID_HANDLE_VALUE);DWORD written=0;
    assert(WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size());CloseHandle(file);
}
Bytes Read(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(file!=INVALID_HANDLE_VALUE);
    Bytes bytes(GetFileSize(file,nullptr));DWORD count=0;
    assert(ReadFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr)&&count==bytes.size());CloseHandle(file);return bytes;
}
int main() {
    wchar_t temporary[32768]{};assert(GetTempPathW(32768,temporary));
    const std::wstring root=std::wstring(temporary)+L"soggfy-refresh-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    assert(CreateDirectoryW(root.c_str(),nullptr));
    const auto path=root+L"\\recording.flac";
    const Bytes old={'o','l','d'},updated={'n','e','w'},other={'o','t','h','e','r'};
    auto current=[](history::RefreshTransactionStage){return true;};
    Create(path,old);
    {
        // A mapped writer can outlive its original file handle. It must still
        // prevent the transaction from claiming a stable source snapshot.
        HANDLE writer=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        assert(writer!=INVALID_HANDLE_VALUE);
        HANDLE mapping=CreateFileMappingW(writer,nullptr,PAGE_READWRITE,0,0,nullptr);assert(mapping);
        auto view=MapViewOfFile(mapping,FILE_MAP_WRITE,0,0,old.size());assert(view);
        CloseHandle(writer);
        {history::MetadataRefreshTransaction transaction;assert(!transaction.Open(path));}
        assert(UnmapViewOfFile(view));CloseHandle(mapping);
    }
    {
        history::MetadataRefreshTransaction transaction;assert(transaction.Open(path));
        Bytes bytes;assert(transaction.Read(bytes)&&bytes==old);
        assert(!DeleteFileW(path.c_str()));
        assert(!MoveFileW(root.c_str(),(root+L"-moved").c_str()));
        HANDLE writer=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        assert(writer==INVALID_HANDLE_VALUE);
        auto result=transaction.Commit(updated,current);
        if(!result.committed)std::cerr<<result.error<<" error="<<GetLastError()<<"\n";
        assert(result.committed&&result.retained_original.empty());
        assert(!transaction.Commit(old,current).committed);
    }
    assert(Read(path)==updated);assert(DeleteFileW(path.c_str()));
    for(unsigned cancellation=0;cancellation<3;++cancellation) {
        Create(path,old);
        {
            history::MetadataRefreshTransaction transaction;assert(transaction.Open(path));unsigned calls=0;
            auto result=transaction.Commit(updated,[&](history::RefreshTransactionStage stage) {
                if(cancellation==0)return false;
                if(cancellation==1)return ++calls<2;
                if(stage==history::RefreshTransactionStage::SourceMoved)throw 1;
                return true;
            });
            assert(!result.committed&&result.retained_original.empty());
        }
        assert(Read(path)==old);assert(DeleteFileW(path.c_str()));
        assert(fs::is_empty(root));
    }
    Create(path,old);std::wstring retained;
    {
        history::MetadataRefreshTransaction transaction;assert(transaction.Open(path));
        auto result=transaction.Commit(updated,[&](history::RefreshTransactionStage stage) {
            if(stage==history::RefreshTransactionStage::SourceMoved)Create(path,other);
            return true;
        });
        assert(!result.committed&&!result.retained_original.empty());retained=result.retained_original;
    }
    assert(Read(path)==other&&Read(retained)==old);
    assert(DeleteFileW(path.c_str())&&DeleteFileW(retained.c_str()));
    Create(path,old);
    const auto alias=root+L"\\alias.flac";assert(CreateHardLinkW(alias.c_str(),path.c_str(),nullptr));
    {history::MetadataRefreshTransaction transaction;assert(!transaction.Open(path));assert(!transaction.Commit(updated,current).committed);}
    assert(Read(path)==old);assert(DeleteFileW(alias.c_str())&&DeleteFileW(path.c_str()));
    history::settings_signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);assert(history::settings_signal);
    assert(history::SetDownloads(true)&&history::SetMetadata(true));
    for(bool stale:{true,false}) {
        Create(path,old);
        {
            const auto generation=history::GetSettings().generation;
            history::MetadataRefreshTransaction transaction;assert(transaction.Open(path));
            assert(!transaction.CommitStaged(current).committed);
            assert(transaction.Stage(updated));
            assert(!transaction.Stage(updated));
            assert(Read(path)==old); // staging is invisible at the original name
            if(stale)assert(history::SetEmbedLyrics(!history::GetSettings().embed_lyrics));
            history::MetadataPublicationGuard guard(generation);
            assert(bool(guard)!=stale);
            if(guard)assert(transaction.CommitStaged(current).committed);
        }
        assert(Read(path)==(stale?old:updated));assert(DeleteFileW(path.c_str()));
        assert(fs::is_empty(root));
    }
    CloseHandle(history::settings_signal);history::settings_signal=nullptr;
    assert(fs::is_empty(root));assert(RemoveDirectoryW(root.c_str()));
    std::cout<<"PASS: identity-held refresh, ancestor locks, cancellation, concurrent creation, rollback and retained originals\n";
}
