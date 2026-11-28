// Synthetic PE imports and a disposable INI only; never loads Spotify.
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#include "../native/version_proxy.cpp"
#include "../native/history_settings.cpp"
#include <cassert>
#include <vector>
void StartToDiskMenu(HMODULE){}
void StartAudioHistory(HMODULE,HMODULE){}
namespace history {
void StartMetadataCollector(HMODULE){}
void StartCefRequestFilter(HMODULE){}
void StartPlaybackSpeed(HMODULE){}
void StartLogger(){}
void QueueDiagnostic(const char*){}
void LogActivity(const char*,const std::string&){}
}
namespace {
template<class T> void Put(std::vector<BYTE>& image,size_t offset,T value) {
    assert(offset+sizeof(value)<=image.size());memcpy(image.data()+offset,&value,sizeof(value));
}
void Text(std::vector<BYTE>& image,size_t offset,const char* text) {
    assert(offset+strlen(text)+1<=image.size());memcpy(image.data()+offset,text,strlen(text)+1);
}
// PE layout adapted from the pinned Floggfy network-patch regression.
std::vector<BYTE> Image() {
    std::vector<BYTE> image(4096);
    Put<uint16_t>(image,0,IMAGE_DOS_SIGNATURE);Put<uint32_t>(image,0x3c,0x80);
    Put<uint32_t>(image,0x80,IMAGE_NT_SIGNATURE);Put<uint16_t>(image,0x84,IMAGE_FILE_MACHINE_AMD64);
    Put<uint16_t>(image,0x94,240);Put<uint16_t>(image,0x98,IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    Put<uint32_t>(image,0x98+56,4096);Put<uint32_t>(image,0x98+108,16);
    Put<uint32_t>(image,0x98+120,0x200);Put<uint32_t>(image,0x98+124,40);
    Put<uint32_t>(image,0x200,0x300);Put<uint32_t>(image,0x20c,0x280);Put<uint32_t>(image,0x210,0x380);
    Text(image,0x280,"ole32.dll");Put<uint64_t>(image,0x300,0x400);Put<uint64_t>(image,0x380,0x11111111);
    Text(image,0x402,"CoCreateInstance");
    Put<uint32_t>(image,0x98+112+13*8,0x500);Put<uint32_t>(image,0x98+116+13*8,64);
    Put<uint32_t>(image,0x500,1);Put<uint32_t>(image,0x504,0x580);Put<uint32_t>(image,0x50c,0x680);Put<uint32_t>(image,0x510,0x600);
    Text(image,0x580,"ole32.dll");Put<uint64_t>(image,0x600,0x700);Put<uint64_t>(image,0x680,0x700);
    Text(image,0x702,"CoCreateInstance");return image;
}
void Imports() {
    auto image=Image();const auto original=image;const auto module=reinterpret_cast<HMODULE>(image.data());
    assert(!history::Settings{}.network_patch);
    StartConnectivityHook(reinterpret_cast<HMODULE>(1),false);
    StartConnectivityHook(module,false);
    assert(image==original&&connectivity_init.State()==hooks::InitState::Waiting&&table_count==0);
    StartConnectivityHook(module,true);
    assert(connectivity_init.State()==hooks::InitState::Active);
    void* replacement=reinterpret_cast<void*>(RepairCoCreateInstance);
    assert(*reinterpret_cast<void**>(image.data()+0x380)==replacement);
    assert(*reinterpret_cast<void**>(image.data()+0x680)==reinterpret_cast<void*>(0x700));
    // Preserve our loader guard: an unresolved delay thunk is never patched.
    *reinterpret_cast<void**>(image.data()+0x680)=reinterpret_cast<void*>(CoCreateInstance);
    StartConnectivityHook(module,true);
    assert(*reinterpret_cast<void**>(image.data()+0x680)==replacement);
    *reinterpret_cast<void**>(image.data()+0x680)=reinterpret_cast<void*>(CoCreateInstance);
    StartConnectivityHook(reinterpret_cast<HMODULE>(1),false);
    StartConnectivityHook(module,false); // even active state must bypass discovery
    assert(*reinterpret_cast<void**>(image.data()+0x680)==reinterpret_cast<void*>(CoCreateInstance));
    StartConnectivityHook(module,true);
    assert(*reinterpret_cast<void**>(image.data()+0x680)==replacement);
}
void Settings() {
    wchar_t executable[32768];assert(GetModuleFileNameW(nullptr,executable,32768));
    const std::wstring exe=executable,path=exe.substr(0,exe.find_last_of(wchar_t(92)))+wchar_t(92)+L"SpotifyHistory.ini";
    // The runner gives this test its own directory. Never overwrite an existing INI.
    assert(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND);
    assert(history::CreateDefaultIni(path));
    assert(GetPrivateProfileIntW(L"To Disk",L"NetworkPatch",9,path.c_str())==0);
    assert(!history::ReadNetworkPatch(path));
    unsigned fixture=0;
    const auto check_bytes=[&](const std::vector<unsigned char>& bytes,bool expected) {
        const auto name=path+L".parser-"+std::to_wstring(++fixture);
        HANDLE file=CreateFileW(name.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        assert(file!=INVALID_HANDLE_VALUE);
        DWORD written=0;
        assert(WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr));
        assert(written==bytes.size());CloseHandle(file);
        assert(history::ReadNetworkPatch(name)==expected);assert(DeleteFileW(name.c_str()));
    };
    const auto check=[&](const std::wstring& body,bool expected) {
        const auto unicode=L"\xfeff"+body;
        const auto begin=reinterpret_cast<const unsigned char*>(unicode.data());
        check_bytes({begin,begin+unicode.size()*sizeof(wchar_t)},expected);
    };
    check(L"[tO dIsK]\nnetworkpatch= 1 \nOther=\u97f3\u697d\n",true);
    check(L"[To Disk]\nNetworkPatch=1\nNetworkPatch=0\n",false);
    check(L"[To Disk]\nNetworkPatch=1\nnetworkpatch=1\n",false);
    check(L"[Other]\nNetworkPatch=1\n",false);
    check(L"[To Disk]\nNetworkPatch=1\nOther="+std::wstring(40000,L'x')+L"\n",false);
    check(L"[To Disk]\nNetworkPatch=\"1\"\n",false);
    check(L"[To Disk]\nNetworkPatch=1\n[Other]\nX=2\n[To Disk]\nNetworkPatch=0\n",false);
    check(L"[To Disk]\nNetworkPatch=1\nOther="+std::wstring(20000,L'x')+L"\nNetworkPatch=0\n",false);
    check(L"[To Disk]\nNetworkPatch=1\n"+std::wstring(1,wchar_t(0)),false);
    check_bytes({0xff,0xfe,0x41},false);
    check_bytes({0xfe,0xff,0x00,0x41},false);
    const std::string utf8="\xef\xbb\xbf[To Disk]\nNetworkPatch=1\nOther=\xe9\x9f\xb3\xe6\xa5\xbd\n";
    check_bytes({utf8.begin(),utf8.end()},true);
    auto invalid_utf8=std::vector<unsigned char>(utf8.begin(),utf8.end());invalid_utf8.push_back(0xff);
    check_bytes(invalid_utf8,false);
    for(const auto* value:{L"0",L"-1",L"2",L"01",L"true",L"1junk",L"",L"1;comment"}) {
        assert(WritePrivateProfileStringW(L"To Disk",L"NetworkPatch",value,path.c_str()));
        assert(!history::ReadNetworkPatch(path));
    }
    assert(WritePrivateProfileStringW(L"To Disk",L"NetworkPatch",nullptr,path.c_str()));
    assert(!history::ReadNetworkPatch(path));
    assert(WritePrivateProfileStringW(L"To Disk",L"NetworkPatch",L"1",path.c_str()));
    assert(history::ReadNetworkPatch(path));
    assert(WritePrivateProfileStringW(L"To Disk",L"Downloads",L"1",path.c_str()));
    history::InitSettings(nullptr);
    assert(history::GetSettings().network_patch&&history::GetSettings().downloads);
    // An external restart-only edit survives an unrelated Classic settings save.
    assert(WritePrivateProfileStringW(L"To Disk",L"NetworkPatch",L"0",path.c_str()));
    const auto epoch=history::CaptureEpoch();
    assert(history::SetEmbedLyrics(false)&&history::FlushSettings(5000));
    assert(history::GetSettings().network_patch&&history::GetSettings().downloads);
    assert(history::CaptureEpoch()==epoch&&!history::ReadNetworkPatch(path));
    assert(DeleteFileW(path.c_str()));
}
}
int main() {
    Imports();Settings();
    puts("PASS: default-off network opt-in, strict values, independent capture, persistence, disabled discovery and guarded IAT mutation");
}
