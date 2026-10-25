// Real Windows thread/event boundary with a gated provider; no live Spotify.
#include "../native/media_session.h"
#include "../native/bounded_queue.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace history;
namespace {
double Now(){return double(GetTickCount64())/1000.0;}
template<class Predicate> void Wait(Predicate ready) {
    const auto deadline=GetTickCount64()+10000;
    while(!ready()){assert(GetTickCount64()<deadline);Sleep(1);}
}
enum class Mode { Good,Failure,Exception,Future,Invalid,ArtworkTooLarge };
struct Source {
    HANDLE entered=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE release=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    SRWLOCK lock=SRWLOCK_INIT;
    MediaReadOptions requested;
    std::atomic<unsigned> calls{0};
    std::atomic<Mode> mode{Mode::Good};
    Source(){assert(entered&&release);}
    ~Source(){CloseHandle(entered);CloseHandle(release);}
    void Entered(){assert(WaitForSingleObject(entered,10000)==WAIT_OBJECT_0);}
    void Release(){assert(SetEvent(release));}
    MediaReadOptions Options(){
        AcquireSRWLockShared(&lock);auto value=requested;ReleaseSRWLockShared(&lock);return value;
    }
};
bool Fetch(Media& media,double& observed,const MediaReadOptions& options,void* context) {
    auto& source=*static_cast<Source*>(context);
    const auto call=++source.calls;
    AcquireSRWLockExclusive(&source.lock);source.requested=options;ReleaseSRWLockExclusive(&source.lock);
    // Match production: timestamp the playback sample BEFORE slow artwork.
    observed=Now();SetEvent(source.entered);
    assert(WaitForSingleObject(source.release,10000)==WAIT_OBJECT_0);
    switch(source.mode.load()) {
        case Mode::Failure:return false;
        case Mode::Exception:throw std::runtime_error("provider failed");
        default:break;
    }
    media.title=L"Track "+std::to_wstring(call);media.artist=L"Artist";
    media.duration=180;media.position=1;media.playing=true;
    if(source.mode==Mode::Future)observed=Now()+10;
    if(source.mode==Mode::Invalid)media.position=std::numeric_limits<double>::quiet_NaN();
    if(source.mode==Mode::ArtworkTooLarge)media.cover.resize(4*1024*1024+1);
    return true;
}
struct Consumer {
    Media out;double observed=0;uint64_t sequence=0;
    MediaRead Poll(MediaSnapshotReader& reader,const MediaReadOptions& options){
        return reader.TryRead(out,observed,sequence,options);
    }
    void Result(MediaSnapshotReader& reader,const MediaReadOptions& options,MediaRead expected){
        MediaRead result=MediaRead::Pending;
        Wait([&]{result=Poll(reader,options);return result!=MediaRead::Pending;});
        assert(result==expected);
    }
};
void BlockedAndInvalid() {
    Source source;MediaSnapshotReader reader(Fetch,&source);Consumer consumer;
    MediaReadOptions options{1,1,30,true};consumer.out.title=L"untouched";
    assert(consumer.Poll(reader,options)==MediaRead::Pending&&consumer.sequence==0);
    assert(reader.Start(options)&&reader.Start(options));
    source.Entered();assert(source.Options()==options);source.Release();
    consumer.Result(reader,options,MediaRead::Ready);
    assert(consumer.out.title==L"Track 1"&&consumer.sequence==1);
    const auto first_time=consumer.observed;
    assert(consumer.Poll(reader,options)==MediaRead::Pending&&consumer.observed==first_time);
    source.Entered();const auto calls=source.calls.load();
    BoundedQueue<unsigned,4> queue;unsigned drained=0;
    const auto start=GetTickCount64();
    while(GetTickCount64()-start<3100) {
        unsigned item=drained;assert(queue.Push(std::move(item)));
        assert(consumer.Poll(reader,options)==MediaRead::Pending);
        unsigned value=0;assert(queue.Pop(value)&&value==drained);++drained;Sleep(1);
    }
    assert(drained>100&&source.calls==calls&&consumer.sequence==1);
    const auto original=consumer.out.title;source.Release();
    consumer.Result(reader,options,MediaRead::Failed);
    assert(consumer.out.title==original&&consumer.sequence==2);
    assert(Now()-consumer.observed>=3); // Slow artwork must not refresh sample time.
    for(auto mode:{Mode::Failure,Mode::Exception,Mode::Future,Mode::Invalid,Mode::ArtworkTooLarge}) {
        source.Entered();source.mode=mode;const auto sequence=consumer.sequence;source.Release();
        consumer.Result(reader,options,MediaRead::Failed);
        assert(consumer.out.title==original&&consumer.sequence==sequence+1);
    }
    source.Entered();source.mode=Mode::Good;source.Release();
    consumer.Result(reader,options,MediaRead::Ready);
    source.Entered();reader.Stop();source.Release();Wait([&]{return reader.IsStopped();});
    std::puts("PASS: blocked Windows provider permits queue draining; stale/invalid/failed samples fail closed");
}
void ConfigurationRevisions() {
    Source source;MediaSnapshotReader reader(Fetch,&source);Consumer consumer;
    MediaReadOptions options{1,1,30,true};assert(reader.Start(options));
    source.Entered();source.Release();consumer.Result(reader,options,MediaRead::Ready);
    // Independently fence generation, selected/effective speed, epoch and artwork.
    for(int change=0;change<5;++change) {
        source.Entered();const auto prior=options;const auto sequence=consumer.sequence;
        if(change==0)++options.generation;
        if(change==1)options.playback_rate=1;
        if(change==2)++options.capture_epoch;
        if(change==3)options.artwork=false;
        if(change==4)++options.generation;
        assert(consumer.Poll(reader,options)==MediaRead::Pending);
        if(change==4) { // A -> B -> A must still reject the in-flight A read.
            options=prior;assert(consumer.Poll(reader,options)==MediaRead::Pending);
        }
        source.Release();source.Entered();
        assert(source.Options()==options);
        assert(consumer.Poll(reader,options)==MediaRead::Pending&&consumer.sequence==sequence);
        source.Release();consumer.Result(reader,options,MediaRead::Ready);
        assert(consumer.sequence==sequence+1); // Obsolete results are never published.
    }
    source.Entered();reader.Stop();source.Release();Wait([&]{return reader.IsStopped();});
    std::puts("PASS: settings/rate/epoch/artwork changes and A-B-A revisions reject outstanding observations");
}
void SequenceAndStop() {
    Source source;MediaSnapshotReader reader(Fetch,&source);Consumer consumer;
    MediaReadOptions options{1,1,30,true};assert(reader.Start(options));
    source.Entered();source.Release();consumer.Result(reader,options,MediaRead::Ready);
    const auto original=consumer.out.title;
    source.Entered();source.Release();source.Entered();source.Release();source.Entered();
    assert(consumer.Poll(reader,options)==MediaRead::Failed);
    assert(consumer.sequence==3&&consumer.out.title==original);
    assert(consumer.Poll(reader,options)==MediaRead::Pending);
    const auto stopped=GetTickCount64();reader.Stop();
    assert(GetTickCount64()-stopped<1000&&!reader.IsStopped()&&!reader.Start(options));
    assert(consumer.Poll(reader,options)==MediaRead::Pending);
    source.Release();Wait([&]{return reader.IsStopped();});
    assert(reader.Start(options));source.Entered();source.Release();
    consumer.Result(reader,options,MediaRead::Ready);assert(consumer.sequence==4);
    source.Entered();reader.Stop();source.Release();Wait([&]{return reader.IsStopped();});
    std::puts("PASS: skipped observations fail closed; stop never waits on provider; restart is single-worker and sequenced");
}
}
int main(){BlockedAndInvalid();ConfigurationRevisions();SequenceAndStop();}
