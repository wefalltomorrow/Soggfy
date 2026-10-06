#include "../native/cef_request_filter_core.h"
#include <cstdio>
#include <cstdlib>
using namespace history;
static void check(bool value,const char* name){if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
int main(){
    check(!ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ads/v1/foo"),"ads delegated to SpotX");
    check(!ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/ad-logic/something"),"ad logic delegated to SpotX");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/gabo-receiver-service/event"),"gabo");
    check(ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/dodo-receiver-service/event"),"dodo");
    check(!ShouldBlockClassicTelemetryUrl(L"https://spclient.wg.spotify.com/metadata/4/track/foo"),"metadata allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://audio-fa.scdn.co/audio/foo"),"audio CDN allowed");
    check(!ShouldBlockClassicTelemetryUrl(L"https://upgrade.scdn.co/upgrade/client"),"client updates delegated to SpotX");
    std::puts("PASS: classic telemetry URL filter");
}
