#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "playback_speed.h"
#include "playback_speed_discovery.h"
#include "playback_speed_compat.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" FARPROC ResolveVersionExport(unsigned index);

namespace history {
namespace {

using CreateTrackPlayer=std::uint64_t(*)(
    std::uint64_t,std::uint64_t,void*,double,unsigned int,int,unsigned int,
    std::uint64_t,unsigned int,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t
);
using ContextSpeedSetter=unsigned char(*)(void*,double);

static CreateTrackPlayer original=nullptr;
static hooks::InitController speed_init;
static std::atomic<bool> supported{false};
static std::atomic<PlaybackSpeedBackend> active_backend{PlaybackSpeedBackend::Unsupported};
static std::atomic<std::uintptr_t> latest_context{0};
static HMODULE latest_module=nullptr;
static std::size_t latest_image_size=0;
static ULONGLONG latest_scan_time=0;
static ULONGLONG latest_apply_time=0;
static double latest_logged_speed=-1.0;

// Spotify 1.3.3.264 x64. These RVAs were verified against the official
// SpotifyFullSetupX64.exe payload. Unlike the old constructor detour, the
// latest backend calls Spotify's own ContextPlayer speed methods directly.
constexpr std::uint32_t k133VtableRva=0x01a04958;
constexpr std::uint32_t k133CurrentSetterRva=0x0057e5b8;
constexpr std::uint32_t k133PreparedSetterRva=0x0057e9a8;
constexpr std::uint32_t k133VtableAssignRva=0x00575c2e;
constexpr std::size_t k133CurrentPlayerOffset=0x888;
constexpr std::size_t k133PreparedPlayerOffset=0x898;
constexpr std::size_t k133DispatcherOffset=0x948;
constexpr std::size_t k133CurrentSetterSlot=0x138;
constexpr std::size_t k133PreparedSetterSlot=0x140;

struct SpotifyVersion {
    std::uint16_t major=0,minor=0,patch=0,build=0;
};

static bool ReadSelf(const void* address,void* output,std::size_t bytes) {
    SIZE_T read=0;
    return address&&output&&bytes&&
        ReadProcessMemory(GetCurrentProcess(),address,output,bytes,&read)&&read==bytes;
}

template<class T>
static bool ReadSelf(const void* address,T& output) {
    return ReadSelf(address,&output,sizeof(output));
}

static bool ModuleVersion(HMODULE module,SpotifyVersion& out) {
    wchar_t path[32768]{};
    const DWORD length=GetModuleFileNameW(module,path,static_cast<DWORD>(sizeof(path)/sizeof(path[0])));
    if(!length || length>=sizeof(path)/sizeof(path[0]))return false;

    using SizeFn=DWORD (WINAPI*)(LPCWSTR,LPDWORD);
    using InfoFn=BOOL (WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID);
    using QueryFn=BOOL (WINAPI*)(LPCVOID,LPCWSTR,LPVOID*,PUINT);
    auto size_fn=reinterpret_cast<SizeFn>(::ResolveVersionExport(7));
    auto info_fn=reinterpret_cast<InfoFn>(::ResolveVersionExport(8));
    auto query_fn=reinterpret_cast<QueryFn>(::ResolveVersionExport(16));
    if(!size_fn||!info_fn||!query_fn)return false;

    DWORD ignored=0;
    const DWORD bytes=size_fn(path,&ignored);
    if(!bytes||bytes>4*1024*1024)return false;
    std::vector<std::uint8_t> buffer(bytes);
    if(!info_fn(path,0,bytes,buffer.data()))return false;

    VS_FIXEDFILEINFO* fixed=nullptr;
    UINT fixed_bytes=0;
    if(!query_fn(buffer.data(),L"\\",reinterpret_cast<void**>(&fixed),&fixed_bytes)||
       !fixed||fixed_bytes<sizeof(VS_FIXEDFILEINFO)||fixed->dwSignature!=VS_FFI_SIGNATURE)
        return false;

    out.major=static_cast<std::uint16_t>(fixed->dwFileVersionMS>>16);
    out.minor=static_cast<std::uint16_t>(fixed->dwFileVersionMS&0xffffu);
    out.patch=static_cast<std::uint16_t>(fixed->dwFileVersionLS>>16);
    out.build=static_cast<std::uint16_t>(fixed->dwFileVersionLS&0xffffu);
    return true;
}

static std::uint64_t Hook(std::uint64_t a1,std::uint64_t player_meta,void* track_meta,
                          double native_speed,unsigned int normalization,int urgency,
                          unsigned int track_select_flag,std::uint64_t a8,unsigned int a9,
                          std::uint64_t start_position_ms,std::uint64_t seek_timestamp,
                          std::uint64_t a12,std::uint64_t a13) {
    const auto configured=GetSettings().playback_speed;
    const double speed=configured>1.0?configured:native_speed;
    return original(a1,player_meta,track_meta,speed,normalization,urgency,track_select_flag,
                    a8,a9,start_position_ms,seek_timestamp,a12,a13);
}

static bool ImageSize(HMODULE module,std::size_t& size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    if(!base)return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x100000)return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
    const auto image_size=nt->OptionalHeader.SizeOfImage;
    if(image_size<0x1000||image_size>0x80000000u)return false;
    size=image_size;return true;
}

static bool Verify133Layout(HMODULE module,std::size_t image_size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    const std::size_t required=k133VtableRva+k133PreparedSetterSlot+sizeof(std::uintptr_t);
    if(!base||image_size<required)return false;

    constexpr std::uint8_t current_prefix[]={
        0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x56
    };
    constexpr std::uint8_t prepared_prefix[]={
        0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x70,0x20,0x55,
        0x48,0x8d,0x6c,0x24,0x80
    };
    constexpr std::uint8_t vtable_assignment[]={
        0x48,0x8d,0x05,0x23,0xed,0x48,0x01,0x48,0x89,0x01
    };

    if(std::memcmp(base+k133CurrentSetterRva,current_prefix,sizeof(current_prefix))||
       std::memcmp(base+k133PreparedSetterRva,prepared_prefix,sizeof(prepared_prefix))||
       std::memcmp(base+k133VtableAssignRva,vtable_assignment,sizeof(vtable_assignment)))
        return false;

    std::uintptr_t current_slot=0,prepared_slot=0;
    std::memcpy(&current_slot,base+k133VtableRva+k133CurrentSetterSlot,sizeof(current_slot));
    std::memcpy(&prepared_slot,base+k133VtableRva+k133PreparedSetterSlot,sizeof(prepared_slot));
    return current_slot==reinterpret_cast<std::uintptr_t>(base+k133CurrentSetterRva)&&
           prepared_slot==reinterpret_cast<std::uintptr_t>(base+k133PreparedSetterRva);
}

static bool ExecutableInSpotify(std::uintptr_t address) {
    if(!latest_module||address<reinterpret_cast<std::uintptr_t>(latest_module)||
       address>=reinterpret_cast<std::uintptr_t>(latest_module)+latest_image_size)
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&mbi,sizeof(mbi))||
       mbi.State!=MEM_COMMIT||(mbi.Protect&PAGE_GUARD)||(mbi.Protect&PAGE_NOACCESS))
        return false;
    const DWORD protect=mbi.Protect&0xffu;
    return protect==PAGE_EXECUTE||protect==PAGE_EXECUTE_READ||
           protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY;
}

static bool TrackPlayerLooksValid(std::uintptr_t player) {
    if(!player)return true;
    std::uintptr_t vtable=0,setter=0,getter=0;
    if(!ReadSelf(reinterpret_cast<const void*>(player),vtable)||!vtable)return false;
    if(!ReadSelf(reinterpret_cast<const void*>(vtable+0xc0),setter)||
       !ReadSelf(reinterpret_cast<const void*>(vtable+0xc8),getter))
        return false;
    return ExecutableInSpotify(setter)&&ExecutableInSpotify(getter);
}

static bool ContextLooksValid(std::uintptr_t candidate,bool* active=nullptr) {
    const auto module=reinterpret_cast<std::uintptr_t>(latest_module);
    if(!candidate||!module)return false;
    std::uintptr_t vtable=0,current=0,prepared=0,dispatcher=0;
    if(!ReadSelf(reinterpret_cast<const void*>(candidate),vtable)||
       vtable!=module+k133VtableRva||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133CurrentPlayerOffset),current)||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133PreparedPlayerOffset),prepared)||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133DispatcherOffset),dispatcher)||
       !dispatcher||
       !TrackPlayerLooksValid(current)||!TrackPlayerLooksValid(prepared))
        return false;
    if(active)*active=current||prepared;
    return true;
}

static bool WritablePrivate(const MEMORY_BASIC_INFORMATION& mbi) {
    if(mbi.State!=MEM_COMMIT||mbi.Type!=MEM_PRIVATE||
       (mbi.Protect&PAGE_GUARD)||(mbi.Protect&PAGE_NOACCESS))
        return false;
    const DWORD protect=mbi.Protect&0xffu;
    return protect==PAGE_READWRITE||protect==PAGE_WRITECOPY||
           protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY;
}

static std::uintptr_t Find133Context() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto wanted=reinterpret_cast<std::uintptr_t>(latest_module)+k133VtableRva;
    const auto minimum=reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
    std::vector<std::uint8_t> buffer(256*1024);
    std::vector<std::uintptr_t> candidates;
    std::vector<std::uintptr_t> active_candidates;

    for(std::uintptr_t address=minimum;address<maximum;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(reinterpret_cast<const void*>(address),&mbi,sizeof(mbi)))break;
        const auto base=reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto next=base+mbi.RegionSize;
        if(next<=address)break;

        if(WritablePrivate(mbi) && mbi.RegionSize<=256ull*1024*1024) {
            for(std::uintptr_t chunk=base;chunk<next;) {
                const auto wanted_bytes=static_cast<std::size_t>(
                    (next-chunk)<buffer.size()?(next-chunk):buffer.size());
                SIZE_T copied=0;
                if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(chunk),
                                     buffer.data(),wanted_bytes,&copied)&&copied>=sizeof(std::uintptr_t)) {
                    std::size_t offset=static_cast<std::size_t>((8-(chunk&7u))&7u);
                    for(;offset+sizeof(std::uintptr_t)<=copied;offset+=sizeof(std::uintptr_t)) {
                        std::uintptr_t value=0;
                        std::memcpy(&value,buffer.data()+offset,sizeof(value));
                        if(value!=wanted)continue;
                        const auto candidate=chunk+offset;
                        bool active=false;
                        if(ContextLooksValid(candidate,&active)) {
                            candidates.push_back(candidate);
                            if(active)active_candidates.push_back(candidate);
                            if(active_candidates.size()>1)return 0;
                        }
                    }
                }
                if(wanted_bytes==0)break;
                chunk+=wanted_bytes;
            }
        }
        address=next;
    }

    if(active_candidates.size()==1)return active_candidates.front();
    if(candidates.size()==1)return candidates.front();
    return 0;
}

static bool Apply133(double speed,bool log_change) {
    auto context=latest_context.load(std::memory_order_acquire);
    if(!ContextLooksValid(context)) {
        latest_context.store(0,std::memory_order_release);
        return false;
    }
    if(!std::isfinite(speed)||speed<1.0||speed>50.0)speed=1.0;

    auto current=reinterpret_cast<ContextSpeedSetter>(
        reinterpret_cast<std::uint8_t*>(latest_module)+k133CurrentSetterRva);
    auto prepared=reinterpret_cast<ContextSpeedSetter>(
        reinterpret_cast<std::uint8_t*>(latest_module)+k133PreparedSetterRva);

    const bool current_result=current(reinterpret_cast<void*>(context),speed)!=0;
    const bool prepared_result=prepared(reinterpret_cast<void*>(context),speed)!=0;
    if(log_change && std::fabs(speed-latest_logged_speed)>0.0001) {
        char line[256];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 playback speed %.3fx applied through ContextPlayer current=%d prepared=%d",
            speed,current_result,prepared_result);
        HistoryLog(line);
        latest_logged_speed=speed;
    }
    return current_result||prepared_result;
}

static bool StartConstructorBackend(HMODULE module,std::size_t image_size,std::uint64_t now) {
    std::uint32_t rva=0;
    auto result=DiscoverPlaybackSpeedTarget(reinterpret_cast<const std::uint8_t*>(module),image_size,rva);
    if(result!=PlaybackDiscoveryResult::Found||!rva) {
        char line[160];std::snprintf(line,sizeof(line),
            "playback-speed discovery unavailable: result=%u",unsigned(result));
        HistoryLog(line);speed_init.MarkUnsupported();return false;
    }
    void* target=reinterpret_cast<std::uint8_t*>(module)+rva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(target,reinterpret_cast<void*>(Hook),
                             reinterpret_cast<void**>(&original));
    if(status==MH_OK)status=MH_EnableHook(target);
    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        active_backend.store(PlaybackSpeedBackend::ConstructorHook,std::memory_order_release);
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[160];std::snprintf(line,sizeof(line),
            "native playback-speed constructor hook active at Spotify.dll+0x%08x",rva);
        HistoryLog(line);
        return true;
    }

    char line[180];std::snprintf(line,sizeof(line),
        "native playback-speed hook failed: %s",MH_StatusToString(status));
    HistoryLog(line);
    if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
        speed_init.MarkUnsupported();
    else speed_init.Retry(now);
    return false;
}

static bool Start133Backend(HMODULE module,std::size_t image_size) {
    if(!Verify133Layout(module,image_size)) {
        HistoryLog("Spotify 1.3.3 playback-speed layout verification failed; staying at 1x");
        speed_init.MarkUnsupported();
        return false;
    }
    latest_module=module;
    latest_image_size=image_size;
    active_backend.store(PlaybackSpeedBackend::ContextSetter133,std::memory_order_release);
    supported.store(true,std::memory_order_release);
    speed_init.Activate();
    HistoryLog("Spotify 1.3.3 ContextPlayer playback-speed backend validated");
    return true;
}

}

bool PlaybackSpeedSupported() {
    return supported.load(std::memory_order_acquire);
}

bool PlaybackSpeedImmediate() {
    return active_backend.load(std::memory_order_acquire)==PlaybackSpeedBackend::ContextSetter133;
}

void StartPlaybackSpeed(HMODULE module) {
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    if(speed_init.State()==hooks::InitState::Active)return;
    if(!speed_init.TryBegin(now))return;

    SpotifyVersion version{};
    if(!ModuleVersion(module,version)) {
        HistoryLog("playback-speed backend disabled: Spotify.dll version could not be verified");
        speed_init.MarkUnsupported();
        return;
    }

    const auto backend=PlaybackSpeedBackendForVersion(
        version.major,version.minor,version.patch,version.build);
    if(backend==PlaybackSpeedBackend::Unsupported) {
        char line[240];
        std::snprintf(line,sizeof(line),
            "playback-speed backend disabled for Spotify %u.%u.%u.%u: player ABI not validated",
            unsigned(version.major),unsigned(version.minor),unsigned(version.patch),unsigned(version.build));
        HistoryLog(line);
        speed_init.MarkUnsupported();
        return;
    }

    std::size_t image_size=0;
    if(!ImageSize(module,image_size)) {
        HistoryLog("playback-speed backend unavailable: invalid Spotify.dll image");
        speed_init.MarkUnsupported();
        return;
    }

    if(backend==PlaybackSpeedBackend::ConstructorHook) {
        StartConstructorBackend(module,image_size,now);
        return;
    }
    if(backend==PlaybackSpeedBackend::ContextSetter133) {
        Start133Backend(module,image_size);
        return;
    }

    speed_init.MarkUnsupported();
}

void MaintainPlaybackSpeed(HMODULE module) {
    if(active_backend.load(std::memory_order_acquire)!=PlaybackSpeedBackend::ContextSetter133||
       !supported.load(std::memory_order_acquire)||module!=latest_module)
        return;

    const ULONGLONG now=GetTickCount64();
    if(now-latest_apply_time<250)return;
    latest_apply_time=now;

    auto context=latest_context.load(std::memory_order_acquire);
    if(!ContextLooksValid(context)) {
        latest_context.store(0,std::memory_order_release);
        if(now-latest_scan_time<1500)return;
        latest_scan_time=now;
        context=Find133Context();
        if(!context)return;
        latest_context.store(context,std::memory_order_release);
        char line[192];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 ContextPlayer located at %p",reinterpret_cast<void*>(context));
        HistoryLog(line);
    }

    Apply133(GetSettings().playback_speed,true);
}

void ApplyPlaybackSpeedNow() {
    if(active_backend.load(std::memory_order_acquire)!=PlaybackSpeedBackend::ContextSetter133)
        return;
    latest_apply_time=0;
    Apply133(GetSettings().playback_speed,true);
}

}
