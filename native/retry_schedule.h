#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace history {

inline constexpr unsigned transient_retry_limit=3;
inline constexpr std::uint64_t transient_retry_delay_ms(unsigned retry_index) {
    constexpr std::uint64_t delays[]={2000,10000,30000};
    return retry_index<transient_retry_limit?delays[retry_index]:0;
}

// Bounded delayed work queue. A popped entry remains charged to the byte
// budget until it is either released or requeued, so in-flight retry metadata
// cannot temporarily escape accounting.
template<class T,std::size_t MaxEntries,std::size_t MaxBytes>
class RetrySchedule {
    struct Entry {T value;std::size_t bytes;std::uint64_t due;};
    std::vector<Entry> entries_;
    std::size_t reserved_bytes_=0,in_flight_bytes_=0;
    bool in_flight_=false;
public:
    bool Push(T&& value,std::size_t bytes,std::uint64_t due) {
        if(entries_.size()+std::size_t(in_flight_)>=MaxEntries)return false;
        if(bytes>MaxBytes-reserved_bytes_)return false;
        entries_.push_back({std::move(value),bytes,due});
        reserved_bytes_+=bytes;
        return true;
    }
    bool TakeDue(std::uint64_t now,T& value,std::size_t& bytes) {
        if(in_flight_)return false;
        std::size_t selected=entries_.size();
        for(std::size_t i=0;i<entries_.size();++i)
            if(entries_[i].due<=now&&(selected==entries_.size()||entries_[i].due<entries_[selected].due))selected=i;
        if(selected==entries_.size())return false;
        value=std::move(entries_[selected].value);
        bytes=entries_[selected].bytes;
        in_flight_=true;in_flight_bytes_=bytes;
        entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(selected));
        return true;
    }
    bool Requeue(T&& value,std::size_t bytes,std::uint64_t due) {
        if(!in_flight_||bytes!=in_flight_bytes_||entries_.size()>=MaxEntries)return false;
        entries_.push_back({std::move(value),bytes,due});
        in_flight_=false;in_flight_bytes_=0;
        return true;
    }
    bool ReleaseInFlight(std::size_t bytes) {
        if(!in_flight_||bytes!=in_flight_bytes_||bytes>reserved_bytes_)return false;
        reserved_bytes_-=bytes;in_flight_=false;in_flight_bytes_=0;
        return true;
    }
    bool Discard(std::size_t index) {
        if(index>=entries_.size())return false;
        reserved_bytes_-=entries_[index].bytes;
        entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(index));
        return true;
    }
    std::uint64_t NextDue() const {
        std::uint64_t due=std::numeric_limits<std::uint64_t>::max();
        for(const auto& entry:entries_)if(entry.due<due)due=entry.due;
        return due;
    }
    const T& At(std::size_t index) const {return entries_.at(index).value;}
    std::size_t Size() const {return entries_.size();}
    std::size_t ReservedBytes() const {return reserved_bytes_;}
};

} // namespace history
