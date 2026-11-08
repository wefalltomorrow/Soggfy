#include "vorbis_validation.h"
#include "existing_quality_core.h"
#include "vorbis_comments.h"
#include <vorbis/codec.h>
#include <cmath>

namespace history {
namespace {
struct Decoder {
    ogg_stream_state stream{};vorbis_info info{};vorbis_comment comments{};
    vorbis_dsp_state dsp{};vorbis_block block{};
    bool stream_ready=false,dsp_ready=false,block_ready=false;
    Decoder(){vorbis_info_init(&info);vorbis_comment_init(&comments);}
    ~Decoder(){
        if(block_ready)vorbis_block_clear(&block);
        if(dsp_ready)vorbis_dsp_clear(&dsp);
        vorbis_comment_clear(&comments);vorbis_info_clear(&info);
        if(stream_ready)ogg_stream_clear(&stream);
    }
};
}
bool DecodeVorbisRecording(const std::vector<uint8_t>& source,
    VorbisDecodedCoverage& output,std::string& error,const std::function<bool()>& canceled) {
    output={};error.clear();
    auto fail=[&](const char* why){error=why;return false;};
    auto stop=[&]{return canceled&&canceled();};
    if(stop())return fail("Vorbis validation canceled");
    Quality quality;
    if(!ParseExistingQualityHeader(source.data(),source.size(),quality)||quality.codec!=Codec::Vorbis)
        return fail("Invalid Vorbis identification");
    Decoder decoder;Stream validation;VorbisDecodedCoverage found;
    size_t offset=0,packet_bytes=0;uint64_t packets=0;bool pending=false,audio_eos=false;
    while(offset<source.size()) {
        if(stop())return fail("Vorbis validation canceled");
        if(validation.complete||source.size()-offset<27)return fail("Trailing or truncated Ogg page");
        const auto* b=source.data()+offset;const size_t header=27+b[26];
        if(header>source.size()-offset)return fail("Truncated Ogg lacing");
        size_t body=0;for(size_t i=27;i<header;++i)body+=b[i];
        if(body>source.size()-offset-header)return fail("Truncated Ogg body");
        Page page;if(!ParsePage(b,header+body,page))return fail("Invalid Ogg page CRC or header");
        if(!offset) {
            if(!page.vorbis_start||page.sequence>1)return fail("Missing Ogg stream start");
        } else if(page.serial!=validation.serial||page.sequence!=validation.next||(page.flags&2))
            return fail("Ogg stream identity or sequence changed");
        if(bool(page.flags&1)!=pending)return fail("Invalid Ogg packet continuation");
        for(size_t i=27;i<header;++i) {
            if(size_t(b[i])>VorbisCommentByteLimit-packet_bytes)return fail("Ogg packet exceeds validation limit");
            packet_bytes+=b[i];pending=b[i]==255;
            if(!pending)packet_bytes=0;
        }
        if((page.flags&4)&&pending)return fail("Ogg EOS inside incomplete packet");
        const auto state=validation.Push(b,header+body);
        if(state==Result::Invalid||state==Result::Ignore||state==Result::Replay)return fail("Invalid Ogg coverage");
        if(!decoder.stream_ready) {
            if(ogg_stream_init(&decoder.stream,int(page.serial)))return fail("Ogg decoder allocation failed");
            decoder.stream_ready=true;
        }
        ogg_page input={const_cast<unsigned char*>(b),long(header),const_cast<unsigned char*>(b+header),long(body)};
        if(ogg_stream_pagein(&decoder.stream,&input))return fail("Ogg decoder rejected page");
        ogg_packet packet;int ready;
        while((ready=ogg_stream_packetout(&decoder.stream,&packet))!=0) {
            if(stop())return fail("Vorbis validation canceled");
            if(ready<0||audio_eos)return fail("Ogg packet gap or data after EOS");
            if(packets<3) {
                if(packet.e_o_s||vorbis_synthesis_headerin(&decoder.info,&decoder.comments,&packet))
                    return fail("Invalid Vorbis codec headers");
                if(packets==2) {
                    if(unsigned(decoder.info.rate)!=quality.rate||unsigned(decoder.info.channels)!=quality.channels)
                        return fail("Vorbis codec quality mismatch");
                    if(vorbis_synthesis_init(&decoder.dsp,&decoder.info))return fail("Vorbis decoder initialization failed");
                    decoder.dsp_ready=true;
                    if(vorbis_block_init(&decoder.dsp,&decoder.block))return fail("Vorbis block initialization failed");
                    decoder.block_ready=true;found.rate=quality.rate;found.channels=quality.channels;
                }
            } else {
                if(packet.bytes<=0||vorbis_synthesis(&decoder.block,&packet))return fail("Invalid Vorbis audio packet");
                if(oggpack_bytes(&decoder.block.opb)>packet.bytes)return fail("Truncated Vorbis audio bits");
                if(vorbis_synthesis_blockin(&decoder.dsp,&decoder.block))return fail("Vorbis packet synthesis failed");
                float** pcm=nullptr;const int count=vorbis_synthesis_pcmout(&decoder.dsp,&pcm);
                if(count<0)return fail("Invalid Vorbis decoded sample count");
                for(unsigned channel=0;channel<found.channels;++channel)
                    for(int sample=0;sample<count;++sample)
                        if(!std::isfinite(pcm[channel][sample]))return fail("Nonfinite Vorbis audio");
                found.samples+=uint64_t(count);++found.packets;
                if(packet.granulepos>=0&&found.samples!=uint64_t(packet.granulepos))
                    return fail("Decoded Vorbis coverage differs from granule position");
                if(vorbis_synthesis_read(&decoder.dsp,count))return fail("Vorbis sample consumption failed");
                audio_eos=packet.e_o_s!=0;
            }
            ++packets;
        }
        offset+=header+body;
    }
    if(!validation.complete||!audio_eos||pending||found.packets<2||
       !found.samples||found.samples!=uint64_t(validation.samples))return fail("Incomplete decoded Vorbis recording");
    if(stop())return fail("Vorbis validation canceled");
    output=found;return true;
}
}
