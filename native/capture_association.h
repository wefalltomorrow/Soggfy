#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace history {
constexpr double capture_completion_wait=10;
struct CaptureRetention {
    double until=0,start=0,finish=0;
    std::uint64_t generation=0;
    bool ambiguous=false;
};

template<class Capture,class Heard>
bool CaptureTimingMatches(const Capture& c,const Heard& h) {
    // Preserve the existing start window. Unambiguous retention belongs to
    // one completed listen. Ambiguous retention only rejects; it never matches
    // publishable audio, even after another overlapping listen is removed.
    return c.born>=h.start-20 && c.born<=h.start+3 &&
        (!c.retention.until || c.retention.ambiguous ||
         (c.retention.start==h.start && c.retention.finish==h.finish && c.retention.generation==h.generation));
}
template<class Capture,class Heard>
bool CaptureMatches(const Capture& c,const Heard& h) {
    return !c.retention.ambiguous && CaptureTimingMatches(c,h) &&
        (!c.retention.until || c.finished<=c.retention.until) &&
        std::fabs(c.Duration()-h.media.duration)<=1.0;
}
template<class Capture,class HeardList>
bool RetainPendingCapture(Capture& c,const HeardList& heard,double now) {
    CaptureRetention candidate;
    std::size_t matches=0;
    for(const auto& h:heard) {
        const double deadline=h.finish+capture_completion_wait;
        if(CaptureTimingMatches(c,h) && now<=deadline &&
           (!c.retention.until || now<=c.retention.until)) {
            if(c.retention.until)return true;
            if(!matches)candidate={deadline,h.start,h.finish,h.generation};
            else candidate.until=std::max(candidate.until,deadline);
            ++matches;
        }
    }
    if(!matches)return false;
    // Keep a disputed candidate as a rejection marker until selection has
    // seen every affected listen. Erasing it here could make a competitor
    // appear unique. Never let later queue removal resolve this ambiguity.
    candidate.ambiguous=matches>1;
    c.retention=candidate;
    return true;
}
template<class Active,class Ready,class HeardList>
void CleanupCaptureTransition(Active& active,Ready& ready,HeardList& heard,
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
    // Remember the rejection on the listen before any worker restart/guard can
    // defer selection. A later decoder failure or pointer reuse may destroy
    // the capture marker; that must not make a competing capture publishable.
    for(auto& h:heard) {
        std::size_t conflicts=0;
        for(const auto& entry:active)
            if(entry.second->retention.ambiguous && CaptureTimingMatches(*entry.second,h))++conflicts;
        for(const auto& c:ready)
            if(c->retention.ambiguous && CaptureTimingMatches(*c,h))++conflicts;
        h.retention_conflicts=std::max(h.retention_conflicts,conflicts);
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
// Evaluate the whole completed-listen batch before consuming either list.
// A timing-only candidate shared by two listens cannot be assigned by iteration
// order. Discard it so erasing the first listen cannot make it look unique later.
// Retention already binds a candidate to one completed generation when known.
template<class Ready,class HeardList>
std::vector<std::size_t> RejectSharedCaptures(Ready& ready,const HeardList& heard) {
    std::vector<std::size_t> conflicts(heard.size(),0);
    if(heard.size()<2)return conflicts;
    ready.erase(std::remove_if(ready.begin(),ready.end(),[&](const auto& capture) {
        std::size_t matches=0;
        for(const auto& h:heard)if(CaptureMatches(*capture,h))++matches;
        if(matches<2)return false;
        for(std::size_t i=0;i<heard.size();++i)
            if(CaptureMatches(*capture,heard[i]))++conflicts[i];
        return true;
    }),ready.end());
    return conflicts;
}
struct CaptureSelection {
    CaptureAssociation state=CaptureAssociation::Wait;
    std::size_t index=0,matches=0,pending=0,retention_conflicts=0;
};
template<class Active,class Ready,class Heard>
CaptureSelection SelectCapture(const Active& active,const Ready& ready,const Heard& h,double now) {
    CaptureSelection result;
    for(std::size_t i=0;i<ready.size();++i) {
        if(ready[i]->retention.ambiguous && CaptureTimingMatches(*ready[i],h))
            ++result.retention_conflicts;
        if(CaptureMatches(*ready[i],h)) {result.index=i;++result.matches;}
    }
    for(const auto& entry:active) {
        const auto& c=*entry.second;
        if(c.retention.ambiguous && CaptureTimingMatches(c,h))++result.retention_conflicts;
        else if(c.retention.until && CaptureTimingMatches(c,h))++result.pending;
    }
    result.retention_conflicts=std::max(result.retention_conflicts,h.retention_conflicts);
    // Retaining a second, unfinished candidate must not let the first ready
    // candidate win a race. Wait for strict codec validation, then reject
    // multiple matches (or an unresolved competitor at the deadline).
    if(result.retention_conflicts || result.matches>1)result.state=CaptureAssociation::Ambiguous;
    else if(result.matches==1 && !result.pending)result.state=CaptureAssociation::Ready;
    else if(now-h.finish>capture_completion_wait)
        result.state=result.matches?CaptureAssociation::Ambiguous:CaptureAssociation::Missing;
    return result;
}
} // namespace history
