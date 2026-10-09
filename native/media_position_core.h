#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace history {

// Keep the accelerated position on a monotonic clock. Spotify's SMTC timeline
// timestamp is periodically refreshed at the *unaccelerated* playback rate;
// extrapolating every fresh timestamp by 50x otherwise rewinds the position.
// The public timeline remains the initial anchor for every new media identity.
struct MediaPositionClock {
    std::string identity;
    double position=0.0, duration=0.0, last_time=0.0, previous_rate=1.0;
    bool previous_playing=false, accelerated=false, initialized=false;

    double Observe(const std::string& key, double reported_position, double length,
                   bool playing, double rate, double now) {
        const double bounded_length=std::isfinite(length)&&length>0.0?length:0.0;
        double resolved=std::isfinite(reported_position)
            ?std::clamp(reported_position,0.0,bounded_length):0.0;
        const bool same=initialized && key==identity &&
            std::fabs(length-duration)<=1.0;
        if(!std::isfinite(rate)||rate<1.0||rate>50.0) rate=1.0;
        if(same && (accelerated || rate>1.0)) {
            const double elapsed=now-last_time;
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
        return resolved;
    }
};

}
