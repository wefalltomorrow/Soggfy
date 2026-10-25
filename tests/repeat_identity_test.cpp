#include "../native/ogg_history_core.h"
#include "../native/media_position_core.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace history;
static bool near(double a,double b) {return std::fabs(a-b)<0.00001;}
struct Playback {
    MediaPositionClock clock;Listen listen;double rate;
    explicit Playback(double r):rate(r){}
    std::string sample(const char* key,double raw,double length,double time,bool playing=true) {
        RawTimeline timeline{raw,100000+std::int64_t(time*1000)};
        const auto position=clock.Observe(key,raw,length,playing,rate,time,&timeline);
        return listen.Observe(key,position,length,playing,time,rate);
    }
    void first() {
        for(int i=0;i<12;++i) {
            assert(sample("a",i*0.5,rate*6,i*0.5).empty());assert(listen.eligible);
        }
    }
};
static Listen ending(double rate=1) {
    Listen l;for(int i=0;i<12;++i)l.Observe("a",i*0.5*rate,6*rate,true,i*0.5,rate);
    return l;
}
int main() {
    for(double rate:{1.,2.,10.,30.}) {
        Playback p(rate);p.first();
        assert(p.sample("a",0,rate*6,6).empty() && p.listen.transient);
        assert(p.clock.position==0); // RC65's accelerated clock concealed this reset.
        assert(p.sample("a",0.5,rate*6,6.5).empty());
        assert(p.sample("a",1,rate*6,7)=="a");
        assert(p.listen.generation==2 && p.listen.completed_generation==1);
        assert(near(p.listen.completed_start,0) && near(p.listen.completed_time,6));
        assert(near(p.listen.start_time,6) && p.listen.phase==ListenPhase::ProvisionalRepeat);
        for(int i=3;i<12;++i) {
            assert(p.sample("a",i*0.5,rate*6,6+i*0.5).empty());assert(p.listen.eligible);
        }
        assert(p.sample("a",0,rate*6,12).empty());
        assert(p.sample("a",0.5,rate*6,12.5).empty());
        assert(p.sample("a",1,rate*6,13)=="a");
        assert(p.listen.generation==3 && p.listen.completed_generation==2);
        assert(near(p.listen.completed_start,6) && near(p.listen.completed_time,12));
    }
    for(double rate:{1.,30.})for(bool changed_length:{false,true}) {
        Playback p(rate);p.first();const double length=rate*(changed_length?9:6);
        p.sample("a",0,length,6);p.sample("a",0.5,length,6.5);
        const auto first=p.sample("a",1,length,7);
        assert(first==(changed_length?"":"a"));
        const auto label=p.sample("b",1.5,length,7.5);
        assert(label==(changed_length?"a":""));
        assert(p.listen.identity=="b" && p.listen.generation==2 && p.listen.eligible);
        assert(near(p.listen.start_time,6) && near(p.clock.position,1.5*rate));
        assert(p.listen.completed_generation==1 && near(p.listen.completed_duration,rate*6));
        const int ticks=changed_length?18:12;
        for(int i=4;i<ticks;++i)assert(p.sample("b",i*0.5,length,6+i*0.5).empty());
        assert(p.sample("c",0,rate*6,6+ticks*0.5)=="b");
        assert(p.listen.completed_generation==2 && near(p.listen.completed_start,6));
    }
    {
        // Timestamp refresh can reduce EXTRAPOLATED position, while raw position
        // advances. It must retain monotonic progress, including near track end.
        MediaPositionClock c;RawTimeline raw{0,1};c.Observe("a",0,180,true,30,0,&raw);
        for(int i=1;i<=12;++i) {
            raw={i*0.5,i+1};
            const auto pos=c.Observe("a",i==11?0.1:i*0.5,180,true,30,i*0.5,&raw);
            assert(near(pos,i*15));
        }
        // An older timeline cannot manufacture a wrap.
        raw={0,1};assert(c.Observe("a",0,180,true,30,6.5,&raw)==180);
    }
    {
        Playback p(30);for(int i=0;i<8;++i)p.sample("a",i*0.5,180,i*0.5);
        assert(p.sample("a",0,180,4).empty());
        assert(!p.listen.eligible && p.listen.reject==ListenReject::PositionRewind);
        assert(p.sample("b",0,180,4.5).empty());
    }
    for(int cause=0;cause<6;++cause) {
        auto l=ending();l.Observe("a",0,6,true,6);
        if(cause==0)l.Invalidate(); // missing snapshot / overflow must not revive coverage
        if(cause==1)l.eligible=false; // defensive even with a direct external invalidation
        const double time=cause==2?8.01:7;
        const double pos=cause==3?4:1;
        assert(l.Observe("a",pos,cause==4?8:6,cause!=5,time).empty());
        assert(!l.eligible && !l.pending_start && l.phase==ListenPhase::Tracking);
        assert(l.Observe("b",1.5,6,true,8.5).empty());
    }
    {
        auto l=ending();l.Observe("a",0,6,true,6);
        assert(l.Observe("a",1,6,true,7)=="a");
        auto paused=l;assert(paused.Observe("b",1,6,false,7.5).empty() && paused.eligible);
        assert(paused.generation==2 && near(paused.start_time,6));
        auto seek=l;assert(seek.Observe("b",4,6,true,7.5).empty() && !seek.eligible);
        assert(seek.generation==3);
        // The original two-second window is inclusive, never extended on confirmation.
        auto edge=l;assert(edge.Observe("b",2,6,true,8).empty() && edge.generation==2);
        auto expired=l;assert(expired.Observe("b",2.01,6,true,8.01).empty());
        assert(expired.generation==3 && !expired.eligible);
    }
    {
        auto l=ending();l.Observe("a",0,6,true,6);
        assert(l.Observe("a",1,6,true,7)=="a");
        l.Observe("a",1.5,6,true,7.5);
        assert(l.Observe("a",2.5,6,true,8.5).empty() && l.eligible);
        assert(l.phase==ListenPhase::Tracking); // label-window expiry is not repeat failure
    }
    {
        // Title first, timeline second; no synthetic completion of the previous skip.
        Listen l;l.Observe("a",0,100,true,0);l.Observe("a",1,100,true,1);
        assert(l.Observe("b",50,100,true,1.5).empty() && l.pending_start);
        l.Observe("b",0,100,true,2);assert(l.eligible && near(l.start_time,2));
    }
    {
        Playback p(30);p.sample("a",0,180,0);p.sample("a",0.5,180,0.5);
        p.rate=1;p.sample("a",1,180,1);assert(near(p.clock.position,30) && p.listen.eligible);
        p.sample("a",1.5,180,1.5);assert(near(p.clock.position,30.5) && p.listen.eligible);
        p.sample("a",2,180,2,false);const auto paused=p.clock.position;
        p.sample("a",2,180,2.5,false);assert(p.clock.position==paused && p.listen.eligible);
        // A real capture epoch reset starts without inherited coverage.
        p.clock={};p.listen={};p.rate=30;p.sample("a",90,180,3);assert(!p.listen.eligible);
    }
    {
        Listen l;l.Observe("short",0,1.5,true,0);l.Observe("short",0.5,1.5,true,0.5);
        l.Observe("short",1,1.5,true,1);l.Observe("short",0,1.5,true,1.5);
        l.Observe("short",0.5,1.5,true,2);assert(l.Observe("short",1,1.5,true,2.5)=="short");
        assert(l.Observe("short",0,1.5,true,3).empty() && l.transient);
        l.Observe("short",0.5,1.5,true,3.5);assert(l.Observe("short",1,1.5,true,4)=="short");
        assert(l.completed_generation==2 && near(l.completed_start,1.5));
    }
    for(double invalid:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        auto l=ending();assert(l.Observe("a",invalid,6,true,6).empty());assert(!l.eligible);
    }
    std::puts("PASS: repeat instances, delayed labels, raw resets, 1x-30x, seeks, pauses, invalidation and rate changes");
}
