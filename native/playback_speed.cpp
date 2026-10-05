#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "playback_speed.h"
#include "playback_speed_discovery.h"
#include "playback_speed_compat.h"
#include "history_settings.h"
#include "async_log.h"
#include "hook_init_state.h"
#include "vendor/minhook/include/MinHook.h"
#include <algorithm>
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
using TrackSpeedSetter=void(*)(void*,double,unsigned int);
using TrackSpeedGetter=double(*)(void*);
using SessionSpeedSetter=void(*)(void*,double,unsigned int);
using SessionSpeedGetter=double(*)(void*);

struct PcmSpan133 {
    float* data;
    std::uint64_t size;
};
struct PcmResult133 {
    float* data;
    std::uint64_t size;
    unsigned char state;
};
using PcmProcess133=PcmResult133*(*)(void*,PcmResult133*,const PcmSpan133*);

static CreateTrackPlayer original=nullptr;
static SessionSpeedSetter original_session_setter=nullptr;
static SessionSpeedGetter original_session_getter=nullptr;
static PcmProcess133 original_pcm_process=nullptr;
static hooks::InitController speed_init;
static std::atomic<bool> supported{false};
static std::atomic<PlaybackSpeedBackend> active_backend{PlaybackSpeedBackend::Unsupported};
static std::atomic<std::uintptr_t> latest_context{0};
static std::atomic<std::uintptr_t> latest_session{0};
static std::atomic<double> observed_session_speed{1.0};
static std::atomic<ULONGLONG> observed_session_time{0};
static std::atomic<double> effective_speed{1.0};
static HMODULE latest_module=nullptr;
static std::size_t latest_image_size=0;
static ULONGLONG latest_scan_time=0;
static ULONGLONG latest_apply_time=0;
static double latest_logged_speed=-1.0;
static ULONGLONG latest_failure_log_time=0;
static ULONGLONG latest_scan_log_time=0;
static ULONGLONG latest_session_scan_time=0;
static ULONGLONG latest_session_log_time=0;
static std::atomic<unsigned long long> session_setter_calls{0};
static std::atomic<unsigned long long> session_getter_calls{0};
static std::atomic<unsigned long long> pcm_process_calls{0};
static std::atomic<unsigned long long> pcm_thinned_calls{0};
static std::atomic<double> pcm_last_requested{1.0};

// Spotify 1.3.3.264 x64. These RVAs were verified against the official
// SpotifyFullSetupX64.exe payload. RC21 uses SessionTrackPlayer's real
// setPlaybackSpeed/getPlaybackSpeed vtable pair. The older AudioSessionImpl
// creation/ContextPlayer RVAs are retained only as historical fallback code.
constexpr std::uint32_t k133PcmProcessRva=0x00463954;
constexpr std::uint32_t k133PcmProcessVtableSlotRva=0x019c7c68;
constexpr std::uint32_t k133TrackCreateRva=0x0057968c;
constexpr std::uint32_t k133SessionVtableRva=0x01a07308;
constexpr std::uint32_t k133SessionSetterRva=0x005a8d18;
constexpr std::uint32_t k133SessionGetterRva=0x005a17d8;
constexpr std::uint32_t k133SessionVtableAssignRva=0x0059c09b;
constexpr std::size_t k133SessionSetterSlot=0x0c0;
constexpr std::size_t k133SessionGetterSlot=0x0c8;
constexpr std::size_t k133SessionDispatcherOffset=0x02a8;
constexpr std::size_t k133SessionPlayerOffset=0x0520;
constexpr std::size_t k133SessionCachedSpeedOffset=0x03f0;
constexpr std::size_t k133SessionAutomationBeginOffset=0x2018;
constexpr std::size_t k133SessionAutomationEndOffset=0x2020;
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

    if(active_backend.load(std::memory_order_acquire)==PlaybackSpeedBackend::TrackCreate133) {
        effective_speed.store(speed,std::memory_order_release);
        char line[320];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 track-player create: native=%.3fx requested=%.3fx applied=%.3fx track_select_flag=%u start_position_ms=%llu",
            native_speed,configured,speed,track_select_flag,
            static_cast<unsigned long long>(start_position_ms));
        HistoryLog(line);
        LogActivity("speed_applied",line);
    }

    return original(a1,player_meta,track_meta,speed,normalization,urgency,track_select_flag,
                    a8,a9,start_position_ms,seek_timestamp,a12,a13);
}

static PcmResult133* PcmProcessHook(void* self,PcmResult133* output,
                                        const PcmSpan133* input) {
    PcmResult133* result=original_pcm_process(self,output,input);
    const auto call=pcm_process_calls.fetch_add(1,std::memory_order_relaxed)+1;
    if(!result||result!=output||!output||!input)return result;

    const auto configured=GetSettings().playback_speed;
    const double requested=std::isfinite(configured)&&configured>=1.0&&configured<=50.0?
        configured:1.0;
    const std::uint64_t produced=output->size;
    const bool sane=output->data&&input->data&&output->data==input->data&&
        produced>0&&produced<=input->size&&input->size<0x10000000ull;

    std::uint64_t kept=produced;
    bool thinned=false;
    if(sane&&requested>1.0&&produced>1) {
        kept=static_cast<std::uint64_t>(
            std::max<double>(1.0,std::floor(static_cast<double>(produced)/requested)));
        if(kept<produced) {
            output->size=kept;
            thinned=true;
            pcm_thinned_calls.fetch_add(1,std::memory_order_relaxed);
            effective_speed.store(requested,std::memory_order_release);
        }
    } else if(sane&&requested<=1.0) {
        effective_speed.store(1.0,std::memory_order_release);
    }

    const double previous=pcm_last_requested.exchange(requested,std::memory_order_relaxed);
    if(call<=8||std::fabs(previous-requested)>0.0001) {
        char line[512];
        std::snprintf(line,sizeof(line),
            "speed_pcm_hook call=%llu input=%llu produced=%llu kept=%llu requested=%.3fx sane=%d thinned=%d state=%u total_thinned=%llu",
            call,static_cast<unsigned long long>(input->size),
            static_cast<unsigned long long>(produced),
            static_cast<unsigned long long>(kept),requested,sane,thinned,
            unsigned(output->state),
            pcm_thinned_calls.load(std::memory_order_relaxed));
        HistoryLog(line);
        LogActivity(thinned?"speed_applied":"speed_observed",line);
    }
    return result;
}

static void SessionSpeedHook(void* self,double native_speed,unsigned int mode) {
    latest_session.store(reinterpret_cast<std::uintptr_t>(self),std::memory_order_release);
    const auto configured=GetSettings().playback_speed;
    const double speed=std::isfinite(configured)&&configured>1.0&&configured<=50.0?
        configured:native_speed;
    const auto call=session_setter_calls.fetch_add(1,std::memory_order_relaxed)+1;
    original_session_setter(self,speed,mode);

    if(call<=8) {
        char line[384];
        std::snprintf(line,sizeof(line),
            "speed_session_hook setter call=%llu session=%p native=%.6f requested=%.6f applied=%.6f mode=%u",
            call,self,native_speed,configured,speed,mode);
        HistoryLog(line);
    }
}

static double SessionGetterHook(void* self) {
    const double speed=original_session_getter(self);
    latest_session.store(reinterpret_cast<std::uintptr_t>(self),std::memory_order_release);
    const auto call=session_getter_calls.fetch_add(1,std::memory_order_relaxed)+1;
    if(std::isfinite(speed)&&speed>0.0&&speed<100.0) {
        observed_session_speed.store(speed,std::memory_order_release);
        observed_session_time.store(GetTickCount64(),std::memory_order_release);
    }
    if(call<=8) {
        char line[320];
        std::snprintf(line,sizeof(line),
            "speed_session_hook getter call=%llu session=%p speed=%.6f",
            call,self,speed);
        HistoryLog(line);
    }
    return speed;
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

static bool Verify133PcmProcess(HMODULE module,std::size_t image_size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    const std::size_t required=std::max<std::size_t>(
        k133PcmProcessRva+0x190,
        k133PcmProcessVtableSlotRva+sizeof(std::uintptr_t));
    if(!base||image_size<required)return false;

    // Spotify 1.3.3.264's PCM filter-chain process method. This method receives
    // an input float span in R8, writes the processed span to RDX and returns
    // RDX. Classic Soggfy accelerated playback by consuming all decoded audio
    // while exposing only a fraction of the PCM samples to the sink. RC26
    // restores that behavior here instead of trying to drive unused internal
    // playback-speed setters.
    constexpr std::uint8_t prefix[]={
        0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x68,0x10,0x48,
        0x89,0x70,0x18,0x48,0x89,0x78,0x20,0x41,0x54,0x41,0x56,0x41,
        0x57,0x48,0x83,0xec,0x60,0x0f,0x29,0x70,0xd8
    };
    constexpr std::uint8_t output_anchor[]={
        0x49,0x8b,0x04,0x24,0x48,0x89,0x07,0x4c,0x89,0x77,0x08,0x48,
        0x8d,0x57,0x10
    };
    constexpr std::uint8_t inner_call[]={
        0xe8,0xac,0x00,0x00,0x00
    };

    if(std::memcmp(base+k133PcmProcessRva,prefix,sizeof(prefix))||
       std::memcmp(base+k133PcmProcessRva+0x168,output_anchor,sizeof(output_anchor))||
       std::memcmp(base+k133PcmProcessRva+0x123,inner_call,sizeof(inner_call)))
        return false;

    std::uintptr_t slot=0;
    std::memcpy(&slot,base+k133PcmProcessVtableSlotRva,sizeof(slot));
    return slot==reinterpret_cast<std::uintptr_t>(base+k133PcmProcessRva);
}

static bool Verify133SessionLayout(HMODULE module,std::size_t image_size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    const std::size_t required=k133SessionVtableRva+k133SessionGetterSlot+sizeof(std::uintptr_t);
    if(!base||image_size<required)return false;

    constexpr std::uint8_t setter_prefix[]={
        0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x55,0x56,0x57,0x48,0x81,
        0xec,0xa0,0x00,0x00,0x00,0x0f,0x29,0x70,0xd8
    };
    constexpr std::uint8_t setter_state[]={
        0x41,0x8b,0xe8,0x0f,0x28,0xf1,0x48,0x8b,0xf1,0x48,0x8b,0x81,
        0x20,0x20,0x00,0x00,0x48,0x39,0x81,0x18,0x20,0x00,0x00
    };
    constexpr std::uint8_t getter_body[]={
        0x48,0x8b,0x91,0x20,0x05,0x00,0x00,0x48,0x85,0xd2,0x74,0x0a,
        0x48,0x8b,0x02,0x48,0x8b,0xca,0x48,0xff,0x60,0x30,0xf2,0x0f,
        0x10,0x81,0xf0,0x03,0x00,0x00,0xc3
    };
    constexpr std::uint8_t vtable_assignment[]={
        0x48,0x8d,0x05,0x66,0xb2,0x46,0x01,0x48,0x89,0x06
    };

    if(std::memcmp(base+k133SessionSetterRva,setter_prefix,sizeof(setter_prefix))||
       std::memcmp(base+k133SessionSetterRva+0x27,setter_state,sizeof(setter_state))||
       std::memcmp(base+k133SessionGetterRva,getter_body,sizeof(getter_body))||
       std::memcmp(base+k133SessionVtableAssignRva,vtable_assignment,sizeof(vtable_assignment)))
        return false;

    std::uintptr_t setter_slot=0,getter_slot=0;
    std::memcpy(&setter_slot,base+k133SessionVtableRva+k133SessionSetterSlot,sizeof(setter_slot));
    std::memcpy(&getter_slot,base+k133SessionVtableRva+k133SessionGetterSlot,sizeof(getter_slot));
    return setter_slot==reinterpret_cast<std::uintptr_t>(base+k133SessionSetterRva)&&
           getter_slot==reinterpret_cast<std::uintptr_t>(base+k133SessionGetterRva);
}

static bool Verify133TrackCreate(HMODULE module,std::size_t image_size) {
    auto* base=reinterpret_cast<const std::uint8_t*>(module);
    if(!base||image_size<k133TrackCreateRva+0x1a0)return false;

    // Spotify 1.3.3.264 AudioSessionImpl track-player creation routine.
    // The fourth x64 argument is the playback-speed double in XMM3. The
    // 0x36 anchor copies XMM3 to XMM7 before the function logs "speed: %f".
    constexpr std::uint8_t prefix[]={
        0x48,0x8b,0xc4,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,
        0x56,0x41,0x57,0x48,0x8d,0xa8,0xf8,0xfc,0xff,0xff,0x48,0x81,
        0xec,0xc8,0x03,0x00,0x00,0x0f,0x29,0x70,0xa8,0x0f,0x29,0x78,
        0x98
    };
    constexpr std::uint8_t speed_anchor[]={
        0x0f,0x28,0xfb,0x49,0x8b,0xf0,0x48,0x89,0x55,0xc8,0x48,0x8b,
        0xf9,0x48,0x89,0x55,0x30,0x4c,0x89,0x85,0x80,0x01,0x00,0x00
    };
    constexpr std::uint8_t stack_anchor[]={
        0x44,0x8b,0xa5,0x30,0x03,0x00,0x00,0x44,0x89,0x64,0x24,0x48,
        0x44,0x8b,0xad,0x38,0x03,0x00,0x00,0x44,0x89,0x6c,0x24,0x4c,
        0x48,0x8b,0x85,0x48,0x03,0x00,0x00
    };
    constexpr std::uint8_t speed_log_anchor[]={
        0xf2,0x0f,0x11,0x7c,0x24,0x20
    };

    return std::memcmp(base+k133TrackCreateRva,prefix,sizeof(prefix))==0 &&
           std::memcmp(base+k133TrackCreateRva+0x36,speed_anchor,sizeof(speed_anchor))==0 &&
           std::memcmp(base+k133TrackCreateRva+0x4e,stack_anchor,sizeof(stack_anchor))==0 &&
           std::memcmp(base+k133TrackCreateRva+0x197,speed_log_anchor,sizeof(speed_log_anchor))==0;
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

static bool ExecutableAddress(std::uintptr_t address) {
    if(!address)return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(!VirtualQuery(reinterpret_cast<const void*>(address),&mbi,sizeof(mbi))||
       mbi.State!=MEM_COMMIT||(mbi.Protect&PAGE_GUARD)||(mbi.Protect&PAGE_NOACCESS))
        return false;
    const DWORD protect=mbi.Protect&0xffu;
    return protect==PAGE_EXECUTE||protect==PAGE_EXECUTE_READ||
           protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY;
}

static bool ExecutableInSpotify(std::uintptr_t address) {
    if(!latest_module||address<reinterpret_cast<std::uintptr_t>(latest_module)||
       address>=reinterpret_cast<std::uintptr_t>(latest_module)+latest_image_size)
        return false;
    return ExecutableAddress(address);
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

static bool PlayerSpeedFunctions(std::uintptr_t player,TrackSpeedSetter& setter,
                                 TrackSpeedGetter& getter) {
    setter=nullptr;getter=nullptr;
    if(!player)return false;
    std::uintptr_t vtable=0,setter_address=0,getter_address=0;
    if(!ReadSelf(reinterpret_cast<const void*>(player),vtable)||!vtable||
       !ReadSelf(reinterpret_cast<const void*>(vtable+0xc0),setter_address)||
       !ReadSelf(reinterpret_cast<const void*>(vtable+0xc8),getter_address)||
       !ExecutableInSpotify(setter_address)||!ExecutableInSpotify(getter_address))
        return false;
    setter=reinterpret_cast<TrackSpeedSetter>(setter_address);
    getter=reinterpret_cast<TrackSpeedGetter>(getter_address);
    return true;
}

static bool ReadPlayerSpeed(std::uintptr_t player,double& speed) {
    TrackSpeedSetter setter=nullptr;TrackSpeedGetter getter=nullptr;
    if(!PlayerSpeedFunctions(player,setter,getter))return false;
    speed=getter(reinterpret_cast<void*>(player));
    return std::isfinite(speed)&&speed>0.0&&speed<100.0;
}

static bool SetPlayerSpeedDirect(std::uintptr_t player,double speed,unsigned int mode,
                                 double& before,double& after) {
    TrackSpeedSetter setter=nullptr;TrackSpeedGetter getter=nullptr;
    if(!PlayerSpeedFunctions(player,setter,getter))return false;
    before=getter(reinterpret_cast<void*>(player));
    if(!std::isfinite(before)||before<=0.0||before>=100.0)return false;
    setter(reinterpret_cast<void*>(player),speed,mode);
    after=getter(reinterpret_cast<void*>(player));
    return std::isfinite(after)&&after>0.0&&after<100.0;
}

static bool WritablePrivate(const MEMORY_BASIC_INFORMATION& mbi) {
    if(mbi.State!=MEM_COMMIT||mbi.Type!=MEM_PRIVATE||
       (mbi.Protect&PAGE_GUARD)||(mbi.Protect&PAGE_NOACCESS))
        return false;
    const DWORD protect=mbi.Protect&0xffu;
    return protect==PAGE_READWRITE||protect==PAGE_WRITECOPY||
           protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY;
}

static bool SessionLooksValid(std::uintptr_t candidate,std::uintptr_t* player_out=nullptr,
                              std::size_t* automation_count_out=nullptr) {
    const auto module=reinterpret_cast<std::uintptr_t>(latest_module);
    if(!candidate||!module)return false;

    std::uintptr_t vtable=0,dispatcher=0,player=0,begin=0,end=0;
    if(!ReadSelf(reinterpret_cast<const void*>(candidate),vtable)||
       vtable!=module+k133SessionVtableRva||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133SessionDispatcherOffset),dispatcher)||
       !dispatcher||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133SessionPlayerOffset),player)||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133SessionAutomationBeginOffset),begin)||
       !ReadSelf(reinterpret_cast<const void*>(candidate+k133SessionAutomationEndOffset),end))
        return false;

    if((begin==0)!=(end==0)||end<begin||((end-begin)%24u)||(end-begin)>0x100000u)
        return false;

    // SessionTrackPlayer's dispatcher/player are interface objects. Their
    // implementations are not guaranteed to live inside Spotify.dll itself.
    // RC21 incorrectly rejected the exact SessionTrackPlayer object whenever
    // either virtual method landed in another executable module.
    std::uintptr_t dispatcher_vtable=0,dispatch_method=0;
    if(!ReadSelf(reinterpret_cast<const void*>(dispatcher),dispatcher_vtable)||
       !dispatcher_vtable||
       !ReadSelf(reinterpret_cast<const void*>(dispatcher_vtable+0x38),dispatch_method)||
       !ExecutableAddress(dispatch_method))
        return false;

    if(player) {
        std::uintptr_t player_vtable=0,getter=0;
        if(!ReadSelf(reinterpret_cast<const void*>(player),player_vtable)||
           !player_vtable||
           !ReadSelf(reinterpret_cast<const void*>(player_vtable+0x30),getter)||
           !ExecutableAddress(getter))
            return false;
    }

    if(player_out)*player_out=player;
    if(automation_count_out)*automation_count_out=(end-begin)/24u;
    return true;
}

static std::vector<std::uintptr_t> Find133Sessions(bool log_scan) {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto wanted=reinterpret_cast<std::uintptr_t>(latest_module)+k133SessionVtableRva;
    const auto minimum=reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
    std::vector<std::uint8_t> buffer(256*1024);
    std::vector<std::uintptr_t> found;
    std::uint64_t scanned_bytes=0;
    unsigned raw_hits=0,valid_hits=0,active_hits=0;

    for(std::uintptr_t address=minimum;address<maximum;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(reinterpret_cast<const void*>(address),&mbi,sizeof(mbi)))break;
        const auto base=reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto next=base+mbi.RegionSize;
        if(next<=address)break;

        if(WritablePrivate(mbi)&&mbi.RegionSize<=256ull*1024*1024) {
            for(std::uintptr_t chunk=base;chunk<next;) {
                const auto wanted_bytes=static_cast<std::size_t>(
                    (next-chunk)<buffer.size()?(next-chunk):buffer.size());
                SIZE_T copied=0;
                if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(chunk),
                                     buffer.data(),wanted_bytes,&copied)&&copied>=sizeof(std::uintptr_t)) {
                    scanned_bytes+=copied;
                    std::size_t offset=static_cast<std::size_t>((8-(chunk&7u))&7u);
                    for(;offset+sizeof(std::uintptr_t)<=copied;offset+=sizeof(std::uintptr_t)) {
                        std::uintptr_t value=0;
                        std::memcpy(&value,buffer.data()+offset,sizeof(value));
                        if(value!=wanted)continue;
                        ++raw_hits;
                        const auto candidate=chunk+offset;
                        std::uintptr_t player=0;
                        std::size_t automation_count=0;
                        const bool valid=SessionLooksValid(candidate,&player,&automation_count);
                        if(!valid) {
                            if(log_scan&&raw_hits<=8) {
                                std::uintptr_t dispatcher=0,begin=0,end=0;
                                std::uintptr_t dispatcher_vtable=0,dispatch_method=0;
                                std::uintptr_t raw_player=0,player_vtable=0,player_getter=0;
                                double cached_speed=0.0;
                                const bool dispatcher_read=ReadSelf(
                                    reinterpret_cast<const void*>(candidate+k133SessionDispatcherOffset),dispatcher);
                                const bool player_read=ReadSelf(
                                    reinterpret_cast<const void*>(candidate+k133SessionPlayerOffset),raw_player);
                                const bool begin_read=ReadSelf(
                                    reinterpret_cast<const void*>(candidate+k133SessionAutomationBeginOffset),begin);
                                const bool end_read=ReadSelf(
                                    reinterpret_cast<const void*>(candidate+k133SessionAutomationEndOffset),end);
                                const bool cached_read=ReadSelf(
                                    reinterpret_cast<const void*>(candidate+k133SessionCachedSpeedOffset),cached_speed);
                                const bool dispatcher_vtable_read=dispatcher&&ReadSelf(
                                    reinterpret_cast<const void*>(dispatcher),dispatcher_vtable);
                                const bool dispatch_method_read=dispatcher_vtable&&ReadSelf(
                                    reinterpret_cast<const void*>(dispatcher_vtable+0x38),dispatch_method);
                                const bool player_vtable_read=raw_player&&ReadSelf(
                                    reinterpret_cast<const void*>(raw_player),player_vtable);
                                const bool player_getter_read=player_vtable&&ReadSelf(
                                    reinterpret_cast<const void*>(player_vtable+0x30),player_getter);
                                const auto vector_bytes=(begin_read&&end_read&&end>=begin)?end-begin:~std::uintptr_t{0};

                                char line[768];
                                std::snprintf(line,sizeof(line),
                                    "speed_session_raw session=%p dispatcher=%p dispatcher_read=%d dispatcher_vtable=%p dispatcher_vtable_read=%d dispatch_method=%p dispatch_method_read=%d dispatch_exec=%d player=%p player_read=%d player_vtable=%p player_vtable_read=%d player_getter=%p player_getter_read=%d player_getter_exec=%d automation_begin=%p automation_end=%p vector_bytes=%llu cached=%.6f cached_read=%d",
                                    reinterpret_cast<void*>(candidate),reinterpret_cast<void*>(dispatcher),dispatcher_read,
                                    reinterpret_cast<void*>(dispatcher_vtable),dispatcher_vtable_read,
                                    reinterpret_cast<void*>(dispatch_method),dispatch_method_read,
                                    ExecutableAddress(dispatch_method),
                                    reinterpret_cast<void*>(raw_player),player_read,
                                    reinterpret_cast<void*>(player_vtable),player_vtable_read,
                                    reinterpret_cast<void*>(player_getter),player_getter_read,
                                    ExecutableAddress(player_getter),
                                    reinterpret_cast<void*>(begin),reinterpret_cast<void*>(end),
                                    static_cast<unsigned long long>(vector_bytes),cached_speed,cached_read);
                                HistoryLog(line);
                            }
                            continue;
                        }
                        ++valid_hits;
                        if(player)++active_hits;
                        if(found.size()<16)found.push_back(candidate);

                        if(log_scan&&valid_hits<=8) {
                            double cached_speed=0.0;
                            const bool cached_read=ReadSelf(
                                reinterpret_cast<const void*>(candidate+k133SessionCachedSpeedOffset),
                                cached_speed);
                            char line[448];
                            std::snprintf(line,sizeof(line),
                                "speed_session_candidate session=%p player=%p automation=%zu cached=%.6f cached_read=%d source=memory_scan callable=0",
                                reinterpret_cast<void*>(candidate),reinterpret_cast<void*>(player),
                                automation_count,cached_speed,cached_read);
                            HistoryLog(line);
                        }
                    }
                }
                if(wanted_bytes==0)break;
                chunk+=wanted_bytes;
            }
        }
        address=next;
    }

    if(log_scan) {
        char line[384];
        std::snprintf(line,sizeof(line),
            "speed_session_scan wanted_vtable=%p scanned_mib=%.2f raw_hits=%u valid=%u active=%u cached=%zu",
            reinterpret_cast<void*>(wanted),static_cast<double>(scanned_bytes)/(1024.0*1024.0),
            raw_hits,valid_hits,active_hits,found.size());
        HistoryLog(line);
    }
    return found;
}

static bool Apply133Session(std::uintptr_t session,double speed,bool log_change) {
    if(!original_session_setter||!original_session_getter)return false;
    if(!std::isfinite(speed)||speed<1.0||speed>50.0)speed=1.0;

    std::uintptr_t player=0;
    std::size_t automation_count=0;
    if(!SessionLooksValid(session,&player,&automation_count))return false;

    double before=original_session_getter(reinterpret_cast<void*>(session));
    if(!std::isfinite(before)||before<=0.0||before>=100.0)return false;

    const bool before_verified=std::fabs(before-speed)<0.01;
    if(!before_verified)
        original_session_setter(reinterpret_cast<void*>(session),speed,0);

    const double after=original_session_getter(reinterpret_cast<void*>(session));
    const bool after_valid=std::isfinite(after)&&after>0.0&&after<100.0;
    const bool verified=after_valid&&std::fabs(after-speed)<0.01;

    if(player&&verified)
        effective_speed.store(speed,std::memory_order_release);

    const ULONGLONG now=GetTickCount64();
    const bool changed=std::fabs(speed-latest_logged_speed)>0.0001;
    const bool periodic=!verified&&now-latest_failure_log_time>=5000;
    if(log_change&&(changed||periodic)) {
        char line[512];
        std::snprintf(line,sizeof(line),
            "requested=%.3fx effective=%.3fx session=%p player=%p automation=%zu before=%.6f after=%.6f verified=%d",
            speed,effective_speed.load(std::memory_order_acquire),
            reinterpret_cast<void*>(session),reinterpret_cast<void*>(player),
            automation_count,before,after,verified);
        HistoryLog(line);
        LogActivity(verified?"speed_verified":"speed_pending",line);
        if(changed)latest_logged_speed=speed;
        if(!verified)latest_failure_log_time=now;
    }
    return player&&verified;
}

static void Maintain133Session() {
    const ULONGLONG now=GetTickCount64();
    if(now-latest_apply_time<250)return;
    latest_apply_time=now;

    const double requested=GetSettings().playback_speed;
    bool any_verified=false;
    bool have_observed=false;

    // Only call Spotify methods on a SessionTrackPlayer pointer that Spotify
    // itself supplied to one of our hooked setter/getter entry points. RC22
    // invoked methods on memory-scanned candidates during startup and could
    // execute through a partially-constructed interface object.
    const auto observed=latest_session.load(std::memory_order_acquire);
    if(observed&&SessionLooksValid(observed)) {
        have_observed=true;
        if(Apply133Session(observed,requested,true))any_verified=true;
    }

    // Memory scans are diagnostic-only. They may identify the exact vtable,
    // player pointer and cached speed, but never invoke a method on the result.
    if(now-latest_session_scan_time>=3000) {
        latest_session_scan_time=now;
        const bool log_scan=now-latest_session_log_time>=5000;
        auto scanned=Find133Sessions(log_scan);
        (void)scanned;
        if(log_scan)latest_session_log_time=now;
    }

    const auto observed_time=observed_session_time.load(std::memory_order_acquire);
    const auto observed_speed=observed_session_speed.load(std::memory_order_acquire);
    if(now-observed_time<1500&&std::isfinite(observed_speed)&&
       std::fabs(observed_speed-requested)<0.01)
        any_verified=true;

    if(!any_verified)effective_speed.store(1.0,std::memory_order_release);
    if(!have_observed&&requested>1.0&&now-latest_failure_log_time>=5000) {
        latest_failure_log_time=now;
        char line[320];
        std::snprintf(line,sizeof(line),
            "requested=%.3fx effective=1.000x no hook-observed SessionTrackPlayer; scan candidates are diagnostic-only setter_calls=%llu getter_calls=%llu",
            requested,
            session_setter_calls.load(std::memory_order_relaxed),
            session_getter_calls.load(std::memory_order_relaxed));
        HistoryLog(line);
        LogActivity("speed_pending",line);
    }
}

static void Log133PlayerLikeFields(std::uintptr_t candidate) {
    if(!candidate)return;
    const auto module=reinterpret_cast<std::uintptr_t>(latest_module);
    unsigned found=0;
    for(std::size_t offset=0x100;offset<=0xc00;offset+=sizeof(std::uintptr_t)) {
        std::uintptr_t object=0;
        if(!ReadSelf(reinterpret_cast<const void*>(candidate+offset),object)||
           !object||object==candidate)
            continue;

        TrackSpeedSetter setter=nullptr;
        TrackSpeedGetter getter=nullptr;
        if(!PlayerSpeedFunctions(object,setter,getter))continue;

        std::uintptr_t vtable=0;
        ReadSelf(reinterpret_cast<const void*>(object),vtable);
        const auto setter_address=reinterpret_cast<std::uintptr_t>(setter);
        const auto getter_address=reinterpret_cast<std::uintptr_t>(getter);
        const auto vtable_rva=(vtable>=module&&vtable<module+latest_image_size)?vtable-module:0;
        const auto setter_rva=(setter_address>=module&&setter_address<module+latest_image_size)?
            setter_address-module:0;
        const auto getter_rva=(getter_address>=module&&getter_address<module+latest_image_size)?
            getter_address-module:0;

        char line[384];
        std::snprintf(line,sizeof(line),
            "speed_scan_playerlike context=%p offset=0x%04zx object=%p vtable=%p vtable_rva=0x%llx setter_rva=0x%llx getter_rva=0x%llx",
            reinterpret_cast<void*>(candidate),offset,reinterpret_cast<void*>(object),
            reinterpret_cast<void*>(vtable),
            static_cast<unsigned long long>(vtable_rva),
            static_cast<unsigned long long>(setter_rva),
            static_cast<unsigned long long>(getter_rva));
        HistoryLog(line);
        if(++found>=8)break;
    }

    if(!found) {
        char line[192];
        std::snprintf(line,sizeof(line),
            "speed_scan_playerlike context=%p none_in_range=0x100-0xc00",
            reinterpret_cast<void*>(candidate));
        HistoryLog(line);
    }
}

static std::uintptr_t Find133Context() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto wanted=reinterpret_cast<std::uintptr_t>(latest_module)+k133VtableRva;
    const auto minimum=reinterpret_cast<std::uintptr_t>(info.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<std::uintptr_t>(info.lpMaximumApplicationAddress);
    const ULONGLONG now=GetTickCount64();
    const bool log_scan=GetSettings().playback_speed>1.0 &&
        now-latest_scan_log_time>=5000;

    std::vector<std::uint8_t> buffer(256*1024);
    std::vector<std::uintptr_t> current_candidates;
    std::vector<std::uintptr_t> prepared_candidates;
    std::uint64_t writable_regions=0;
    std::uint64_t scanned_bytes=0;
    std::uint64_t readable_chunks=0;
    unsigned vtable_hits=0;
    unsigned valid_layouts=0;
    unsigned inactive_layouts=0;
    unsigned invalid_layouts=0;
    unsigned logged_candidates=0;

    for(std::uintptr_t address=minimum;address<maximum;) {
        MEMORY_BASIC_INFORMATION mbi{};
        if(!VirtualQuery(reinterpret_cast<const void*>(address),&mbi,sizeof(mbi)))break;
        const auto base=reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto next=base+mbi.RegionSize;
        if(next<=address)break;

        if(WritablePrivate(mbi) && mbi.RegionSize<=256ull*1024*1024) {
            ++writable_regions;
            for(std::uintptr_t chunk=base;chunk<next;) {
                const auto wanted_bytes=static_cast<std::size_t>(
                    (next-chunk)<buffer.size()?(next-chunk):buffer.size());
                SIZE_T copied=0;
                if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(chunk),
                                     buffer.data(),wanted_bytes,&copied)&&copied>=sizeof(std::uintptr_t)) {
                    ++readable_chunks;
                    scanned_bytes+=copied;
                    std::size_t offset=static_cast<std::size_t>((8-(chunk&7u))&7u);
                    for(;offset+sizeof(std::uintptr_t)<=copied;offset+=sizeof(std::uintptr_t)) {
                        std::uintptr_t value=0;
                        std::memcpy(&value,buffer.data()+offset,sizeof(value));
                        if(value!=wanted)continue;

                        ++vtable_hits;
                        const auto candidate=chunk+offset;
                        std::uintptr_t current=0,prepared=0,dispatcher=0;
                        const bool current_read=ReadSelf(
                            reinterpret_cast<const void*>(candidate+k133CurrentPlayerOffset),current);
                        const bool prepared_read=ReadSelf(
                            reinterpret_cast<const void*>(candidate+k133PreparedPlayerOffset),prepared);
                        const bool dispatcher_read=ReadSelf(
                            reinterpret_cast<const void*>(candidate+k133DispatcherOffset),dispatcher);
                        const bool current_ok=current_read&&TrackPlayerLooksValid(current);
                        const bool prepared_ok=prepared_read&&TrackPlayerLooksValid(prepared);

                        bool active=false;
                        const bool valid=ContextLooksValid(candidate,&active);
                        if(valid) {
                            ++valid_layouts;
                            if(!active)++inactive_layouts;
                        } else {
                            ++invalid_layouts;
                        }

                        if(log_scan&&logged_candidates<6) {
                            char line[512];
                            std::snprintf(line,sizeof(line),
                                "speed_scan_candidate context=%p valid=%d active=%d current=%p current_read=%d current_ok=%d prepared=%p prepared_read=%d prepared_ok=%d dispatcher=%p dispatcher_read=%d",
                                reinterpret_cast<void*>(candidate),valid,active,
                                reinterpret_cast<void*>(current),current_read,current_ok,
                                reinterpret_cast<void*>(prepared),prepared_read,prepared_ok,
                                reinterpret_cast<void*>(dispatcher),dispatcher_read);
                            HistoryLog(line);
                            Log133PlayerLikeFields(candidate);
                            ++logged_candidates;
                        }

                        if(!valid||!active)continue;
                        if(current)current_candidates.push_back(candidate);
                        else if(prepared)prepared_candidates.push_back(candidate);
                    }
                }
                if(wanted_bytes==0)break;
                chunk+=wanted_bytes;
            }
        }
        address=next;
    }

    // The audible/current TrackPlayer is the strongest signal. A prepared-only
    // ContextPlayer is useful only when there is no current-track candidate.
    std::uintptr_t selected=0;
    if(current_candidates.size()==1)selected=current_candidates.front();
    else if(current_candidates.empty()&&prepared_candidates.size()==1)
        selected=prepared_candidates.front();

    if(log_scan) {
        latest_scan_log_time=now;
        char line[512];
        std::snprintf(line,sizeof(line),
            "speed_scan_summary requested=%.3fx wanted_vtable=%p writable_regions=%llu readable_chunks=%llu scanned_mib=%.2f vtable_hits=%u valid_layouts=%u inactive=%u invalid=%u current_candidates=%zu prepared_candidates=%zu selected=%p",
            GetSettings().playback_speed,reinterpret_cast<void*>(wanted),
            static_cast<unsigned long long>(writable_regions),
            static_cast<unsigned long long>(readable_chunks),
            static_cast<double>(scanned_bytes)/(1024.0*1024.0),
            vtable_hits,valid_layouts,inactive_layouts,invalid_layouts,
            current_candidates.size(),prepared_candidates.size(),
            reinterpret_cast<void*>(selected));
        HistoryLog(line);
    }

    return selected;
}

static bool Apply133(double speed,bool log_change) {
    auto context=latest_context.load(std::memory_order_acquire);
    bool active=false;
    if(!ContextLooksValid(context,&active)||!active) {
        latest_context.store(0,std::memory_order_release);
        effective_speed.store(1.0,std::memory_order_release);
        return false;
    }
    if(!std::isfinite(speed)||speed<1.0||speed>50.0)speed=1.0;

    std::uintptr_t current_player=0,prepared_player=0;
    if(!ReadSelf(reinterpret_cast<const void*>(context+k133CurrentPlayerOffset),current_player)||
       !ReadSelf(reinterpret_cast<const void*>(context+k133PreparedPlayerOffset),prepared_player)) {
        effective_speed.store(1.0,std::memory_order_release);
        return false;
    }

    double current_before=0,current_after=0,prepared_before=0,prepared_after=0;
    bool current_verified=false,prepared_verified=false;

    // First use Spotify's public-internal ContextPlayer wrappers. On 1.3.3 the
    // wrappers can return success without changing music playback, so verify
    // the nested TrackPlayer's real speed afterwards.
    auto current_wrapper=reinterpret_cast<ContextSpeedSetter>(
        reinterpret_cast<std::uint8_t*>(latest_module)+k133CurrentSetterRva);
    auto prepared_wrapper=reinterpret_cast<ContextSpeedSetter>(
        reinterpret_cast<std::uint8_t*>(latest_module)+k133PreparedSetterRva);
    const bool current_result=current_wrapper(reinterpret_cast<void*>(context),speed)!=0;
    const bool prepared_result=prepared_wrapper(reinterpret_cast<void*>(context),speed)!=0;

    if(current_player&&ReadPlayerSpeed(current_player,current_after))
        current_verified=std::fabs(current_after-speed)<0.001;
    if(prepared_player&&ReadPlayerSpeed(prepared_player,prepared_after))
        prepared_verified=std::fabs(prepared_after-speed)<0.001;

    // Spotify 1.2.67+ can hard-block the ContextPlayer speed path for music.
    // The wrappers themselves show the actual low-level ABI: TrackPlayer
    // vtable +0xc0 is the setter and +0xc8 is the getter. If the wrapper was a
    // no-op, apply the exact same TrackPlayer call directly and verify it.
    if(current_player&&!current_verified) {
        SetPlayerSpeedDirect(current_player,speed,0,current_before,current_after);
        current_verified=std::fabs(current_after-speed)<0.001;
    }
    if(prepared_player&&!prepared_verified) {
        SetPlayerSpeedDirect(prepared_player,speed,2,prepared_before,prepared_after);
        prepared_verified=std::fabs(prepared_after-speed)<0.001;
    }

    const bool verified=current_verified||(!current_player&&prepared_verified);
    effective_speed.store(verified?speed:1.0,std::memory_order_release);

    const ULONGLONG now=GetTickCount64();
    const bool changed=std::fabs(speed-latest_logged_speed)>0.0001;
    const bool should_log_failure=!verified && now-latest_failure_log_time>=5000;
    if(log_change&&(changed||should_log_failure)) {
        char line[768];
        std::snprintf(line,sizeof(line),
            "requested=%.3fx effective=%.3fx context=%p current=%p prepared=%p wrapper_current=%d wrapper_prepared=%d current_before=%.6f current_after=%.6f current_verified=%d prepared_before=%.6f prepared_after=%.6f prepared_verified=%d",
            speed,effective_speed.load(std::memory_order_acquire),
            reinterpret_cast<void*>(context),reinterpret_cast<void*>(current_player),
            reinterpret_cast<void*>(prepared_player),current_result,prepared_result,
            current_before,current_after,current_verified,prepared_before,prepared_after,prepared_verified);
        HistoryLog(line);
        LogActivity(verified?"speed_verified":"speed_failed",line);
        if(changed)latest_logged_speed=speed;
        if(!verified)latest_failure_log_time=now;
    }
    return verified;
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

static bool Start133PcmBackend(HMODULE module,std::size_t image_size,
                               std::uint64_t now) {
    effective_speed.store(1.0,std::memory_order_release);
    latest_module=module;
    latest_image_size=image_size;
    pcm_process_calls.store(0,std::memory_order_release);
    pcm_thinned_calls.store(0,std::memory_order_release);
    pcm_last_requested.store(GetSettings().playback_speed,std::memory_order_release);

    if(!Verify133PcmProcess(module,image_size)) {
        HistoryLog("Spotify 1.3.3 classic PCM playback-speed target verification failed; staying at 1x");
        speed_init.MarkUnsupported();
        return false;
    }

    void* target=reinterpret_cast<std::uint8_t*>(module)+k133PcmProcessRva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(target,reinterpret_cast<void*>(PcmProcessHook),
                             reinterpret_cast<void**>(&original_pcm_process));
    if(status==MH_OK) {
        active_backend.store(PlaybackSpeedBackend::PcmThin133,std::memory_order_release);
        status=MH_EnableHook(target);
        if(status!=MH_OK&&status!=MH_ERROR_ENABLED) {
            active_backend.store(PlaybackSpeedBackend::Unsupported,std::memory_order_release);
            MH_RemoveHook(target);
            original_pcm_process=nullptr;
        }
    }

    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[384];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 classic PCM playback-speed backend active at Spotify.dll+0x%08x vtable_slot=Spotify.dll+0x%08x",
            k133PcmProcessRva,k133PcmProcessVtableSlotRva);
        HistoryLog(line);
        return true;
    }

    char line[256];
    std::snprintf(line,sizeof(line),
        "Spotify 1.3.3 classic PCM playback-speed hook failed: %s",
        MH_StatusToString(status));
    HistoryLog(line);
    if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
        speed_init.MarkUnsupported();
    else speed_init.Retry(now);
    return false;
}

static bool Start133SessionBackend(HMODULE module,std::size_t image_size,
                                   std::uint64_t now) {
    effective_speed.store(1.0,std::memory_order_release);
    latest_module=module;
    latest_image_size=image_size;
    latest_session.store(0,std::memory_order_release);
    observed_session_speed.store(1.0,std::memory_order_release);
    observed_session_time.store(0,std::memory_order_release);
    session_setter_calls.store(0,std::memory_order_release);
    session_getter_calls.store(0,std::memory_order_release);

    if(!Verify133SessionLayout(module,image_size)) {
        HistoryLog("Spotify 1.3.3 SessionTrackPlayer playback-speed layout verification failed; staying at 1x");
        speed_init.MarkUnsupported();
        return false;
    }

    void* setter_target=reinterpret_cast<std::uint8_t*>(module)+k133SessionSetterRva;
    void* getter_target=reinterpret_cast<std::uint8_t*>(module)+k133SessionGetterRva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(setter_target,reinterpret_cast<void*>(SessionSpeedHook),
                             reinterpret_cast<void**>(&original_session_setter));
    if(status==MH_OK)
        status=MH_CreateHook(getter_target,reinterpret_cast<void*>(SessionGetterHook),
                             reinterpret_cast<void**>(&original_session_getter));
    if(status!=MH_OK) {
        MH_RemoveHook(getter_target);
        MH_RemoveHook(setter_target);
        original_session_setter=nullptr;
        original_session_getter=nullptr;
    } else {
        active_backend.store(PlaybackSpeedBackend::SessionPlayer133,std::memory_order_release);
        status=MH_EnableHook(setter_target);
        if(status==MH_OK||status==MH_ERROR_ENABLED)status=MH_EnableHook(getter_target);
        if(status!=MH_OK&&status!=MH_ERROR_ENABLED) {
            MH_DisableHook(getter_target);
            MH_DisableHook(setter_target);
            MH_RemoveHook(getter_target);
            MH_RemoveHook(setter_target);
            original_session_setter=nullptr;
            original_session_getter=nullptr;
            active_backend.store(PlaybackSpeedBackend::Unsupported,std::memory_order_release);
        }
    }

    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[384];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 SessionTrackPlayer playback-speed backend active setter=Spotify.dll+0x%08x getter=Spotify.dll+0x%08x vtable=Spotify.dll+0x%08x",
            k133SessionSetterRva,k133SessionGetterRva,k133SessionVtableRva);
        HistoryLog(line);
        return true;
    }

    char line[256];
    std::snprintf(line,sizeof(line),
        "Spotify 1.3.3 SessionTrackPlayer playback-speed hook failed: %s",
        MH_StatusToString(status));
    HistoryLog(line);
    if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
        speed_init.MarkUnsupported();
    else speed_init.Retry(now);
    return false;
}

static bool Start133TrackCreateBackend(HMODULE module,std::size_t image_size,
                                       std::uint64_t now) {
    effective_speed.store(1.0,std::memory_order_release);
    latest_module=module;
    latest_image_size=image_size;

    if(!Verify133TrackCreate(module,image_size)) {
        HistoryLog("Spotify 1.3.3 track-player creation layout verification failed; staying at 1x");
        speed_init.MarkUnsupported();
        return false;
    }

    void* target=reinterpret_cast<std::uint8_t*>(module)+k133TrackCreateRva;
    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(target,reinterpret_cast<void*>(Hook),
                             reinterpret_cast<void**>(&original));
    if(status==MH_OK) {
        // Publish the backend before threads are resumed with the detour active,
        // so the very first TrackPlayer creation can update effective_speed.
        active_backend.store(PlaybackSpeedBackend::TrackCreate133,std::memory_order_release);
        status=MH_EnableHook(target);
        if(status!=MH_OK&&status!=MH_ERROR_ENABLED)
            active_backend.store(PlaybackSpeedBackend::Unsupported,std::memory_order_release);
    }

    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        supported.store(true,std::memory_order_release);
        speed_init.Activate();
        char line[320];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 exact track-player creation speed hook active at Spotify.dll+0x%08x; speed is XMM3/argument4 and applies when a TrackPlayer is created",
            k133TrackCreateRva);
        HistoryLog(line);
        return true;
    }

    char line[240];
    std::snprintf(line,sizeof(line),
        "Spotify 1.3.3 exact track-player creation hook failed: %s",
        MH_StatusToString(status));
    HistoryLog(line);
    if(status==MH_ERROR_NOT_EXECUTABLE||status==MH_ERROR_UNSUPPORTED_FUNCTION)
        speed_init.MarkUnsupported();
    else speed_init.Retry(now);
    return false;
}

static bool Start133Backend(HMODULE module,std::size_t image_size) {
    effective_speed.store(1.0,std::memory_order_release);
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
    {
        char line[512];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 ContextPlayer playback-speed backend validated base=%p image_size=0x%zx vtable=%p current_wrapper=%p prepared_wrapper=%p current_offset=0x%zx prepared_offset=0x%zx dispatcher_offset=0x%zx",
            reinterpret_cast<void*>(module),image_size,
            reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module)+k133VtableRva),
            reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module)+k133CurrentSetterRva),
            reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(module)+k133PreparedSetterRva),
            k133CurrentPlayerOffset,k133PreparedPlayerOffset,k133DispatcherOffset);
        HistoryLog(line);
    }
    return true;
}

}

bool PlaybackSpeedSupported() {
    return supported.load(std::memory_order_acquire);
}

bool PlaybackSpeedImmediate() {
    const auto backend=active_backend.load(std::memory_order_acquire);
    return backend==PlaybackSpeedBackend::PcmThin133||
           backend==PlaybackSpeedBackend::SessionPlayer133||
           backend==PlaybackSpeedBackend::ContextSetter133;
}

double PlaybackSpeedEffective() {
    const auto backend=active_backend.load(std::memory_order_acquire);
    if(backend==PlaybackSpeedBackend::ConstructorHook&&supported.load(std::memory_order_acquire))
        return std::max(1.0,GetSettings().playback_speed);
    return effective_speed.load(std::memory_order_acquire);
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
    if(backend==PlaybackSpeedBackend::PcmThin133) {
        Start133PcmBackend(module,image_size,now);
        return;
    }
    if(backend==PlaybackSpeedBackend::SessionPlayer133) {
        Start133SessionBackend(module,image_size,now);
        return;
    }
    if(backend==PlaybackSpeedBackend::TrackCreate133) {
        Start133TrackCreateBackend(module,image_size,now);
        return;
    }
    if(backend==PlaybackSpeedBackend::ContextSetter133) {
        Start133Backend(module,image_size);
        return;
    }

    speed_init.MarkUnsupported();
}

void MaintainPlaybackSpeed(HMODULE module) {
    const auto backend=active_backend.load(std::memory_order_acquire);
    if(!supported.load(std::memory_order_acquire)||module!=latest_module)
        return;
    if(backend==PlaybackSpeedBackend::PcmThin133)return;
    if(backend==PlaybackSpeedBackend::SessionPlayer133) {
        Maintain133Session();
        return;
    }
    if(backend!=PlaybackSpeedBackend::ContextSetter133)return;

    const ULONGLONG now=GetTickCount64();
    if(now-latest_apply_time<250)return;
    latest_apply_time=now;

    auto context=latest_context.load(std::memory_order_acquire);
    bool active=false;
    if(!ContextLooksValid(context,&active)||!active) {
        latest_context.store(0,std::memory_order_release);
        effective_speed.store(1.0,std::memory_order_release);

        // RC17 could permanently cache the one structurally valid ContextPlayer
        // that existed during startup even though it had no current/prepared
        // TrackPlayer. Keep rescanning until a genuinely active player appears.
        if(now-latest_scan_time<750)return;
        latest_scan_time=now;
        context=Find133Context();
        if(!context)return;

        latest_context.store(context,std::memory_order_release);

        std::uintptr_t current=0,prepared=0;
        ReadSelf(reinterpret_cast<const void*>(context+k133CurrentPlayerOffset),current);
        ReadSelf(reinterpret_cast<const void*>(context+k133PreparedPlayerOffset),prepared);
        char line[256];
        std::snprintf(line,sizeof(line),
            "Spotify 1.3.3 active ContextPlayer located at %p current=%p prepared=%p",
            reinterpret_cast<void*>(context),reinterpret_cast<void*>(current),
            reinterpret_cast<void*>(prepared));
        HistoryLog(line);
    }

    Apply133(GetSettings().playback_speed,true);
}

void ApplyPlaybackSpeedNow() {
    const auto backend=active_backend.load(std::memory_order_acquire);
    if(backend==PlaybackSpeedBackend::PcmThin133) {
        // The next PCM process callback picks up the new setting immediately.
        return;
    }
    if(backend==PlaybackSpeedBackend::SessionPlayer133) {
        latest_apply_time=0;
        latest_session_scan_time=0;
        return;
    }
    if(backend!=PlaybackSpeedBackend::ContextSetter133)return;

    latest_apply_time=0;
    auto context=latest_context.load(std::memory_order_acquire);
    bool active=false;
    if(!ContextLooksValid(context,&active)||!active) {
        // Do not run a full process-memory scan on the UI/settings caller.
        // Wake the normal maintenance path so it can find the active player.
        latest_context.store(0,std::memory_order_release);
        latest_scan_time=0;
        effective_speed.store(1.0,std::memory_order_release);
        return;
    }
    Apply133(GetSettings().playback_speed,true);
}

}
