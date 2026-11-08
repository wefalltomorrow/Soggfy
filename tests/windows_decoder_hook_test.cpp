#include "../native/playback_speed.cpp"
#include <cassert>
history::DecoderContext ReadOtherTranslationUnit();
static unsigned test_epoch=7;
static double test_speed=1;
static bool test_downloads=true;
extern "C" FARPROC ResolveVersionExport(unsigned){return nullptr;}
namespace history {
Settings GetSettings(){Settings s;s.downloads=test_downloads;s.playback_speed=test_speed;return s;}
unsigned CaptureEpoch(){return test_epoch;}
void HistoryLog(const char*){}
}
static history::DecodeResult* Decode(void* self,history::DecodeResult* output,float*,
    std::uint64_t* samples,const unsigned char*,std::uint64_t* encoded,unsigned char){
    const auto context=ReadOtherTranslationUnit();
    assert(context.active&&context.owner==reinterpret_cast<std::uintptr_t>(self)&&context.epoch==test_epoch);
    ++test_epoch;
    assert(ReadOtherTranslationUnit().epoch==test_epoch-1);
    if(samples)*samples=300;
    if(encoded)*encoded=10;
    return output;
}
int main(){
    using namespace history;
    original_decode=Decode;
    for(bool downloads:{false,true})for(double speed:{1.,2.,10.,30.}){
        test_downloads=downloads;test_speed=speed;
        DecodeResult output{};float pcm[300]{};std::uint64_t samples=300,encoded=40;
        assert(DecodeAudioHook(reinterpret_cast<void*>(123),&output,pcm,&samples,nullptr,&encoded,0)==&output);
        assert(encoded==10&&samples==(downloads?ClassicPcmSamplesToKeep(300,speed):300));
        assert(!ReadOtherTranslationUnit().active);
    }
    {
        DecoderScope outer(456,99);DecodeResult output{};
        assert(DecodeAudioHook(reinterpret_cast<void*>(123),&output,nullptr,nullptr,nullptr,nullptr,0)==&output);
        assert(ReadOtherTranslationUnit().owner==456&&ReadOtherTranslationUnit().epoch==99);
    }
    puts("PASS: real decoder hook scope, immutable entry epoch, original returns and 1x-30x/downloads-off PCM behavior");
}
