#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <string_view>
namespace history {
struct LogRecord {char event[32]{};char detail[1536]{};};
// Each lane owns its lock and fixed storage. Producers never wait or allocate.
template<size_t Capacity=128> class LogQueue {
    static_assert(Capacity>0);
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    std::array<LogRecord,Capacity> records_{};
    size_t head_=0,tail_=0,count_=0;
    std::mutex lock_;
    std::atomic<unsigned> dropped_{0};
    template<size_t N> static void Copy(char (&to)[N],std::string_view from) {
        size_t n=from.size()<N-1?from.size():N-1;
        if(n<from.size())while(n && (static_cast<unsigned char>(from[n])&0xc0)==0x80)--n;
        for(size_t i=0;i<n;++i)to[i]=static_cast<unsigned char>(from[i])<32?' ':from[i];
        to[n]=0;
    }
public:
    bool Push(std::string_view event,std::string_view detail) {
        std::unique_lock<std::mutex> guard(lock_,std::try_to_lock);
        if(!guard.owns_lock()||count_==Capacity){dropped_.fetch_add(1,std::memory_order_relaxed);return false;}
        auto& record=records_[tail_];Copy(record.event,event);Copy(record.detail,detail);
        tail_=(tail_+1)%Capacity;++count_;return true;
    }
    bool Pop(LogRecord& record) {
        std::lock_guard<std::mutex> guard(lock_);
        if(!count_)return false;
        record=records_[head_];head_=(head_+1)%Capacity;--count_;return true;
    }
    unsigned TakeDropped(){return dropped_.exchange(0,std::memory_order_relaxed);}
};
}
