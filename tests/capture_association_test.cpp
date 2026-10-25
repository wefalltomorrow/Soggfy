#include "../native/capture_association.h"
#include "../native/ogg_history_core.h"
#include "../native/flac_history_core.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
using namespace history;
struct TestCapture {
    double born=100,finished=0;
    CaptureRetention retention;
    Stream stream;
    FlacInfo flac{44100,2,16,4096,4096,8192};
    FlacCoverage coverage;
    bool lossless=false;
    double Duration() const {return lossless?double(coverage.samples)/flac.rate:stream.Duration();}
};
struct TestHeard {struct {double duration;} media;double start,finish;};
using Active=std::map<int,std::unique_ptr<TestCapture>>;
using Ready=std::vector<std::unique_ptr<TestCapture>>;
using HeardList=std::vector<TestHeard>;
static void put(std::vector<uint8_t>& v,unsigned at,uint64_t n,unsigned count) {
    for(unsigned i=0;i<count;++i)v[at+i]=uint8_t(n>>(i*8));
}
static std::vector<uint8_t> page(unsigned seq,unsigned flags,uint64_t samples,bool bos=false) {
    std::vector<uint8_t> v(58);std::memcpy(v.data(),"OggS",4);
    v[5]=uint8_t(flags);put(v,6,samples,8);put(v,14,1234,4);put(v,18,seq,4);v[26]=1;v[27]=30;
    if(bos) {std::memcpy(v.data()+28,"\x01vorbis",7);v[39]=2;put(v,40,44100,4);}
    uint32_t crc=0;
    for(auto b:v) {crc^=uint32_t(b)<<24;for(int i=0;i<8;++i)crc=crc&0x80000000?(crc<<1)^0x04c11db7:crc<<1;}
    put(v,22,crc,4);return v;
}
static void ogg(Active& active,Ready& ready,int key,const std::vector<uint8_t>& bytes,double time) {
    auto it=active.find(key);if(it==active.end())return;
    const auto result=it->second->stream.Push(bytes.data(),bytes.size());
    if(result==Result::Complete) {
        it->second->finished=time;ready.push_back(std::move(it->second));active.erase(it);
    } else if(result==Result::Invalid || result==Result::Ignore)active.erase(it);
}
static void begin(Active& active,Ready& ready,int key=1,double born=100) {
    active[key]=std::make_unique<TestCapture>();active[key]->born=born;
    ogg(active,ready,key,page(1,2,0,true),born);
    ogg(active,ready,key,page(2,0,44100),born+0.1);
}
static void finish(Active& active,Ready& ready,int key=1,double time=111) {
    ogg(active,ready,key,page(3,4,441000),time);
}
int main() {
    const TestHeard h{{10},100,110};
    {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        // RC64's transition predicate erases this still-incomplete capture.
        const auto& c=*active.at(1);
        assert(c.born>=h.start-20 && c.born<=h.start+3 && c.born<110-3);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        assert(active.size()==1 && active.at(1)->retention.until==120);
        assert(SelectCapture(active,ready,h,110).state==CaptureAssociation::Wait);
        finish(active,ready);
        assert(ready.size()==1 && ready[0]->stream.complete);
        assert(SelectCapture(active,ready,h,111).state==CaptureAssociation::Ready);
        heard.clear();PruneRetainedCaptures(active,ready,heard,111);assert(ready.empty());
    }
    {
        Active active;Ready ready;const TestHeard flac_heard{{8192.0/44100},100,110};
        HeardList heard{flac_heard};active[1]=std::make_unique<TestCapture>();
        auto& c=*active[1];c.lossless=true;
        assert(c.coverage.Frame(c.flac,4096,44100,2,16,0,0));
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        assert(active.size()==1 && !c.coverage.Complete(c.flac));
        assert(SelectCapture(active,ready,flac_heard,110).state==CaptureAssociation::Wait);
        assert(c.coverage.Frame(c.flac,4096,44100,2,16,0,1) && c.coverage.Complete(c.flac));
        c.finished=112;ready.push_back(std::move(active[1]));active.erase(1);
        assert(SelectCapture(active,ready,flac_heard,112).state==CaptureAssociation::Ready);
    }
    {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);begin(active,ready,2);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);finish(active,ready);
        auto selected=SelectCapture(active,ready,h,111);
        assert(selected.matches==1 && selected.pending==1 && selected.state==CaptureAssociation::Wait);
        assert(SelectCapture(active,ready,h,120).state==CaptureAssociation::Wait);
        assert(SelectCapture(active,ready,h,120.001).state==CaptureAssociation::Ambiguous);
        finish(active,ready,2,112);
        assert(SelectCapture(active,ready,h,112).state==CaptureAssociation::Ambiguous);
        heard.clear();PruneRetainedCaptures(active,ready,heard,112);assert(ready.empty());
    }
    for(bool corrupt:{false,true}) {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        auto end=page(corrupt?3:4,4,441000);if(corrupt)end.back()^=1;
        ogg(active,ready,1,end,111);assert(active.empty() && ready.empty());
        assert(SelectCapture(active,ready,h,121).state==CaptureAssociation::Missing);
    }
    {
        Active active;Ready ready;HeardList heard;begin(active,ready);begin(active,ready,2);finish(active,ready,2);
        begin(active,ready,3,109); // New decoder near fast Next survives existing overlap rule.
        CleanupCaptureTransition(active,ready,heard,100,110,false,110);
        assert(active.size()==1 && active.count(3) && ready.empty());
        // Completed but ignored listens never enter heard.
        begin(active,ready);CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        assert(!active.count(1));
    }
    {
        Active active;Ready ready;HeardList heard{h,{{10},106,112}};
        begin(active,ready);begin(active,ready,2,106);
        CleanupCaptureTransition(active,ready,heard,100,106,true,110);
        CleanupCaptureTransition(active,ready,heard,106,112,true,112);
        assert(active.at(1)->retention.until==120 && active.at(2)->retention.until==122);
        finish(active,ready,1,113);finish(active,ready,2,113);
        const auto first=SelectCapture(active,ready,heard[0],113);
        const auto second=SelectCapture(active,ready,heard[1],113);
        assert(first.state==CaptureAssociation::Ready && second.state==CaptureAssociation::Ready);
        assert(first.index!=second.index);
    }
    for(double finished:{120.0,120.001}) {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);finish(active,ready,1,finished);
        // A queued callback's timestamp, not the worker's delivery time, sets the cutoff.
        assert(SelectCapture(active,ready,h,121).state==
            (finished==120?CaptureAssociation::Ready:CaptureAssociation::Missing));
        PruneRetainedCaptures(active,ready,heard,121);assert(ready.empty());
    }
    {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        const TestHeard second{{10},106,112};heard.push_back(second);
        CleanupCaptureTransition(active,ready,heard,106,112,false,112);
        assert(active.size()==1 && active.at(1)->retention.until==120);
        assert(!CaptureTimingMatches(*active.at(1),second));
        finish(active,ready);
        // A later skipped track's cleanup must not erase an earlier completed capture.
        CleanupCaptureTransition(active,ready,heard,106,113,false,113);assert(ready.size()==1);
        assert(SelectCapture(active,ready,second,113).state==CaptureAssociation::Wait);
        heard.erase(heard.begin());PruneRetainedCaptures(active,ready,heard,113);assert(ready.empty());
    }
    {
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        PruneRetainedCaptures(active,ready,heard,120);assert(active.size()==1);
        PruneRetainedCaptures(active,ready,heard,120.001);assert(active.empty());
        begin(active,ready);CleanupCaptureTransition(active,ready,heard,100,110,true,121);assert(active.empty());
    }
    {
        TestCapture c;
        for(double born:{80.0,103.0}) {c.born=born;assert(CaptureTimingMatches(c,h));}
        for(double born:{79.999,103.001,15.0}) {c.born=born;assert(!CaptureTimingMatches(c,h));}
        Active active;Ready ready;HeardList heard{h};begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,100,110,true,110);
        ogg(active,ready,1,page(3,4,88200),111);
        assert(ready.size()==1 && SelectCapture(active,ready,h,121).state==CaptureAssociation::Missing);
    }
    {
        // Production Listen verifies a natural 30x transition before retention.
        Listen listen;listen.Observe("fast",0,180,true,100,30);
        for(int i=1;i<12;++i)assert(listen.Observe("fast",i*15,180,true,100+i*0.5,30).empty());
        const double start=listen.start_time;
        assert(listen.Observe("next",0,180,true,106,30)=="fast");
        HeardList heard{{{180},start,106}};Active active;Ready ready;begin(active,ready);
        CleanupCaptureTransition(active,ready,heard,start,listen.start_time,true,106);
        assert(active.size()==1 && active.at(1)->retention.until==116);
        ogg(active,ready,1,page(3,4,44100*180),107);
        assert(SelectCapture(active,ready,heard[0],107).state==CaptureAssociation::Ready);
    }
    std::puts("PASS: late Ogg/FLAC completion, bounded owned retention, skips, 30x, ambiguity, integrity and expiry");
}
