#include "../native/client_playback.h"
#include "../native/ogg_history_core.h"
#include <cassert>
#include <thread>
#include <cstdio>
using namespace history;
int main() {
    const std::string base="v=1&title=Song&artist=Artist&album=Album&uri=spotify%3Atrack%3A0000000000000000000001&duration=120";
    const std::string id="1234567890abcdef1234567890abcdef";
    ClientPlayback p;
    assert(ParseClientPlayback(base+"&instance=1234567890ABCDEF1234567890ABCDEF&paused=0&buffering=1&local=1",p));
    assert(p.instance==id&&p.available&&p.paused==0&&p.buffering==1);
    assert(p.media.Matches("Song","Artist","Album",120));
    assert(!p.media.Matches("Other","Artist","Album",120));
    for(const auto& suffix:{"&instance=0","&instance=00000000000000000000000000000000",
        "&instance=1234567890abcdef1234567890abcdeg","&paused=2","&local=true",
        "&buffering=0&buffering=1","&instance=1234567890abcdef1234567890abcdef&instance=x"})
        assert(!ParseClientPlayback(base+suffix,p));
    assert(!ParseClientPlayback("v=1",p));
    assert(!ParseClientPlayback(base+std::string(4096,'x'),p));
    assert(!ParseClientPlayback("v=1&title=Ad&artist=X&album=X&uri=spotify:ad:0000000000000000000001&duration=30",p));
    assert(!ParseClientPlayback("v=1&title=X&artist=X&album=X&uri=spotify:track:short&duration=30",p));
    assert(ParseClientPlayback("v=1&title=Episode&uri=spotify:episode:0000000000000000000001&duration=100",p));
    assert(p.available&&p.instance.empty()&&p.paused==-1);
    assert(ParseClientPlayback(base+"&local=0",p)&&!p.available);
    assert(ParseClientPlayback(base+"&instance="+id,p));
    assert(!ClientPlaybackAgrees(p,"Song","Artist","Album",0,120,10,1));
    assert(ParseClientPlayback(base+"&instance="+id+"&position=0&paused=0&buffering=0",p));
    p.received=p.identity_since=10;
    assert(ClientPlaybackAgrees(p,"Song","Artist","Album",0,120,10,1));
    assert(!ClientPlaybackAgrees(p,"Song","Artist","Album",119,120,10,1));
    assert(!ClientPlaybackAgrees(p,"Song","Artist","Album",0,120,9.9,1));
    assert(!ClientPlaybackAgrees(p,"Old labels","Artist","Album",0,120,10,1));
    assert(ClientPlaybackAgrees(p,"Song","Artist","Album",15,120,10.5,30));
    p.paused=1;assert(!ClientPlaybackAgrees(p,"Song","Artist","Album",15,120,10.5,30));
    assert(!ParseClientPlayback(base+"&position=121",p));
    assert(!ParseClientPlayback(base+"&position=NaN",p));
    assert(!ParseClientPlayback(base+"&position=1.2.3",p));
    ClientPlaybackState state;
    assert(state.Token()==0&&!state.Read(10).seen);
    state.Publish(p,10);assert(state.Token()==0&&state.Read(10).available);
    state.Publish({},11);const auto blocked=state.Token();assert(blocked==1);
    state.Publish({},12);assert(state.Token()==blocked);
    state.Publish(p,13);assert(state.Token()==2&&state.Read(16).available);
    assert(!state.Read(16.001).available&&state.Token()==3);
    state.Publish(p,20);assert(state.Token()==4);
    assert(!state.Read(19).available&&state.Token()==5);
    // The reader must never observe a permitted snapshot with an odd fence.
    std::thread writer([&]{for(int i=0;i<10000;++i)state.Publish(i%2?p:ClientPlayback{},30);});
    for(int i=0;i<10000;++i){auto snap=state.Read(30);assert(!snap.available||!(snap.boundary&1));}
    writer.join();
    for(double rate:{1.,2.,10.,30.}) {
        Listen l;const auto first="spotify:track:one/"+id;
        assert(l.Observe(first,0,120,true,0,rate,true).empty());
        for(double t=.5;t<120/rate;t+=.5)
            assert(l.Observe(first,t*rate,120,true,t,rate,true).empty());
        const auto generation=l.generation;
        assert(l.Observe("spotify:track:one/new-instance",0,120,true,120/rate,rate,true)==first);
        assert(l.completed_generation==generation&&l.generation==generation+1&&l.eligible);
        assert(l.Observe("spotify:track:one/new-instance",rate*.5,120,true,120/rate+.5,rate,true).empty());
        // A rewind with the SAME explicit ID cannot manufacture a repeat.
        Listen seek;seek.Observe(first,0,120,true,0,rate,true);
        for(double t=.5;t<120/rate;t+=.5)seek.Observe(first,t*rate,120,true,t,rate,true);
        assert(seek.Observe(first,0,120,true,120/rate,rate,true).empty());
        assert(!seek.eligible&&seek.phase==ListenPhase::Tracking);
        Listen pause;pause.Observe(first,0,120,true,0,rate,true);
        pause.Observe(first,rate,120,false,1,rate,true);
        pause.Observe(first,rate,120,false,2,rate,true);
        pause.Observe(first,rate,120,true,3,rate,true);
        assert(pause.eligible&&pause.Observe(first,rate*2,120,true,4,rate,true).empty());
    }
    std::puts("PASS: client identity parsing, stale/remote/interstitial fences, concurrent snapshots, exact repeats/seeks and pause at 1/2/10/30x");
}
