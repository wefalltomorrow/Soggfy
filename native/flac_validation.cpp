#include "flac_validation.h"
#include "flac_history_core.h"
#include <FLAC/stream_decoder.h>
#include <algorithm>
#include <cstring>
#include <memory>
namespace history {
namespace {
struct Context {
    const std::vector<uint8_t>& source;
    const std::function<bool()>& canceled;
    FlacInfo info;
    FlacCoverage coverage;
    size_t at=0;
    bool failed=false;
    bool Stop() noexcept {
        try { return failed || (canceled && canceled()); }
        catch(...) { failed=true;return true; }
    }
};
FLAC__StreamDecoderReadStatus Read(const FLAC__StreamDecoder*,FLAC__byte buffer[],size_t* bytes,void* data) {
    auto& c=*static_cast<Context*>(data);
    if(!*bytes||c.Stop()) {c.failed=true;*bytes=0;return FLAC__STREAM_DECODER_READ_STATUS_ABORT;}
    *bytes=std::min(*bytes,c.source.size()-c.at);
    if(!*bytes)return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    std::memcpy(buffer,c.source.data()+c.at,*bytes);c.at+=*bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
FLAC__StreamDecoderTellStatus Tell(const FLAC__StreamDecoder*,FLAC__uint64* at,void* data) {
    *at=static_cast<Context*>(data)->at;return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}
FLAC__StreamDecoderWriteStatus Write(const FLAC__StreamDecoder*,const FLAC__Frame* frame,const FLAC__int32* const pcm[],void* data) {
    auto& c=*static_cast<Context*>(data);const auto& h=frame->header;
    const bool sample=h.number_type==FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER;
    if(c.Stop()||!c.coverage.Frame(c.info,h.blocksize,h.sample_rate,h.channels,h.bits_per_sample,
        sample?1:0,sample?h.number.sample_number:h.number.frame_number)) {
        c.failed=true;return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    const int64_t bound=int64_t(1)<<(h.bits_per_sample-1);
    for(unsigned channel=0;channel<h.channels;++channel)
        for(unsigned i=0;i<h.blocksize;++i)
            if(pcm[channel][i]<-bound||pcm[channel][i]>=bound) {
                c.failed=true;return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void Error(const FLAC__StreamDecoder*,FLAC__StreamDecoderErrorStatus,void* data) {
    // Never permit the reference decoder to recover by skipping bad frames.
    static_cast<Context*>(data)->failed=true;
}
}
bool DecodeFlacRecording(const std::vector<uint8_t>& source,FlacDecodedCoverage& output,
    std::string& error,const std::function<bool()>& canceled) {
    output={};error.clear();
    auto fail=[&](const char* reason){error=reason;return false;};
    Context c{source,canceled,{}, {}};size_t audio=0;
    if(c.Stop())return fail("FLAC validation canceled");
    if(ParseFlacMetadata(source,c.info,audio)!=FlacParse::Valid||audio>16*1024*1024||
       !c.info.total_samples||audio==source.size())return fail("invalid FLAC metadata or empty coverage");
    using Decoder=std::unique_ptr<FLAC__StreamDecoder,decltype(&FLAC__stream_decoder_delete)>;
    Decoder decoder(FLAC__stream_decoder_new(),FLAC__stream_decoder_delete);
    if(!decoder)return fail("FLAC decoder allocation failed");
    if(!FLAC__stream_decoder_set_md5_checking(decoder.get(),true)||
       FLAC__stream_decoder_init_stream(decoder.get(),Read,nullptr,Tell,nullptr,nullptr,Write,nullptr,Error,&c)!=
       FLAC__STREAM_DECODER_INIT_STATUS_OK)return fail("FLAC decoder initialization failed");
    if(!FLAC__stream_decoder_process_until_end_of_metadata(decoder.get())||c.Stop())
        return fail("FLAC decoder rejected metadata");
    FLAC__uint64 position=0;
    if(!FLAC__stream_decoder_get_decode_position(decoder.get(),&position)||position!=audio)
        return fail("FLAC metadata extent mismatch");
    while(c.coverage.samples<c.info.total_samples) {
        const auto previous=c.coverage.samples;
        if(c.Stop()||!FLAC__stream_decoder_process_single(decoder.get())||c.failed||
           c.coverage.samples==previous)return fail("FLAC audio decode, CRC or coverage failure");
    }
    if(c.Stop()||!c.coverage.frames||
       !FLAC__stream_decoder_get_decode_position(decoder.get(),&position)||position!=source.size())
        return fail("FLAC trailing data or canceled validation");
    if(!FLAC__stream_decoder_finish(decoder.get()))return fail("FLAC PCM checksum mismatch");
    output={c.coverage.samples,c.coverage.frames,c.info.rate,c.info.channels,c.info.bits};
    return true;
}
}
