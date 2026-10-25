#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace history {
constexpr double capture_completion_wait=10;
struct CaptureRetention {
    double until=0,start=0,finish=0;
    std::uint64_t generation=0;
};

template<class Capture,class Heard>
bool CaptureTimingMatches(const Capture& c,const Heard& h) {
    // Preserve the existing start window. Retention belongs to one completed
    // listen, so an overlapping later listen cannot inherit its audio.
    return c.born>=h.start-20 && c.born<=h.start+3 &&
        (!c.retention.until ||
         (c.retention.start==h.start && c.retention.finish==h.finish && c.retention.generation==h.generation));
}
template<class Capture,class Heard>
bool CaptureMatches(const Capture& c,const Heard& h) {
    return CaptureTimingMatches(c,h) &&
        (!c.retention.until || c.finished<=c.retention.until) &&
        std::fabs(c.Duration()-h.media.duration)<=1.0;
}
template<class Capture,class HeardList>
bool RetainPendingCapture(Capture& c,const HeardList& heard,double now) {
    for(const auto& h:heard) {
        const double deadline=h.finish+capture_completion_wait;
        if(CaptureTimingMatches(c,h) && now<=deadline &&
           (!c.retention.until || now<=c.retention.until)) {
            if(!c.retention.until)c.retention={deadline,h.start,h.finish,h.generation};
            return true;
        }
    }
    return false;
}
template<class Active,class Ready,class HeardList>
void CleanupCaptureTransition(Active& active,Ready& ready,const HeardList& heard,
                              double previous_start,double start,bool completed,double now) {
    auto prior=[=](const auto& c) {
        return c->born>=previous_start-20 && c->born<=previous_start+3 && c->born<start-3;
    };
    // Keep the existing fast-Next overlap exception. Only a completed,
    // nonignored listen in heard can protect a candidate from skip cleanup.
    if(!completed)ready.erase(std::remove_if(ready.begin(),ready.end(),[&](auto& c) {
        return prior(c) && !RetainPendingCapture(*c,heard,now);
    }),ready.end());
    for(auto it=active.begin();it!=active.end();) {
        if(prior(it->second) && !RetainPendingCapture(*it->second,heard,now))
            it=active.erase(it);
        else ++it;
    }
}
template<class Active,class Ready,class HeardList>
void PruneRetainedCaptures(Active& active,Ready& ready,const HeardList& heard,double now) {
    auto expired=[&](const auto& c) {
        if(!c->retention.until)return false;
        if(now>c->retention.until)return true;
        return std::none_of(heard.begin(),heard.end(),[&](const auto& h) {
            return CaptureTimingMatches(*c,h);
        });
    };
    for(auto it=active.begin();it!=active.end();) {
        if(expired(it->second))it=active.erase(it);else ++it;
    }
    ready.erase(std::remove_if(ready.begin(),ready.end(),expired),ready.end());
}
enum class CaptureAssociation { Wait,Ready,Ambiguous,Missing };
struct CaptureSelection {
    CaptureAssociation state=CaptureAssociation::Wait;
    std::size_t index=0,matches=0,pending=0;
};
template<class Active,class Ready,class Heard>
CaptureSelection SelectCapture(const Active& active,const Ready& ready,const Heard& h,double now) {
    CaptureSelection result;
    for(std::size_t i=0;i<ready.size();++i) {
        if(CaptureMatches(*ready[i],h)) {result.index=i;++result.matches;}
    }
    for(const auto& entry:active) {
        const auto& c=*entry.second;
        if(c.retention.until && CaptureTimingMatches(c,h))++result.pending;
    }
    // Retaining a second, unfinished candidate must not let the first ready
    // candidate win a race. Wait for strict codec validation, then reject
    // multiple matches (or an unresolved competitor at the deadline).
    if(result.matches>1)result.state=CaptureAssociation::Ambiguous;
    else if(result.matches==1 && !result.pending)result.state=CaptureAssociation::Ready;
    else if(now-h.finish>capture_completion_wait)
        result.state=result.matches?CaptureAssociation::Ambiguous:CaptureAssociation::Missing;
    return result;
}
} // namespace history
