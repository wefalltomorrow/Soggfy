#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace history {

constexpr size_t kStreamDumpMaxPayload = 65307;

struct StreamDumpRecord {
    uint64_t stream_id=0;
    uint64_t time_ms=0;
    uint64_t input_length=0;
    uint64_t dropped_before=0;
    uint32_t epoch=0;
    uint32_t status=0;
    uint32_t length=0;
    char codec[8]{};
    char event[24]{};
    char state[16]{};
    unsigned char bytes[kStreamDumpMaxPayload];
};

template<size_t Capacity>
class StreamDumpQueue {
    static_assert(Capacity>0,"stream dump queue must have capacity");
    StreamDumpRecord slots_[Capacity];
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
    std::atomic<uint64_t> dropped_{0};
    std::atomic_flag producer_=ATOMIC_FLAG_INIT;

    static void CopyRecord(StreamDumpRecord& to,const StreamDumpRecord& from) {
        to.stream_id=from.stream_id;
        to.time_ms=from.time_ms;
        to.input_length=from.input_length;
        to.dropped_before=from.dropped_before;
        to.epoch=from.epoch;
        to.status=from.status;
        to.length=from.length;
        std::memcpy(to.codec,from.codec,sizeof(to.codec));
        std::memcpy(to.event,from.event,sizeof(to.event));
        std::memcpy(to.state,from.state,sizeof(to.state));
        if(from.length)std::memcpy(to.bytes,from.bytes,from.length);
    }
public:
    bool Push(const StreamDumpRecord& record) {
        // Decoder hooks can run on different Spotify threads. Serialize only
        // successful producers; callbacks never wait behind another producer.
        if(producer_.test_and_set(std::memory_order_acquire)) {
            dropped_.fetch_add(1,std::memory_order_relaxed);
            return false;
        }
        if(record.length>kStreamDumpMaxPayload) {
            dropped_.fetch_add(1,std::memory_order_relaxed);
            producer_.clear(std::memory_order_release);
            return false;
        }
        const auto write=write_.load(std::memory_order_relaxed);
        const auto read=read_.load(std::memory_order_acquire);
        if(write-read>=Capacity) {
            dropped_.fetch_add(1,std::memory_order_relaxed);
            producer_.clear(std::memory_order_release);
            return false;
        }
        auto& slot=slots_[write%Capacity];
        CopyRecord(slot,record);
        slot.dropped_before+=dropped_.exchange(0,std::memory_order_acq_rel);
        write_.store(write+1,std::memory_order_release);
        producer_.clear(std::memory_order_release);
        return true;
    }

    bool Pop(StreamDumpRecord& record) {
        const auto read=read_.load(std::memory_order_relaxed);
        if(read==write_.load(std::memory_order_acquire))return false;
        CopyRecord(record,slots_[read%Capacity]);
        read_.store(read+1,std::memory_order_release);
        return true;
    }

    uint64_t TakeDropped() { return dropped_.exchange(0,std::memory_order_acq_rel); }
    void RecordDrop() { dropped_.fetch_add(1,std::memory_order_relaxed); }
};

}
