#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "cef_request_filter.h"
#include "cef_request_filter_core.h"
#include "history_settings.h"
#include "hook_init_state.h"
#include "vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cstring>

namespace history {
namespace {
struct Base {
    size_t size;
    void(*add_ref)(Base*);
    int(*release)(Base*);
    int(*has_one_ref)(Base*);
    int(*has_at_least_one_ref)(Base*);
};
struct CefString {
    wchar_t* str;
    size_t length;
    void(*dtor)(wchar_t*);
};
struct Request {
    Base base;
    int(*is_read_only)(Request*);
    CefString*(*get_url)(Request*);
};
using CreateUrlRequest=void*(*)(Request*,void*,void*);
using FreeUserString=void(*)(CefString*);
static CreateUrlRequest original_create=nullptr;
static FreeUserString free_user_string=nullptr;
static hooks::InitController filter_init;
static std::atomic<unsigned> blocked_count{0};

static void* CreateHook(Request* request,void* client,void* context) {
    if(request && request->base.size>=sizeof(Request) && request->get_url &&
       GetSettings().block_telemetry) {
        CefString* url=request->get_url(request);
        if(url) {
            bool blocked=false;
            if(url->str && url->length && url->length<=16384)
                blocked=ShouldBlockClassicTelemetryUrl(std::wstring_view(url->str,url->length));
            if(free_user_string)free_user_string(url);
            if(blocked) {
                unsigned count=blocked_count.fetch_add(1,std::memory_order_relaxed)+1;
                if(count<=16)HistoryLog("classic telemetry request blocked");
                return nullptr;
            }
        }
    }
    return original_create?original_create(request,client,context):nullptr;
}
}

void StartCefRequestFilter(HMODULE cef) {
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    if(filter_init.State()==hooks::InitState::Active)return;
    if(!filter_init.TryBegin(now))return;
    if(!cef){filter_init.Retry(now);return;}

    auto address=GetProcAddress(cef,"cef_urlrequest_create");
    auto free_address=GetProcAddress(cef,"cef_string_userfree_utf16_free");
    if(!address||!free_address) {
        HistoryLog("classic telemetry filter unavailable: CEF request exports missing");
        filter_init.MarkUnsupported();
        return;
    }
    static_assert(sizeof(free_user_string)==sizeof(free_address));
    std::memcpy(&free_user_string,&free_address,sizeof(free_address));

    MH_STATUS status=MH_Initialize();
    if(status==MH_ERROR_ALREADY_INITIALIZED)status=MH_OK;
    if(status==MH_OK)
        status=MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(CreateHook),
                             reinterpret_cast<void**>(&original_create));
    if(status==MH_ERROR_ALREADY_CREATED) {
        HistoryLog("classic telemetry filter unavailable: request hook already owned");
        filter_init.MarkUnsupported();
        return;
    }
    if(status==MH_OK)status=MH_EnableHook(reinterpret_cast<void*>(address));
    if(status==MH_OK||status==MH_ERROR_ENABLED) {
        filter_init.Activate();
        HistoryLog("classic telemetry filter active");
    } else {
        HistoryLog(("classic telemetry filter failed: "+std::string(MH_StatusToString(status))).c_str());
        if(status!=MH_ERROR_NOT_EXECUTABLE&&status!=MH_ERROR_UNSUPPORTED_FUNCTION)
            filter_init.Retry(now);
        else filter_init.MarkUnsupported();
    }
}
}
