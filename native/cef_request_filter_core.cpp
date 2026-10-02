#include "cef_request_filter_core.h"
namespace history {
bool ShouldBlockClassicTelemetryUrl(std::wstring_view url) {
    static constexpr std::wstring_view prefixes[]={
        L"https://spclient.wg.spotify.com/ads/",
        L"https://spclient.wg.spotify.com/ad-logic/",
        L"https://spclient.wg.spotify.com/gabo-receiver-service/",
        L"https://spclient.wg.spotify.com/dodo-receiver-service/"
    };
    for(auto prefix:prefixes)if(url.rfind(prefix,0)==0)return true;
    return false;
}
}
