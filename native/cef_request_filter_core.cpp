#include "cef_request_filter_core.h"

namespace history {
namespace {

static bool StartsWith(std::wstring_view value,std::wstring_view prefix) {
    return value.size()>=prefix.size()&&value.substr(0,prefix.size())==prefix;
}

static std::wstring_view UrlPath(std::wstring_view url) {
    const auto scheme=url.find(L"://");
    if(scheme==std::wstring_view::npos)return {};

    const auto authority=scheme+3;
    const auto start=url.find_first_of(L"/?#",authority);
    if(start==std::wstring_view::npos||url[start]!=L'/')return {};

    const auto end=url.find_first_of(L"?#",start);
    return url.substr(start,end==std::wstring_view::npos?
        std::wstring_view::npos:end-start);
}

}

bool ShouldBlockClassicTelemetryUrl(std::wstring_view url) {
    const auto path=UrlPath(url);
    if(path.empty())return false;

    // Match Spotify's ad/telemetry endpoints by path rather than by hostname.
    // BlockTheSpot uses the same strategy because Spotify rotates requests
    // between wg and regional *-spclient hosts. Keeping the rules path-scoped
    // avoids accidentally blocking metadata, audio CDN, login or other traffic.
    return StartsWith(path,L"/ads/")||
           StartsWith(path,L"/ad-logic/")||
           StartsWith(path,L"/gabo-receiver-service/")||
           StartsWith(path,L"/dodo-receiver-service/");
}
}
