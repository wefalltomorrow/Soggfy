#include "../native/stream_dump_core.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

int main() {
    using namespace history;
    StreamDumpQueue<2> queue;
    StreamDumpRecord first{};
    first.stream_id=1;
    first.length=5;
    std::memcpy(first.bytes,"first",first.length);
    StreamDumpRecord second{};
    second.stream_id=2;
    StreamDumpRecord dropped{};
    dropped.stream_id=3;
    assert(queue.Push(first));
    assert(queue.Push(second));
    assert(!queue.Push(dropped));

    StreamDumpRecord output{};
    assert(queue.Pop(output)&&output.stream_id==1&&output.dropped_before==0&&
           output.length==5&&std::memcmp(output.bytes,"first",5)==0);
    StreamDumpRecord fourth{};
    fourth.stream_id=4;
    fourth.length=6;
    std::memcpy(fourth.bytes,"fourth",fourth.length);
    assert(queue.Push(fourth));
    assert(queue.Pop(output)&&output.stream_id==2);
    assert(queue.Pop(output)&&output.stream_id==4&&output.dropped_before==1&&
           output.length==6&&std::memcmp(output.bytes,"fourth",6)==0);
    assert(!queue.Pop(output));
    assert(queue.TakeDropped()==0);

    StreamDumpRecord oversized{};
    oversized.length=uint32_t(kStreamDumpMaxPayload+1);
    assert(!queue.Push(oversized));
    assert(queue.TakeDropped()==1);

    StreamDumpQueue<8> concurrent;
    constexpr uint64_t per_producer=10000;
    std::atomic<uint64_t> accepted{0};
    std::atomic<unsigned> producers_done{0};
    std::vector<uint64_t> seen;
    seen.reserve(size_t(per_producer)*2);
    uint64_t dropped_total=0;
    std::thread consumer([&]{
        StreamDumpRecord item;
        for(;;) {
            // Acquire the completion counter before the final queue check. A
            // failed Pop followed by observing both producers as done is not
            // enough: that Pop may have read the queue index before the
            // producer's final release publication became visible.
            const bool all_producers_done=
                producers_done.load(std::memory_order_acquire)==2;
            if(concurrent.Pop(item)) {
                assert(item.length==sizeof(uint64_t));
                uint64_t sequence=0;std::memcpy(&sequence,item.bytes,sizeof(sequence));
                assert(sequence<per_producer);
                assert(item.stream_id==1||item.stream_id==2);
                seen.push_back((item.stream_id<<32)|sequence);
                accepted.fetch_add(1,std::memory_order_relaxed);
                dropped_total+=item.dropped_before;
            } else if(all_producers_done) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
    });
    auto produce=[&](uint64_t stream_id) {
        for(uint64_t sequence=0;sequence<per_producer;++sequence) {
            StreamDumpRecord item;item.stream_id=stream_id;item.length=sizeof(sequence);
            std::memcpy(item.bytes,&sequence,sizeof(sequence));
            concurrent.Push(item);
        }
        producers_done.fetch_add(1,std::memory_order_release);
    };
    std::thread producer_a(produce,1),producer_b(produce,2);
    producer_a.join();producer_b.join();consumer.join();
    dropped_total+=concurrent.TakeDropped();
    assert(accepted.load(std::memory_order_relaxed)==seen.size());
    assert(accepted.load(std::memory_order_relaxed)+dropped_total==per_producer*2);
    std::sort(seen.begin(),seen.end());
    assert(std::adjacent_find(seen.begin(),seen.end())==seen.end());
    std::puts("PASS: bounded stream queue is MPSC-safe, nonblocking and accounts for loss");
}
