#include "../native/cef_request_filter_core.h"
#include <cstdio>
#include <cstdlib>
using namespace history;
static void check(bool value,const char* name){if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
int main(){
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ads/v1/foo"),"ads");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ad-logic/something"),"ad logic");
    check(ShouldBlockClassicTelemetryUrl(L"https://gew1-spclient.spotify.com/ads/v2/config"),"regional ads");
    check(ShouldBlockClassicTelemetryUrl(L"https://guc3-spclient.spotify.com/ad-logic/state/config"),"regional ad logic");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/gabo-receiver-service/event"),"gabo");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/dodo-receiver-service/event"),"dodo");
    check(!ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/metadata/4/track/foo"),"metadata allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://gew1-spclient.spotify.com/metadata/4/track/foo"),"regional metadata allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://notspclient.spotify.com/ads/foo"),"unrelated spotify host allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://audio-fa.scdn.co/audio/foo"),"audio CDN allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://upgrade.scdn.co/upgrade/client"),"client updates allowed");
    std::puts("PASS: classic telemetry URL filter");
}
