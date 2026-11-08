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
    // RC64: Windows delivery may lag a sample. Use observation timestamps for
    // both the accelerated clock and Listen, never the later delivery time.
    MediaPositionClock thirty;history::Listen sampled,delivered;
    sampled.Observe("30x",0,180,true,100,30);
    delivered.Observe("30x",0,180,true,100.70,30);
    thirty.Observe("30x",0,180,true,30,100);
    const auto second=thirty.Observe("30x",0.5,180,true,30,100.5);
    check(second==15,"30x sample uses observation time before delayed delivery");
    sampled.Observe("30x",second,180,true,100.5,30);
    delivered.Observe("30x",second,180,true,100.75,30);
    check(sampled.eligible&&!delivered.eligible,"variable delivery delays cannot create false position-ahead rejection");
    for(int i=2;i<12;++i) {
        const double t=100+i*0.5;
        const auto position=thirty.Observe("30x",i*0.5,180,true,30,t);
        check(sampled.Observe("30x",position,180,true,t,30).empty()&&sampled.eligible,
              "30x complete coverage survives refreshed 1x timeline");
    }
    check(sampled.Observe("next",0,180,true,106,30)=="30x","30x natural transition retains complete listen");
    MediaPositionClock paused;history::Listen paused_listen;
    auto sample=[&](bool playing,double time,double rate=30) {
        const auto position=paused.Observe("pause",0,180,playing,rate,time);
        paused_listen.Observe("pause",position,180,playing,time,rate);return position;
    };
    sample(true,100);check(sample(true,100.5)==15,"30x progresses before pause");
    check(sample(false,101)==15&&sample(false,102)==15,"paused observations never invent progress");
    check(sample(true,102.5)==15&&sample(true,103)==30&&paused_listen.eligible,
          "30x pause/resume preserves coverage without pause-time advance");
    // The clock accounts for the last interval at its previous rate; after
    // Downloads-off's effective-1x change, subsequent intervals advance at 1x.
    check(paused.Observe("pause",0,180,true,1,103.5)==45,"speed change preserves previous interval rate");
    check(paused.Observe("pause",0,180,true,1,104)==45.5,"effective 1x advances only half a second");
    history::Listen gap;gap.Observe("gap",0,180,true,100,30);
    gap.Observe("gap",105,180,true,103.5,30);
    check(!gap.eligible,"a real observation gap still rejects 30x coverage");
    std::puts("PASS: accelerated timeline stability, delayed observations, 30x pause/resume and complete listens");
}
