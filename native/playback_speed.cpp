#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "playback_speed.h"
#include "playback_speed_compat.h"
#include "playback_speed_pcm_core.h"
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

struct DecodeResult {
    std::uint32_t code;
    std::uint32_t reserved;
    std::uint64_t detail;
};

using DecodeAudio=DecodeResult*(*)(void*,DecodeResult*,float*,std::uint64_t*,
                                   const unsigned char*,std::uint64_t*,unsigned char);

static DecodeAudio original_decode=nullptr;
static hooks::InitController speed_init;
static std::atomic<bool> supported{false};
static std::atomic<unsigned long long> decode_calls{0};
static std::atomic<unsigned long long> thinned_calls{0};
static std::atomic<double> last_requested{1.0};

// Spotify 1.3.1.234.g59d6bf59 x64, verified against the official x64 payload.
// This is the live snd-decoder dispatcher: the same layer old Soggfy hooked.
// The dispatcher consumes compressed data first, then returns the produced PCM
// float count through R9. Reducing only that returned count accelerates playback
// without relying on per-track/player construction, so natural queue advances
// and pre-created next-track players keep the selected speed.
constexpr std::uint32_t kDecodeAudioRva=0x00e9b3a0;
constexpr std::uint32_t kDecoderNameRva=0x01aa55a8;
constexpr std::uint32_t kDecoderVtableRva=0x01aa55b8;
constexpr std::uint32_t kDecodeAudioSlotRva=0x01aa55c0;
constexpr std::uint32_t kDecoderFirstMethodRva=0x00e9b36c;

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

static bool ImageSize(HMODULE module,std::size_t& size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    if(!base)return false;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x100000)return false;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
    const auto image_size=nt->OptionalHeader.SizeOfImage;
    if(image_size<0x1000||image_size>0x80000000u)return false;
    size=image_size;
    return true;
}

static bool VerifyDecoder(HMODULE module,std::size_t image_size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    const std::size_t required=kDecodeAudioSlotRva+sizeof(std::uintptr_t);
    if(!base||image_size<required||image_size<kDecodeAudioRva+0x220)return false;

    constexpr char decoder_name[]="snd-decoder";
    constexpr std::uint8_t prefix[]={
        0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,
        0x89,0x78,0x20,0x4c,0x89,0x40,0x18,0x55,0x41,0x54,0x41,0x55,
        0x41,0x56,0x41,0x57,0x48,0x8b,0xec,0x48,0x81,0xec,0x80,0x00,
        0x00,0x00,0x4d,0x8b,0xe1,0x48,0x8b,0xfa,0x48,0x8b,0xf1,0x4c,
        0x8b,0x75,0x58
    };
    // At +0x178 the dispatcher calls the live compressed-audio decoder.
    constexpr std::uint8_t ogg_decode_call[]={
        0x48,0x8d,0x55,0xb8,0x48,0x8d,0x4e,0x18,0xe8,0xab,0xf7,0xff,0xff
    };
    // At +0x20b it converts produced bytes to float samples and writes the
    // resulting count back through the original R9 pointer (saved in R12).
    constexpr std::uint8_t count_store[]={
        0x48,0xc1,0xe8,0x02,0x49,0x89,0x04,0x24,0x41,0x8b,0xc5,0x49,0x89,0x06
    };

    if(std::memcmp(base+kDecoderNameRva,decoder_name,sizeof(decoder_name))||
       std::memcmp(base+kDecodeAudioRva,prefix,sizeof(prefix))||
       std::memcmp(base+kDecodeAudioRva+0x178,ogg_decode_call,sizeof(ogg_decode_call))||
       std::memcmp(base+kDecodeAudioRva+0x20b,count_store,sizeof(count_store)))
        return false;

    std::uintptr_t first=0,slot=0;
    std::memcpy(&first,base+kDecoderVtableRva,sizeof(first));
    std::memcpy(&slot,base+kDecodeAudioSlotRva,sizeof(slot));
    return first==reinterpret_cast<std::uintptr_t>(base+kDecoderFirstMethodRva)&&
           slot==reinterpret_cast<std::uintptr_t>(base+kDecodeAudioRva);
}

static double RequestedSpeed() {
    const double value=GetSettings().playback_speed;
    return std::isfinite(value)&&value>=1.0&&value<=50.0?value:1.0;
}

static DecodeResult* DecodeAudioHook(void* self,DecodeResult* output,
                                     float* pcm,std::uint64_t* sample_count,
                                     const unsigned char* encoded,
                                     std::uint64_t* encoded_count,
                                     unsigned char flags) {
    const std::uint64_t capacity=sample_count?*sample_count:0;
    const std::uint64_t encoded_before=encoded_count?*encoded_count:0;

    DecodeResult* result=original_decode(
        self,output,pcm,sample_count,encoded,encoded_count,flags);

    const auto call=decode_calls.fetch_add(1,std::memory_order_relaxed)+1;
    if(!sample_count)return result;

    const double requested=RequestedSpeed();
    const std::uint64_t produced=*sample_count;
    const std::uint64_t encoded_after=encoded_count?*encoded_count:0;
    const bool sane=pcm&&capacity>0&&capacity<0x10000000ull&&
                    produced>0&&produced<=capacity;

    std::uint64_t kept=produced;
    bool thinned=false;
    if(sane&&requested>1.0&&produced>1) {
        kept=ClassicPcmSamplesToKeep(produced,requested);
        if(kept<produced) {
            *sample_count=kept;
            thinned=true;
            thinned_calls.fetch_add(1,std::memory_order_relaxed);
        }
    }

    const double previous=last_requested.exchange(requested,std::memory_order_relaxed);
    if(call<=12||std::fabs(previous-requested)>0.0001) {
        char line[640];
        std::snprintf(line,sizeof(line),
            "speed_decode_hook call=%llu capacity=%llu produced=%llu kept=%llu encoded_before=%llu encoded_after=%llu requested=%.3fx sane=%d thinned=%d flags=%u result_same=%d total_thinned=%llu",
            call,
            static_cast<unsigned long long>(capacity),
            static_cast<unsigned long long>(produced),
            static_cast<unsigned long long>(kept),
            static_cast<unsigned long long>(encoded_before),
            static_cast<unsigned long long>(encoded_after),
            requested,sane,thinned,unsigned(flags),result==output,
            thinned_calls.load(std::memory_order_relaxed));
        HistoryLog(line);
    }
    return result;
}

}

bool PlaybackSpeedSupported() {
    return supported.load(std::memory_order_acquire);
}

bool PlaybackSpeedImmediate() {
    return supported.load(std::memory_order_acquire);
}

double PlaybackSpeedEffective() {
    return PlaybackSpeedSupported()?RequestedSpeed():1.0;
}

void ArmPlaybackSpeedRuntime() {
    // No-op for the PCM backend. Speed is read on every DecodeAudio call.
}

bool PlaybackSpeedRuntimeArmed() {
    return PlaybackSpeedSupported();
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
            "playback-speed hook disabled for Spotify %u.%u.%u.%u: decoder ABI not validated; capture remains available at 1x",
            unsigned(version.major),unsigned(version.minor),unsigned(version.patch),unsigned(version.build));
        HistoryLog(line);
        speed_init.MarkUnsupported();
        return;
    }

    std::size_t image_size=0;
    if(!ImageSize(module,image_size)||!VerifyDecoder(module,image_size)) {
        HistoryLog("playback-speed PCM hook disabled: Spotify 1.3.1 snd-decoder layout verification failed");
        speed_init.MarkUnsupported();
        return;
    }

    void* target=reinterpret_cast<std::uint8_t*>(module)+kDecodeAudioRva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(target,reinterpret_cast<void*>(DecodeAudioHook),
                             reinterpret_cast<void**>(&original_decode));
    if(status==MH_OK)status=MH_EnableHook(target);

    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[280];
        std::snprintf(line,sizeof(line),
            "native playback-speed PCM hook active at Spotify.dll+0x%08x; speed applies per decode call across natural track transitions",
            kDecodeAudioRva);
        HistoryLog(line);
    } else {
        char line[180];
        std::snprintf(line,sizeof(line),
            "native playback-speed PCM hook failed: %s",MH_StatusToString(status));
        HistoryLog(line);
        if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
            speed_init.MarkUnsupported();
        else
            speed_init.Retry(now);
    }
}

}
