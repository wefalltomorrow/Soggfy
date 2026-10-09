#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "playback_speed.h"
#include "playback_speed_discovery.h"
#include "playback_speed_compat.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
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
static CreateTrackPlayer original=nullptr;
static hooks::InitController speed_init;
static std::atomic<bool> supported{false};
static std::atomic<bool> runtime_armed{false};

struct SpotifyVersion {
    std::uint16_t major=0,minor=0,patch=0,build=0;
};

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
    const bool armed=runtime_armed.load(std::memory_order_acquire);
    const double speed=armed&&configured>1.0?configured:native_speed;
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
}

bool PlaybackSpeedSupported() {
    return supported.load(std::memory_order_acquire);
}

void ArmPlaybackSpeedRuntime() {
    if(!supported.load(std::memory_order_acquire))return;
    if(!runtime_armed.exchange(true,std::memory_order_acq_rel))
        HistoryLog("native playback-speed runtime armed after Spotify player initialization");
}

bool PlaybackSpeedRuntimeArmed() {
    return runtime_armed.load(std::memory_order_acquire);
}

void StartPlaybackSpeed(HMODULE module) {
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    if(speed_init.State()==hooks::InitState::Active)return;
    if(!speed_init.TryBegin(now))return;

    SpotifyVersion version{};
    if(!ModuleVersion(module,version)) {
        HistoryLog("playback-speed hook disabled: Spotify.dll version could not be verified");
        speed_init.MarkUnsupported();
        return;
    }
    if(!PlaybackSpeedVersionSupported(version.major,version.minor,version.patch,version.build)) {
        char line[240];
        std::snprintf(line,sizeof(line),
            "playback-speed hook disabled for Spotify %u.%u.%u.%u: player ABI not validated; capture remains available at 1x",
            unsigned(version.major),unsigned(version.minor),unsigned(version.patch),unsigned(version.build));
        HistoryLog(line);
        speed_init.MarkUnsupported();
        return;
    }

    std::size_t image_size=0;
    if(!ImageSize(module,image_size)) {
        HistoryLog("playback-speed discovery unavailable: invalid Spotify.dll image");
        speed_init.MarkUnsupported();return;
    }
    std::uint32_t rva=0;
    auto result=DiscoverPlaybackSpeedTarget(reinterpret_cast<const std::uint8_t*>(module),image_size,rva);
    if(result!=PlaybackDiscoveryResult::Found||!rva) {
        char line[160];std::snprintf(line,sizeof(line),
            "playback-speed discovery unavailable: result=%u",unsigned(result));
        HistoryLog(line);speed_init.MarkUnsupported();return;
    }
    void* target=reinterpret_cast<std::uint8_t*>(module)+rva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(target,reinterpret_cast<void*>(Hook),
                             reinterpret_cast<void**>(&original));
    if(status==MH_OK)status=MH_EnableHook(target);
    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[220];std::snprintf(line,sizeof(line),
            "native playback-speed hook active at Spotify.dll+0x%08x; acceleration remains disarmed until the Spotify player is ready",rva);
        HistoryLog(line);
    } else {
        char line[180];std::snprintf(line,sizeof(line),
            "native playback-speed hook failed: %s",MH_StatusToString(status));
        HistoryLog(line);
        if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
            speed_init.MarkUnsupported();
        else speed_init.Retry(now);
    }
}
}
