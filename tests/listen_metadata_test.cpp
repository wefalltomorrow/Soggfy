#include "../native/listen_metadata.h"
#include "../native/ogg_history_core.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace history;

static Media sample() {
    Media m;m.title=L"Title";m.artist=L"Artist A; Artist B";m.album=L"Album";
    m.duration=6;m.position=5.5;m.raw_position=5.5;m.timeline_age=.1;m.playing=true;
    return m;
}
int main() {
    const std::string first="spotify:track:a/11111111111111111111111111111111";
    const std::string repeat="spotify:track:a/22222222222222222222222222222222";
    const std::string other="spotify:track:b/11111111111111111111111111111111";
    Listen listen;
    for(int i=0;i<12;++i)listen.Observe(first,i*.5,6,true,i*.5,1,true);
    auto previous=sample();
    const auto generation=listen.generation;
    auto final=previous;final.position=6;final.playing=false;
    final.cover={1,2,3};final.cover_extension=L".jpg";final.track=7;
    final.genre=L"Late genre";final.album_artist=L"Album artist";
    assert(listen.Observe(first,6,6,false,6,1,true)==first);
    assert(listen.completed_generation==generation && listen.generation==generation);
    auto completed=previous;
    assert(RefreshListenMetadata(completed,final,{first,generation,true},
        {listen.identity,listen.generation,true},true));
    assert(completed.cover==final.cover && completed.track==7 && completed.genre==L"Late genre");
    assert(completed.album_artist==L"Album artist");
    // Metadata refresh does not rewrite finished coverage or revive eligibility.
    assert(completed.position==5.5 && completed.raw_position==5.5 && completed.timeline_age==.1);
    assert(completed.duration==6 && !listen.eligible);
    assert(listen.completed_start==0 && listen.completed_time==6);

    // Further metadata can arrive while a completed candidate awaits audio.
    auto late=final;late.title=L"Corrected title";late.genre=L"Corrected genre";
    late.cover={4,5};late.cover_extension=L".png";late.duration=6.5;
    assert(listen.Observe(first,6,6,false,6.5,1,true).empty());
    assert(RefreshListenMetadata(completed,late,{first,generation,true},
        {listen.identity,listen.generation,true},true));
    assert(completed.title==late.title && completed.cover==late.cover && completed.duration==6);

    // Unknown optional fields must not erase already observed values.
    auto sparse=sample();sparse.position=6;sparse.playing=false;
    assert(RefreshListenMetadata(completed,sparse,{first,generation,true},{first,generation,true}));
    assert(completed.cover==late.cover && completed.cover_extension==L".png");
    assert(completed.track==7 && completed.album_artist==L"Album artist");
    assert(completed.genre==L"Corrected genre" && completed.position==6 && !completed.playing);
    auto partial=sparse;partial.cover={9};
    assert(RefreshListenMetadata(completed,partial,{first,generation,true},{first,generation,true}));
    assert(completed.cover==late.cover && completed.cover_extension==L".png");

    for(const auto& id:{repeat,other,first}) {
        auto target=completed;
        const auto next_generation=id==first?generation+1:generation;
        assert(!RefreshListenMetadata(target,final,{first,generation,true},{id,next_generation,true}));
        assert(target.cover==completed.cover && target.genre==completed.genre);
    }
    // A real exact repeat starts another generation even for identical labels.
    assert(listen.Observe(repeat,0,6,true,7,1,true).empty());
    assert(listen.generation!=generation);
    assert(!RefreshListenMetadata(completed,final,{first,generation,true},
        {listen.identity,listen.generation,true},true));
    for(bool before:{false,true})for(bool after:{false,true}) {
        if(before && after)continue;
        assert(!RefreshListenMetadata(completed,final,{first,generation,before},{first,generation,after}));
    }
    assert(!RefreshListenMetadata(completed,final,{"",generation,true},{"",generation,true}));
    assert(!RefreshListenMetadata(completed,final,{first,0,true},{first,0,true}));
    for(double duration:{0.,8.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        auto invalid=final;invalid.duration=duration;
        assert(!RefreshListenMetadata(completed,invalid,{first,generation,true},{first,generation,true}));
    }
    auto empty=final;empty.title.clear();
    assert(!RefreshListenMetadata(completed,empty,{first,generation,true},{first,generation,true}));
    // Rejected moves leave the incoming observation usable for a new instance.
    auto next=final;
    assert(!RefreshListenMetadata(completed,std::move(next),{first,generation,true},{repeat,generation+1,true}));
    assert(next.cover==final.cover && next.title==final.title);
    auto active=sample();auto moving=final;const auto* cover=moving.cover.data();
    assert(RefreshListenMetadata(active,std::move(moving),{first,generation,true},{first,generation,true}));
    assert(active.cover.data()==cover); // no extra artwork copy in the polling path
    std::puts("PASS: final/pending metadata refresh, optional fields, exact-instance fences, coverage and moved artwork");
}
