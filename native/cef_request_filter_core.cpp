#include "cef_request_filter_core.h"

namespace history {
namespace {

static bool StartsWith(std::wstring_view value,std::wstring_view prefix) {
    return value.size()>=prefix.size()&&value.substr(0,prefix.size())==prefix;
}

static bool EndsWith(std::wstring_view value,std::wstring_view suffix) {
    return value.size()>=suffix.size()&&
           value.substr(value.size()-suffix.size())==suffix;
}

}

bool ShouldBlockClassicTelemetryUrl(std::wstring_view url) {
    constexpr std::wstring_view scheme=L"https://";
    if(!StartsWith(url,scheme))return false;

    const auto host_start=scheme.size();
    const auto slash=url.find(L'/',host_start);
    if(slash==std::wstring_view::npos)return false;

    const auto host=url.substr(host_start,slash-host_start);
    const auto path=url.substr(slash);

    const bool wg=host==L"spclient.wg.spotify.com";
    const bool regional=EndsWith(host,L"-spclient.spotify.com");

    // Current Spotify ad endpoints are served from both the historic wg host
    // and regional *-spclient.spotify.com hosts. Keep the match path-scoped so
    // ordinary metadata, login and audio requests remain untouched.
    if((wg||regional) &&
       (StartsWith(path,L"/ads/")||StartsWith(path,L"/ad-logic/")))
        return true;

    // These tracking receivers are currently specific to the wg endpoint.
    if(wg &&
       (StartsWith(path,L"/gabo-receiver-service/")||
        StartsWith(path,L"/dodo-receiver-service/")))
        return true;

    return false;
}
}
