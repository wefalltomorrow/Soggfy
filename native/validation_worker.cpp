#include <windows.h>
#include "validation_protocol.h"
#include "vorbis_validation.h"
#include "flac_validation.h"
#include <cwchar>
#include <limits>
namespace {
bool Number(const wchar_t* s,uint64_t& value) {
    if(!s||!*s)return false;
    value=0;
    for(;*s;++s) {
        if(*s<L'0'||*s>L'9'||value>(UINT64_MAX-unsigned(*s-L'0'))/10)return false;
        value=value*10+unsigned(*s-L'0');
    }
    return true;
}
}
int wmain(int argc,wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc!=5)return 2;
    uint64_t map=0,pipe=0,size=0,format=0;
    if(!Number(argv[1],map)||!Number(argv[2],pipe)||!Number(argv[3],size)||
       !Number(argv[4],format)||!map||!pipe||map>UINTPTR_MAX||pipe>UINTPTR_MAX||
       !size||size>history::ValidationInputLimit||(format!=1&&format!=2))return 2;
    HANDLE mapping=reinterpret_cast<HANDLE>(uintptr_t(map));
    HANDLE output=reinterpret_cast<HANDLE>(uintptr_t(pipe));
    auto view=static_cast<const uint8_t*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,size_t(size)));
    if(!view)return 3;
    history::ValidationReply reply;
    bool valid=false;
    try {
        std::vector<uint8_t> bytes(view,view+size_t(size));
        std::string error;
        if(format==1) {
            history::VorbisDecodedCoverage decoded;
            valid=history::DecodeVorbisRecording(bytes,decoded,error);
            reply.samples=decoded.samples;reply.units=decoded.packets;
            reply.rate=decoded.rate;reply.channels=decoded.channels;
        } else {
            history::FlacDecodedCoverage decoded;
            valid=history::DecodeFlacRecording(bytes,decoded,error);
            reply.samples=decoded.samples;reply.units=decoded.frames;
            reply.rate=decoded.rate;reply.channels=decoded.channels;reply.bits=decoded.bits;
        }
    } catch(...) {valid=false;}
    UnmapViewOfFile(view);CloseHandle(mapping);
    reply.format=uint32_t(format);
    DWORD written=0;
    if(!valid||!history::ValidReply(reply,history::ValidationFormat(format))||
       !WriteFile(output,&reply,sizeof(reply),&written,nullptr)||written!=sizeof(reply))return 4;
    CloseHandle(output);return 0;
}
