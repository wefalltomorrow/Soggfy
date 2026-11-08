#pragma once
#include <cstdint>
namespace history {
// Dynamic call context, not a Spotify playback ID. No private object offsets.
struct DecoderContext {std::uintptr_t owner=0;unsigned epoch=0;bool active=false;};
inline thread_local DecoderContext decoder_context;
class DecoderScope {
    DecoderContext previous_;
public:
    DecoderScope(std::uintptr_t owner,unsigned epoch) noexcept:previous_(decoder_context) {
        decoder_context={owner,epoch,true};
    }
    ~DecoderScope(){decoder_context=previous_;}
    DecoderScope(const DecoderScope&)=delete;
    DecoderScope& operator=(const DecoderScope&)=delete;
};
struct CaptureOwner {
    std::uintptr_t owner=0;
    bool Accept(std::uintptr_t observed) noexcept {
        // Unscoped callbacks retain legacy behavior, never prove ownership.
        if(!observed)return true;
        if(owner&&owner!=observed)return false;
        owner=observed;return true;
    }
};
}
