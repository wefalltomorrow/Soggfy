#include "../native/cef_request_filter_core.h"
#include <cstdio>
#include <cstdlib>
using namespace history;
static void check(bool value,const char* name){if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
int main(){
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ads/v1/foo"),"wg ads");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ad-logic/something"),"wg ad logic");
    check(ShouldBlockClassicTelemetryUrl(L"https://gew1-spclient.spotify.com/ads/v2/config"),"regional ads");
    check(ShouldBlockClassicTelemetryUrl(L"https://guc3-spclient.spotify.com/ad-logic/state/config"),"regional ad logic");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/gabo-receiver-service/event"),"wg gabo");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/dodo-receiver-service/event"),"wg dodo");

    // Hostnames are intentionally irrelevant: Spotify can move these endpoints
    // between regional hosts without requiring another Soggfy release.
    check(ShouldBlockClassicTelemetryUrl(L"https://new-region.example/ads/v3/config?x=1"),"host-independent ads");
    check(ShouldBlockClassicTelemetryUrl(L"http://127.0.0.1/ad-logic/test#fragment"),"host-independent ad logic");
    check(ShouldBlockClassicTelemetryUrl(L"https://future.spotify.example/gabo-receiver-service/event"),"host-independent gabo");

    check(!ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/metadata/4/track/foo"),"metadata allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://gew1-spclient.spotify.com/metadata/4/track/foo"),"regional metadata allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://audio-fa.scdn.co/audio/foo"),"audio CDN allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://upgrade.scdn.co/upgrade/client"),"client updates allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://example.com/notads/ads/foo"),"embedded ads token allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"not-a-url/ads/foo"),"invalid URL allowed");
    std::puts("PASS: classic ad/telemetry URL filter");
}
