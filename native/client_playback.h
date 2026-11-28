#pragma once
#include "rich_metadata.h"
#include "spotify_identity.h"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>

namespace history {
// This is client observation, not proof of a decoder's playback ownership.
struct ClientPlayback {
    RichMetadata media;
    std::string instance;
    int paused=-1,buffering=-1,local=-1;
    bool seen=false,available=false;
    double received=0,identity_since=0,position=-1;
    std::uint64_t boundary=0;
};
inline bool ClientPlaybackAgrees(const ClientPlayback& client,const std::string& title,
    const std::string& artist,const std::string& album,double position,double duration,double at,double rate) {
    if(!client.media.Matches(title,artist,album,duration))return false;
    if(client.instance.empty())return true;
    if(client.position<0||at<client.identity_since)return false;
    const double expected=std::clamp(client.position+
        ((client.paused==0&&client.buffering==0)?(at-client.received)*rate:0.),0.,duration);
    return std::fabs(position-expected)<=std::max(1.5,rate*.25);
}
inline bool ParseClientPlayback(const std::string& input,ClientPlayback& out) {
    if(input.size()>4096)return false;
    ClientPlayback next;std::string metadata;unsigned fields=0;
    for(std::size_t at=0;at<input.size();) {
        auto end=input.find('&',at);if(end==std::string::npos)end=input.size();
        const auto eq=input.find('=',at);if(eq==std::string::npos||eq>=end)return false;
        const auto key=input.substr(at,eq-at),value=input.substr(eq+1,end-eq-1);
        unsigned bit=0;int* flag=nullptr;
        if(key=="instance")bit=1;
        else if(key=="paused"){bit=2;flag=&next.paused;}
        else if(key=="buffering"){bit=4;flag=&next.buffering;}
        else if(key=="local"){bit=8;flag=&next.local;}
        else if(key=="position")bit=16;
        if(bit) {
            if(fields&bit)return false;
            fields|=bit;
            if(flag) {if(value!="0"&&value!="1")return false;*flag=value=="1";}
            else if(bit==16) {
                char* end_number=nullptr;
                if(value.empty()||value.find_first_not_of("0123456789.")!=std::string::npos)return false;
                next.position=std::strtod(value.c_str(),&end_number);
                if(!end_number||*end_number||!std::isfinite(next.position)||next.position<0)return false;
            } else {
                if(value.size()!=32)return false;
                bool nonzero=false;
                for(char c:value) {
                    if(c>='A'&&c<='F')c=char(c-'A'+'a');
                    if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
                    nonzero|=c!='0';next.instance+=c;
                }
                if(!nonzero)return false;
            }
        } else {if(!metadata.empty())metadata+='&';metadata+=input.substr(at,end-at);}
        at=end+1;
    }
    std::string error;
    if(!ParsePlaybackMetadata(metadata,next.media,error)||
       !ValidSpotifyUri(next.media.uri,next.media.kind=="episode"?"episode":"track"))return false;
    if(next.position>next.media.duration)return false;
    next.available=next.local!=0;out=std::move(next);return true;
}

class ClientPlaybackState {
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"audio fence must be lock-free");
    std::mutex lock_;
    ClientPlayback snapshot_;
    // Even = media permitted, odd = unknown/unsupported/remote content.
    // Both edges advance the fence, so A -> ad -> A cannot reuse old events.
    std::atomic<std::uint64_t> boundary_{0};
    void SetBlocked(bool blocked) {
        auto token=boundary_.load(std::memory_order_relaxed);
        if(bool(token&1)!=blocked)++token;
        snapshot_.boundary=token;boundary_.store(token,std::memory_order_release);
    }
public:
    std::uint64_t Token() const {return boundary_.load(std::memory_order_acquire);}
    void Publish(ClientPlayback next,double now) {
        std::lock_guard<std::mutex> guard(lock_);
        next.identity_since=snapshot_.seen&&snapshot_.available==next.available&&
            snapshot_.instance==next.instance&&snapshot_.media.uri==next.media.uri?snapshot_.identity_since:now;
        next.seen=true;next.received=now;snapshot_=std::move(next);
        SetBlocked(!snapshot_.available||!std::isfinite(now));
    }
    ClientPlayback Read(double now) {
        std::lock_guard<std::mutex> guard(lock_);
        if(snapshot_.seen&&(!std::isfinite(now)||now<snapshot_.received||now-snapshot_.received>3)) {
            snapshot_.available=false;SetBlocked(true);
        }
        return snapshot_;
    }
};
inline ClientPlaybackState client_playback;
} // namespace history
