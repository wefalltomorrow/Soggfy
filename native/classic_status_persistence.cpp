#define WIN32_LEAN_AND_MEAN
#include "classic_status_persistence.h"
#include "classic_status_journal.h"
#include "history_settings.h"
#include "media_session.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace history {
namespace {
constexpr std::uintmax_t kMaxJournalBytes=16ull*1024*1024;
constexpr std::size_t kMaxRecordLine=100000;
SRWLOCK journal_lock=SRWLOCK_INIT;
std::wstring loaded_root;
bool loaded=false;
std::unordered_map<std::wstring,std::vector<DurableClassicStatus>> by_title;

std::wstring Fold(std::wstring value) {
    for(auto& c:value)c=static_cast<wchar_t>(std::towlower(c));
    return value;
}
std::wstring JournalPath(const std::wstring& root) {
    if(root.empty())return {};
    const wchar_t separator=(root.back()==L'\\'||root.back()==L'/')?L'\0':L'\\';
    return separator?root+separator+L"SoggfyTrackStatus.tsv":root+L"SoggfyTrackStatus.tsv";
}
std::wstring FromUtf8(const std::string& value) {
    if(value.empty())return {};
    if(value.size()>32767)return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(n<=0)return {};
    std::wstring out(static_cast<std::size_t>(n),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n)!=n)return {};
    return out;
}
void Store(const DurableClassicStatus& item) {
    if(item.title.empty() || (item.status!="DONE"&&item.status!="ERROR"))return;
    auto& bucket=by_title[Fold(item.title)];
    for(auto& existing:bucket){
        if(Fold(existing.artist)==Fold(item.artist)&&Fold(existing.album)==Fold(item.album)){
            existing=item;return;
        }
    }
    bucket.push_back(item);
}
void Load(const std::wstring& root) {
    if(loaded&&loaded_root==root)return;
    loaded=true;loaded_root=root;by_title.clear();
    if(root.empty())return;
    std::ifstream file(std::filesystem::path(WindowsPath(JournalPath(root))),std::ios::binary);
    if(!file)return;
    std::string line;
    std::size_t bytes=0;
    while(std::getline(file,line)) {
        bytes+=line.size()+1;
        if(bytes>kMaxJournalBytes || line.size()>kMaxRecordLine)break;
        StatusJournalRow row{};
        if(!DecodeStatusJournalRow(line,row))continue;
        DurableClassicStatus status;
        status.title=FromUtf8(row.title);status.artist=FromUtf8(row.artist);
        status.album=FromUtf8(row.album);status.path=FromUtf8(row.path);
        status.status=std::move(row.status);status.message=std::move(row.message);
        Store(status);
    }
}
std::string Encode(const DurableClassicStatus& item) {
    return EncodeStatusJournalRow({
        Utf8(item.title),Utf8(item.artist),Utf8(item.album),
        item.status,Utf8(item.path),item.message
    });
}
void Compact(const std::wstring& path) {
    const auto temporary=path+L".new";
    std::ofstream output(std::filesystem::path(WindowsPath(temporary)),
                         std::ios::binary|std::ios::trunc);
    if(!output)return;
    for(const auto& [_,records]:by_title) {
        for(const auto& r:records){
            const auto line=Encode(r);
            if(!line.empty())output.write(line.data(),std::streamsize(line.size()));
        }
    }
    output.flush();const bool ok=bool(output);output.close();
    if(!ok||!MoveFileExW(WindowsPath(temporary).c_str(),WindowsPath(path).c_str(),
                         MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(WindowsPath(temporary).c_str());
        HistoryLog("persistent status journal compaction failed; previous file preserved");
    }
}
} // namespace

void PersistClassicTrackStatus(const std::wstring& root,const DurableClassicStatus& item) {
    if(root.empty()||item.title.empty()||(item.status!="DONE"&&item.status!="ERROR"))return;
    const auto line=Encode(item);
    if(line.empty())return;
    AcquireSRWLockExclusive(&journal_lock);
    Load(root);
    Store(item);
    if(EnsureDirectory(root)) {
        const auto path=JournalPath(root);
        std::error_code ec;
        const auto size=std::filesystem::file_size(std::filesystem::path(WindowsPath(path)),ec);
        if(!ec && size>=kMaxJournalBytes){
            Compact(path);
        } else {
            std::ofstream output(std::filesystem::path(WindowsPath(path)),
                                 std::ios::binary|std::ios::app);
            if(output) {
                output.write(line.data(),std::streamsize(line.size()));
                output.flush();
            }
        }
    }
    ReleaseSRWLockExclusive(&journal_lock);
}

std::vector<DurableClassicStatus> FindPersistedClassicTrackStatuses(
    const std::wstring& root,const std::wstring& title) {
    std::vector<DurableClassicStatus> result;
    if(root.empty()||title.empty())return result;
    AcquireSRWLockExclusive(&journal_lock);
    Load(root);
    const auto found=by_title.find(Fold(title));
    if(found!=by_title.end())result=found->second;
    ReleaseSRWLockExclusive(&journal_lock);
    return result;
}
}
