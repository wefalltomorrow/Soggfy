#include "ogg_history_core.h"
#include <cstring>
#include <cmath>
#include <algorithm>
namespace history {
static uint64_t le(const uint8_t* p, unsigned n) {
    uint64_t value=0; for(unsigned i=0;i<n;i++) value |= uint64_t(p[i])<<(8*i);
    return value;
}
static uint32_t crc_table[256];
static const bool initialized = [] {
    for(unsigned i=0;i<256;i++) {
        uint32_t r=i<<24;
        for(unsigned bit=0;bit<8;bit++) r=r&0x80000000 ? (r<<1)^0x04c11db7 : r<<1;
        crc_table[i]=r;
    } return true;
}();
bool ParsePage(const uint8_t* b, size_t n, Page& out) {
    (void)initialized;
    if(!b || n<27 || n>65307 || std::memcmp(b,"OggS",4) || b[4] || (b[5]&~7)) return false;
    size_t header=27+b[26]; if(header>n) return false;
    size_t body=0; for(size_t i=27;i<header;i++) body+=b[i];
    if(header+body!=n) return false;
    uint32_t crc=0;
    for(size_t i=0;i<n;i++) {
        uint8_t v = i>=22 && i<26 ? 0 : b[i];
        crc=(crc<<8)^crc_table[((crc>>24)^v)&255];
    }
    if(crc!=le(b+22,4)) return false;
    out={}; out.flags=b[5]; out.granule=int64_t(le(b+6,8));
    out.serial=uint32_t(le(b+14,4)); out.sequence=uint32_t(le(b+18,4));
    if((out.flags&2) && !(out.flags&1) && body>=30 && !std::memcmp(b+header,"\x01vorbis",7)
       && le(b+header+7,4)==0) {
        out.channels=b[header+11]; out.rate=uint32_t(le(b+header+12,4));
        int32_t nominal=int32_t(le(b+header+20,4)); out.bitrate=nominal>0 ? uint32_t(nominal) : 0;
        out.vorbis_start=out.channels>0 && out.rate>=8000 && out.rate<=384000;
    }
    return true;
}
Result Stream::Push(const uint8_t* b, size_t n) {
    Page p;
    if(!ParsePage(b,n,p)) { active=false; complete=false; return Result::Invalid; }

    // Spotify can revisit already-consumed Ogg pages internally while buffering
    // or reinitializing a decoder. A lower sequence number from the same logical
    // stream is a replay, not a missing-page condition. Do not append it again.
    // Genuine forward gaps still invalidate the capture.
    if(active && p.serial==serial && p.sequence<next)
        return Result::Replay;

    if(p.vorbis_start) {
        *this={}; active=true; serial=p.serial; next=p.sequence+1;
        rate=p.rate; channels=p.channels; bitrate=p.bitrate; pages=1; samples=p.granule;
        return Result::Begin;
    }
    if(!active) return Result::Ignore;
    if(p.serial!=serial || p.sequence>next || (p.flags&2) || p.granule < -1
       || (p.granule>=0 && samples>=0 && p.granule<samples)) {
        active=false; complete=false; return Result::Invalid;
    }
    ++next; ++pages; if(p.granule>=0) samples=p.granule;
    if(p.flags&4) {
        active=false; complete=samples>0 && p.granule>0;
        return complete ? Result::Complete : Result::Invalid;
    }
    return Result::Append;
}
const char* ListenRejectName(ListenReject reason) {
    switch(reason) {
        case ListenReject::None:return "none";
        case ListenReject::StartTooLate:return "start_too_late";
        case ListenReject::ClockDiscontinuity:return "clock_discontinuity";
        case ListenReject::PositionRewind:return "position_rewind";
        case ListenReject::PositionAhead:return "position_ahead";
        case ListenReject::DurationChanged:return "duration_changed";
        case ListenReject::InvalidObservation:return "invalid_observation";
        case ListenReject::IdentityUnresolved:return "identity_unresolved";
    }
    return "unknown";
}

void Listen::Invalidate(ListenReject reason) {
    eligible=false;pending_start=false;transient=false;phase=ListenPhase::Tracking;reject=reason;
}
namespace {
constexpr double identity_window=2.0,repeat_confirmation=1.0;
bool IdentityWindowOpen(const Listen& l,double time) {
    return time>=l.wrap_time && time-l.wrap_time<=identity_window;
}
bool Continues(double previous,double time_before,bool was_playing,double rate,
               double pos,double length,bool playing,double time) {
    const double elapsed=time-time_before;
    const double expected=previous+(was_playing?elapsed*rate:0);
    const double tolerance=std::max(0.5,rate*0.25);
    return elapsed>=0 && elapsed<=3 && pos<=length && pos>=previous-0.1 &&
        pos<=expected+tolerance && (!was_playing || !playing || pos>=expected-tolerance);
}
void BeginListen(Listen& l,const std::string& key,double pos,double length,
                 bool playing,double time,double rate,double start,bool observed_start=false) {
    l.identity=key;l.start_time=start;l.duration=length;l.identity_time=time;
    l.eligible=!key.empty() && (observed_start || pos<=std::max(1.5,rate*0.75));
    l.pending_start=!l.eligible && !key.empty();l.phase=ListenPhase::Tracking;
    l.reject=l.eligible||key.empty()?ListenReject::None:ListenReject::StartTooLate;
    l.last_time=time;l.last_position=pos;l.playing=playing;l.last_rate=rate;
    if(!key.empty())++l.generation;
}
std::string CompleteListen(Listen& l,double time) {
    l.completed_generation=l.generation;l.completed_start=l.start_time;
    l.completed_time=time;l.completed_duration=l.duration;
    l.eligible=false;l.pending_start=false;return l.identity;
}
bool NaturalEnd(const Listen& l,double time) {
    const double elapsed=time-l.last_time;
    return l.eligible && l.playing && l.duration>0 && elapsed>=0 && elapsed<=3 &&
        l.last_position+elapsed*l.last_rate>=l.duration-0.1;
}
}
std::string Listen::Observe(const std::string& key,double pos,double length,
                            bool is_playing,double time,double playback_rate) {
    transient=false;reject=ListenReject::None;
    if(!std::isfinite(playback_rate) || playback_rate<0.25 || playback_rate>100.0)playback_rate=1;
    if(!std::isfinite(pos) || !std::isfinite(length) || !std::isfinite(time) ||
       pos<0 || length<=0 || pos>length) {
        Invalidate(ListenReject::InvalidObservation);return {};
    }
    const double elapsed=time-last_time;
    // Match MediaPositionClock: the preceding interval belongs to its old rate.
    const double expected=last_position+(playing && elapsed>=0?elapsed*last_rate:0);
    const double start_window=std::max(1.5,playback_rate*0.75);
    const double tolerance=std::max(1.5,last_rate*0.25);
    last_expected=expected;last_elapsed=elapsed;last_tolerance=tolerance;
    auto sample=[&] {last_time=time;last_position=pos;playing=is_playing;last_rate=playback_rate;};
    auto finish_time=[&] {return std::min(time,last_time+std::max(0.0,duration-last_position)/last_rate);};
    std::string done;
    if(key!=identity) {
        if(!key.empty() && eligible && phase==ListenPhase::ProvisionalRepeat &&
           IdentityWindowOpen(*this,time) &&
           Continues(last_position,last_time,playing,last_rate,pos,length,is_playing,time)) {
            // The old completion was already emitted. Relabel this instance
            // without manufacturing another completion or resetting its start.
            identity=key;duration=length;identity_time=time;pending_start=false;
            phase=ListenPhase::Tracking;sample();return {};
        }
        const bool awaiting=phase==ListenPhase::AwaitingIdentity;
        const bool observed_start=awaiting && eligible && IdentityWindowOpen(*this,time) &&
            is_playing && std::fabs(length-wrap_length)<=1 &&
            Continues(wrap_last_position,wrap_last_time,true,wrap_rate,pos,length,is_playing,time);
        if(!key.empty() && (awaiting?observed_start && NaturalEnd(*this,wrap_time):NaturalEnd(*this,time)))
            done=CompleteListen(*this,awaiting?wrap_start:finish_time());
        const double start=observed_start?wrap_start:time-pos/playback_rate;
        BeginListen(*this,key,pos,length,is_playing,time,playback_rate,start,observed_start);
        return done;
    }
    if(key.empty()) {Invalidate();sample();return {};}
    // Require a visible backward reset, not merely a short song inside the
    // accelerated start window. A new wrap takes precedence over reconciliation.
    if(phase!=ListenPhase::AwaitingIdentity && NaturalEnd(*this,time) && is_playing &&
       pos<=start_window && pos<last_position-0.1) {
        phase=ListenPhase::AwaitingIdentity;wrap_time=time;wrap_start=time-pos/playback_rate;
        wrap_position=pos;wrap_length=length;wrap_last_time=time;wrap_last_position=pos;
        wrap_rate=playback_rate;transient=true;return {};
    }
    if(phase==ListenPhase::ProvisionalRepeat) {
        if(!IdentityWindowOpen(*this,time))phase=ListenPhase::Tracking;
        else if(!eligible || std::fabs(length-duration)>1 ||
                !Continues(last_position,last_time,playing,last_rate,pos,length,is_playing,time))
            Invalidate(ListenReject::IdentityUnresolved);
    }
    if(phase==ListenPhase::AwaitingIdentity) {
        const double waiting=time-wrap_time;
        if(!eligible || !IdentityWindowOpen(*this,time) || !is_playing ||
           std::fabs(length-wrap_length)>1 ||
           !Continues(wrap_last_position,wrap_last_time,true,wrap_rate,pos,length,is_playing,time)) {
            Invalidate(ListenReject::IdentityUnresolved);sample();return {};
        }
        wrap_last_time=time;wrap_last_position=pos;wrap_rate=playback_rate;
        if(waiting>=repeat_confirmation && std::fabs(length-duration)<=1) {
            done=CompleteListen(*this,wrap_start);
            BeginListen(*this,key,pos,length,is_playing,time,playback_rate,wrap_start,true);
            phase=ListenPhase::ProvisionalRepeat;return done;
        }
        if(waiting>=identity_window) {Invalidate(ListenReject::IdentityUnresolved);sample();return {};}
        transient=true;return {};
    }
    if(pending_start && time>=identity_time && time-identity_time<=identity_window && pos<=start_window) {
        start_time=time-pos/playback_rate;duration=length;eligible=true;pending_start=false;
        sample();return {};
    }
    if(eligible) {
        if(elapsed<0 || elapsed>3)Invalidate(ListenReject::ClockDiscontinuity);
        else if(pos<last_position-tolerance)Invalidate(ListenReject::PositionRewind);
        else if(pos>expected+tolerance)Invalidate(ListenReject::PositionAhead);
        else if(std::fabs(length-duration)>1)Invalidate(ListenReject::DurationChanged);
    }
    if(eligible && playing && !is_playing && pos>=duration-tolerance && expected>=duration-0.1)
        done=CompleteListen(*this,finish_time());
    sample();return done;
}
}
