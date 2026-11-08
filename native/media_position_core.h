#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <cstdint>

namespace history {
struct RawTimeline { double position=0; std::int64_t updated=0; };

// Keep the accelerated position on a monotonic clock. Spotify's SMTC timeline
// timestamp is periodically refreshed at the *unaccelerated* playback rate;
// extrapolating every fresh timestamp by 50x otherwise rewinds the position.
// The public timeline remains the initial anchor for every new media identity.
struct MediaPositionClock {
    std::string identity;
    double position=0.0, duration=0.0, last_time=0.0, previous_rate=1.0;
    bool previous_playing=false, accelerated=false, initialized=false;
    RawTimeline raw;bool raw_valid=false,awaiting_label=false;
    double reset_time=0;

    double Observe(const std::string& key, double reported_position, double length,
                   bool playing, double rate, double now,const RawTimeline* timeline=nullptr) {
        const double bounded_length=std::isfinite(length)&&length>0.0?length:0.0;
        double resolved=std::isfinite(reported_position)
            ?std::clamp(reported_position,0.0,bounded_length):0.0;
        bool same=initialized && key==identity &&
            std::fabs(length-duration)<=1.0;
        if(!std::isfinite(rate)||rate<1.0||rate>50.0) rate=1.0;
        const double elapsed=now-last_time;
        const bool interval=initialized && std::isfinite(elapsed) && elapsed>=0 && elapsed<=3;
        const bool valid=timeline && std::isfinite(timeline->position) &&
            timeline->position>=0 && timeline->position<=bounded_length && timeline->updated>0;
        // Extrapolated SMTC positions can fall on ordinary timestamp refreshes.
        // Only a newer raw timeline moving backwards can break the monotonic
        // accelerated clock. A seek is exposed to Listen, never certified here.
        const bool reset=interval && key==identity && valid && raw_valid &&
            timeline->updated>raw.updated && timeline->position<raw.position-1.5;
        if(reset) {
            awaiting_label=previous_playing && playing &&
                position+elapsed*previous_rate>=duration-0.1 &&
                resolved<=std::max(1.5,rate*0.75);
            reset_time=now;same=false;
        } else if(awaiting_label) {
            if(now<reset_time || now-reset_time>2)awaiting_label=false;
            else if(key!=identity) {
                // A delayed label can describe the timeline already observed
                // after the reset. Carry its clock only with continuous raw
                // evidence and the original deadline; never re-open the window.
                same=interval && valid && raw_valid && timeline->updated>=raw.updated &&
                    timeline->position>=raw.position-0.1 &&
                    timeline->position<=raw.position+elapsed*previous_rate+std::max(0.5,previous_rate*0.25);
                awaiting_label=false;
            }
        }
        if(same && (accelerated || rate>1.0)) {
            if(std::isfinite(elapsed) && elapsed>=0.0 && elapsed<=3.0) {
                const double advance=(previous_playing && playing)
                    ?elapsed*previous_rate:0.0;
                resolved=std::max(resolved,std::min(bounded_length,position+advance));
            } else {
                resolved=std::max(resolved,std::min(bounded_length,position));
            }
        }
        identity=key;
        position=resolved;
        duration=length;
        last_time=now;
        previous_rate=rate;
        previous_playing=playing;
        accelerated=(same&&accelerated)||rate>1.0;
        initialized=true;
        if(valid && (!raw_valid || timeline->updated>=raw.updated)) {raw=*timeline;raw_valid=true;}
        else if(!valid)raw_valid=false;
        return resolved;
    }
};

}
