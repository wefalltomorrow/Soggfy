#include "../native/picture_envelope.h"
#include <cassert>
#include <cstdio>
#include <string>
using namespace history;
using Bytes=std::vector<uint8_t>;
static void Number(Bytes& b,uint32_t n) {for(int shift:{24,16,8,0})b.push_back(uint8_t(n>>shift));}
static void Field(Bytes& b,std::string_view s) {Number(b,uint32_t(s.size()));b.insert(b.end(),s.begin(),s.end());}
static Bytes Picture(std::string_view mime="image/png",std::string_view description=u8"日本語") {
    Bytes b;Number(b,3);Field(b,mime);Field(b,description);
    for(uint32_t n:{640u,480u,24u,0u})Number(b,n);
    Field(b,std::string_view("x\0y",3));return b;
}
static void Reject(const Bytes& b) {
    PictureEnvelope out;out.type=3;out.mime="stale";out.data="stale";out.linked=true;
    assert(!ParsePictureEnvelope(b.data(),b.size(),out));
    assert(out.type==0&&out.mime.empty()&&out.data.empty()&&!out.linked);
    assert(!SummarizePicture(b.data(),b.size()).envelope_valid);
}
int main() {
    auto b=Picture();PictureEnvelope out;
    assert(ParsePictureEnvelope(b.data(),b.size(),out));
    assert(out.type==3&&out.mime=="image/png"&&out.description==u8"日本語"&&!out.linked);
    assert(out.width==640&&out.height==480&&out.depth==24&&out.colors==0);
    assert(out.data==std::string_view("x\0y",3)); // not an image: envelope validity is separate
    for(size_t n=0;n<b.size();++n)Reject(Bytes(b.begin(),b.begin()+n));
    auto bad=b;bad.push_back(0);Reject(bad);
    // Every length field must reject UINT32_MAX without overflow or allocation.
    for(size_t at:{size_t(4),size_t(17),b.size()-7}) {
        bad=b;for(size_t i=0;i<4;++i)bad[at+i]=255;Reject(bad);
    }
    bad=b;bad[3]=21;Reject(bad);
    Reject(Picture(""));Reject(Picture("image/\npng"));Reject(Picture(std::string("image/\xff",7)));
    for(const auto& s:{std::string("\xc0\xaf",2),std::string("\xed\xa0\x80",3),
        std::string("\xf4\x90\x80\x80",4),std::string("\xe2\x82",2),std::string("\x80",1)})Reject(Picture("image/png",s));
    b=Picture("image/png",std::string("a\0b",3));assert(ParsePictureEnvelope(b.data(),b.size(),out));
    b=Picture("-->","");assert(ParsePictureEnvelope(b.data(),b.size(),out)&&out.linked);
    b=Picture();auto summary=SummarizePicture(b.data(),b.size());b.clear();
    assert(summary.envelope_valid&&summary.mime=="image/png"&&summary.data_bytes==3&&summary.width==640);
    Bytes decoded;
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{
        {"Zg==","f"},{"Zm8=","fo"},{"Zm9v","foo"},{"Zm9vYg==","foob"},
        {"Zm9vYmE=","fooba"},{"Zm9vYmFy","foobar"},{"AA==",std::string(1,0)}}) {
        assert(DecodePictureBase64(pair.first,decoded));
        assert(std::string(decoded.begin(),decoded.end())==pair.second);
    }
    for(const std::string text:{"","Zg","Zg=","Zg===","Zh==","Zm9=","Zg==AAAA",
        "=g==","Z=g=","Zg=A","Zg==\n","Zg-_","!!!!","===="}) {
        decoded={1,2,3};assert(!DecodePictureBase64(text,decoded)&&decoded.empty());
    }
    // Independent fixed wire fixture: type 3, image/png, no description,
    // zero informational dimensions, one payload byte (0x78).
    assert(DecodePictureBase64("AAAAAwAAAAlpbWFnZS9wbmcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAF4",decoded));
    assert(ParsePictureEnvelope(decoded.data(),decoded.size(),out));
    assert(out.mime=="image/png"&&out.type==3&&out.data=="x"&&out.width==0);
    // Size limits are checked before decoder allocation and clear stale output.
    std::string large(((PictureEnvelopeLimit+2)/3+1)*4,'A');
    decoded={1};assert(!DecodePictureBase64(large,decoded)&&decoded.empty());
    uint32_t random=0x56e19a73;
    for(unsigned iteration=0;iteration<10000;++iteration) {
        auto mutation=Picture();random=random*1664525u+1013904223u;
        mutation[random%mutation.size()]^=uint8_t((random>>24)|1);
        if(ParsePictureEnvelope(mutation.data(),mutation.size(),out)) {
            assert(out.data.data()>=reinterpret_cast<const char*>(mutation.data()));
            assert(out.data.data()+out.data.size()==reinterpret_cast<const char*>(mutation.data()+mutation.size()));
        }
    }
    std::puts("PASS: bounded picture envelopes, UTF-8, canonical base64, links and opaque rejection");
}
