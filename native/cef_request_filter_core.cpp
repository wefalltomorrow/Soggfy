#include "cef_request_filter_core.h"
namespace history {
bool ShouldBlockClassicTelemetryUrl(std::wstring_view url) {
    // Soggfy only owns privacy/telemetry filtering. Ad blocking and Spotify
    // update blocking are delegated to SpotX so the two projects do not fight
    // over the same request paths or client files.
    static constexpr std::wstring_view prefixes[]={
        L"https://spclient.wg.spotify.com/gabo-receiver-service/",
        L"https://spclient.wg.spotify.com/dodo-receiver-service/"
    };
    for(auto prefix:prefixes)if(url.rfind(prefix,0)==0)return true;
    return false;
}
}
