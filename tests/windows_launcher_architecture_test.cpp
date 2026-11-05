#include "../native/launcher_architecture.h"
#include "../native/startup_launcher.h"
#include <cassert>
#include <cstdio>
#include <vector>
static void Write(const std::wstring& path, WORD machine, WORD magic, bool truncated=false) {
    IMAGE_DOS_HEADER dos{}; dos.e_magic=IMAGE_DOS_SIGNATURE; dos.e_lfanew=sizeof(dos);
    IMAGE_FILE_HEADER header{};header.Machine=machine;header.NumberOfSections=1;
    header.SizeOfOptionalHeader=magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC?sizeof(IMAGE_OPTIONAL_HEADER32):sizeof(IMAGE_OPTIONAL_HEADER64);
    std::vector<unsigned char> bytes(sizeof(dos)+4+sizeof(header)+header.SizeOfOptionalHeader+sizeof(IMAGE_SECTION_HEADER));
    std::memcpy(bytes.data(),&dos,sizeof(dos));DWORD signature=IMAGE_NT_SIGNATURE;
    std::memcpy(bytes.data()+sizeof(dos),&signature,4);
    std::memcpy(bytes.data()+sizeof(dos)+4,&header,sizeof(header));
    std::memcpy(bytes.data()+sizeof(dos)+4+sizeof(header),&magic,2);
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
    assert(file!=INVALID_HANDLE_VALUE);DWORD written=0;
    const DWORD size=static_cast<DWORD>(bytes.size()-(truncated?1:0));
    assert(WriteFile(file,bytes.data(),size,&written,nullptr)&&written==size);CloseHandle(file);
}
int main() {
    wchar_t temp[MAX_PATH]{},name[MAX_PATH]{};assert(GetTempPathW(MAX_PATH,temp));
    assert(GetTempFileNameW(temp,L"sgf",0,name));assert(DeleteFileW(name));assert(CreateDirectoryW(name,nullptr));
    const std::wstring folder=name,exe=folder+L"\\Spotify.exe",dll=folder+L"\\Soggfy.dll";
    using startup::Architecture;
    Write(exe,IMAGE_FILE_MACHINE_AMD64,IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    assert(startup::InspectArchitecture(exe).architecture==Architecture::X64);
    Write(dll,IMAGE_FILE_MACHINE_I386,IMAGE_NT_OPTIONAL_HDR32_MAGIC);
    assert(startup::InspectArchitecture(dll).architecture==Architecture::X86);
    PROCESS_INFORMATION process{};std::wstring error;
    assert(!startup::Launch(folder,process,error));assert(error.find(L"32-bit (x86)")!=std::wstring::npos);
    assert(!process.hProcess&&!process.hThread);
    Write(dll,IMAGE_FILE_MACHINE_ARM64,IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    assert(startup::InspectArchitecture(dll).architecture==Architecture::Arm64);
    assert(!startup::RequireX64(dll,L"Soggfy.dll",error));assert(error.find(L"ARM64")!=std::wstring::npos);
    Write(dll,IMAGE_FILE_MACHINE_I386,IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    assert(startup::InspectArchitecture(dll).architecture==Architecture::Other);
    Write(dll,IMAGE_FILE_MACHINE_AMD64,IMAGE_NT_OPTIONAL_HDR64_MAGIC,true);
    assert(startup::InspectArchitecture(dll).architecture==Architecture::Invalid);
    assert(DeleteFileW(dll.c_str()));
    assert(startup::InspectArchitecture(dll).error==ERROR_FILE_NOT_FOUND);
    assert(DeleteFileW(exe.c_str()));assert(RemoveDirectoryW(folder.c_str()));
    std::puts("PASS: PE architecture validation and launcher rejection before process startup");
}
