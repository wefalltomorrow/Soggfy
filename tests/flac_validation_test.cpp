#include "../native/flac_validation.h"
#include "../native/flac_history_core.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);assert(file);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    history::FlacInfo info;size_t audio=0;
    assert(history::ParseFlacMetadata(bytes,info,audio)==history::FlacParse::Valid);
    history::FlacDecodedCoverage decoded;std::string error;
    if(!history::DecodeFlacRecording(bytes,decoded,error)){std::cerr<<error<<"\n";return 1;}
    assert(decoded.samples==info.total_samples&&decoded.frames&&decoded.rate==info.rate&&
           decoded.channels==info.channels&&decoded.bits==info.bits);
    auto reject=[&](const std::vector<uint8_t>& bad) {
        decoded={999,999,999,999,999};
        assert(!history::DecodeFlacRecording(bad,decoded,error));
        assert(!error.empty()&&!decoded.samples&&!decoded.frames&&!decoded.rate&&!decoded.channels&&!decoded.bits);
    };
    for(size_t missing=1;missing<=std::min(size_t(32),bytes.size()-audio);++missing)
        reject(std::vector<uint8_t>(bytes.begin(),bytes.end()-missing));
    auto bad=bytes;bad.push_back(0);reject(bad);
    bad=bytes;bad.insert(bad.end(),bytes.begin()+audio,bytes.end());reject(bad);
    bad=bytes;bad[25]^=1;reject(bad); // declared coverage differs by one sample
    bad=bytes;bad[26]^=1;reject(bad); // known PCM checksum must match
    bad=bytes;bad.back()^=1;reject(bad); // final frame CRC
    for(size_t i=0;i<16;++i) {
        bad=bytes;bad[audio+(bytes.size()-audio-1)*i/15]^=0x40;reject(bad);
    }
    // FLAC permits absent MD5. Frame CRC, full payload and coverage remain required.
    bad=bytes;std::fill(bad.begin()+26,bad.begin()+42,0);
    assert(history::DecodeFlacRecording(bad,decoded,error));
    assert(!history::DecodeFlacRecording(bytes,decoded,error,[]{return true;}));
    assert(!decoded.samples);
    unsigned calls=0;
    assert(!history::DecodeFlacRecording(bytes,decoded,error,[&]{return ++calls>=4;}));
    assert(!decoded.samples);
    assert(!history::DecodeFlacRecording(bytes,decoded,error,[]()->bool{throw 1;}));
    assert(!decoded.samples);
    std::cout<<"PASS: FLAC full payload, exact coverage, CRC/MD5, trailing data and cancellation\n";
}
