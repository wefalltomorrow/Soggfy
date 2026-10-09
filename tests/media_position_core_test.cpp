#include "../native/media_position_core.h"
#include "../native/ogg_history_core.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using history::MediaPositionClock;
static void check(bool pass,const char* name) {
    if(!pass) {std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}
}
int main() {
    MediaPositionClock clock;
    const double start=clock.Observe("Magumba",3.840,572.080,true,50.0,0.0);
    check(std::fabs(start-3.840)<0.0001,"first accelerated poll uses public timeline");
    const double a=clock.Observe("Magumba",190.918,572.080,true,50.0,3.742);
    check(std::fabs(a-190.940)<0.05,"normal accelerated playback advances");
    const double b=clock.Observe("Magumba",13.429,572.080,true,50.0,4.257);
    check(b>=a+25.7,"stale SMTC timestamp cannot rewind 50x playback");
    check(b<=a+26.0,"stale SMTC timestamp follows expected wall-clock advance");
    const double pause=clock.Observe("Magumba",4.5,572.080,false,50.0,4.8);
    check(pause>=b,"pause preserves established accelerated progress");
    const double still=clock.Observe("Magumba",5.0,572.080,false,50.0,5.3);
    check(still==pause,"paused clock does not advance artificially");
    const double resume=clock.Observe("Magumba",5.5,572.080,true,50.0,5.8);
    check(resume==still,"resuming preserves progress");
    const double changed=clock.Observe("Magumba",6.0,572.080,true,1.0,6.3);
    check(changed>resume,"changing speed does not cause a rewind");
    const double next=clock.Observe("Empty Branes",11.782,618.320,true,50.0,6.8);
    check(std::fabs(next-11.782)<0.0001,"new media identity resets clock");
    MediaPositionClock regular;
    regular.Observe("Normal",0.0,200.0,true,1.0,0.0);
    check(regular.Observe("Normal",1.0,200.0,true,1.0,1.0)==1.0,"1x behavior unchanged");
    check(regular.Observe("Normal",0.2,200.0,true,1.0,2.0)==0.2,"1x rewind remains visible");

    MediaPositionClock playback;
    history::Listen listener;
    check(listener.Observe("magumba",playback.Observe("magumba",0.0,572.08,true,50,0.0),
                           572.08,true,0.0,50).empty(),"track start");
    for(int i=1;i<=22;i++) {
        const double t=i*0.5;
        const double stale=std::fmod(t,4.0)*50.0+std::floor(t/4.0)*4.0;
        const double stable=playback.Observe("magumba",stale,572.08,true,50,t);
        check(listener.Observe("magumba",stable,572.08,true,t,50).empty(),
              "no premature completion while Spotify timestamp refreshes");
        check(listener.eligible,"50x full listen stays eligible after timeline refresh");
    }
    check(listener.Observe("next",3.0,618.0,true,11.5,50)=="magumba",
          "natural 50x transition produces a completed listen");
    std::puts("PASS: accelerated timeline stability and completed 50x listens");
}
