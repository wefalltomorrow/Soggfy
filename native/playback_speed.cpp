#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "playback_speed.h"
#include "playback_speed_discovery.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace history {
namespace {
using CreateTrackPlayer=std::uint64_t(*)(
    std::uint64_t,std::uint64_t,void*,double,unsigned int,int,unsigned int,
    std::uint64_t,unsigned int,std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t
);
static CreateTrackPlayer original=nullptr;
static hooks::InitController speed_init;
static std::atomic<bool> supported{false};

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
}

bool PlaybackSpeedSupported() {
    return supported.load(std::memory_order_acquire);
}

void StartPlaybackSpeed(HMODULE module) {
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    if(speed_init.State()==hooks::InitState::Active)return;
    if(!speed_init.TryBegin(now))return;
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
        char line[160];std::snprintf(line,sizeof(line),
            "native playback-speed hook active at Spotify.dll+0x%08x",rva);
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
