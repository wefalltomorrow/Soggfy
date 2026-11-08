#include "../native/metadata_refresh_plan.h"
#include <cassert>
#include <cstdio>
using namespace history;
static const std::string uri="spotify:track:0123456789abcdefghijkl";
static VorbisComments Original() {
    return {"original vendor",{"SPOTIFY_URI="+uri,"HISTORY_COMPLETE_LISTEN=1",
        "HISTORY_TRANSCODED=0","HISTORY_CAPTURE=native compressed Ogg pages",
        "HISTORY_SAMPLES=12345","CUSTOM=preserve","METADATA_BLOCK_PICTURE=opaque"}};
}
static Tags Incoming() {return {{{"SPOTIFY_URI",uri}}, {1,2,3},"image/jpeg"};}
static std::vector<std::string> Values(const Tags& tags,const std::string& key) {
    std::vector<std::string> values;
    for(const auto& field:tags.fields)if(field.first==key)values.push_back(field.second);
    return values;
}
int main() {
    auto old=Original();auto fresh=Incoming();Tags delta;
    auto run=[&](MetadataPlanState expected) {
        delta={{{"stale","stale"}},{1},"stale"};
        const auto old_fields=old.fields;const auto fresh_fields=fresh.fields;
        assert(PlanMetadataRefresh(old,uri,fresh,delta)==expected);
        assert(old.fields==old_fields&&fresh.fields==fresh_fields);
        assert(delta.cover.empty()&&delta.mime.empty());
        if(expected!=MetadataPlanState::Changed)assert(delta.fields.empty());
    };
    run(MetadataPlanState::Unchanged);
    assert(PlanMetadataRefresh(old,"",fresh,delta)==MetadataPlanState::Rejected&&delta.fields.empty());
    const std::string episode="spotify:episode:0123456789abcdefghijkl";
    auto podcast=old;podcast.fields[0]="SPOTIFY_URI="+episode;auto podcast_tags=fresh;podcast_tags.fields[0].second=episode;
    assert(PlanMetadataRefresh(podcast,episode,podcast_tags,delta)==MetadataPlanState::Unchanged);
    fresh.fields.insert(fresh.fields.end(),{{"TITLE",u8"日本語"},{"ARTIST","first"},
        {"artist","second"},{"HISTORY_SAMPLES","98765"},{"CUSTOM","overwrite"},
        {"METADATA_BLOCK_PICTURE","replace"}});
    run(MetadataPlanState::Changed);
    assert(delta.fields.size()==3&&Values(delta,"ARTIST")==std::vector<std::string>({"first","second"}));
    assert(Values(delta,"TITLE")==std::vector<std::string>({u8"日本語"}));
    old.fields.insert(old.fields.end(),{"title=old","ARTIST=","aRtIsT=custom artist","ARTIST=other artist"});
    run(MetadataPlanState::Unchanged);
    old=Original();old.fields.push_back("TITLE");old.fields.push_back("ARTIST=");
    run(MetadataPlanState::Changed);assert(Values(delta,"TITLE").empty());
    old=Original();fresh=Incoming();
    fresh.fields.insert(fresh.fields.end(),{{"TITLE",std::string("bad")+char(0)+"value"},{"ARTIST",std::string(1,char(0xff))}});
    run(MetadataPlanState::Unchanged);
    // Complete markers are eligibility only, not a payload validation result.
    for(size_t i=0;i<4;++i) {
        old=Original();old.fields.erase(old.fields.begin()+i);run(MetadataPlanState::Rejected);
    }
    for(const auto& field:{"SPOTIFY_URI=spotify:track:abcdefghijkl0123456789",
        "HISTORY_COMPLETE_LISTEN=0","history_transcoded=1","HISTORY_CAPTURE=unknown","SPOTIFY_URI="}) {
        old=Original();old.fields.push_back(field);run(MetadataPlanState::Rejected);
    }
    old=Original();fresh=Incoming();fresh.fields.clear();run(MetadataPlanState::Rejected);
    fresh=Incoming();fresh.fields.push_back({"SPOTIFY_URI","other"});run(MetadataPlanState::Rejected);
    fresh=Incoming();old.fields.push_back("spotify_uri="+uri);run(MetadataPlanState::Unchanged);
    old.fields[3]="HISTORY_CAPTURE=native compressed FLAC input";run(MetadataPlanState::Unchanged);
    old=Original();fresh=Incoming();
    old.fields.push_back("SPOTIFY_ALBUM_URI=spotify:album:0123456789abcdefghijkl");
    fresh.fields.push_back({"SPOTIFY_ALBUM_URI","spotify:album:abcdefghijkl0123456789"});
    run(MetadataPlanState::Rejected);
    fresh=Incoming();fresh.fields.push_back({"SPOTIFY_ALBUM_URI","malformed"});run(MetadataPlanState::Rejected);
    old=Original();fresh=Incoming();
    for(const auto& value:{"0","-1","01","1/12","1234567"})fresh.fields.push_back({"TRACKNUMBER",value});
    run(MetadataPlanState::Unchanged);
    fresh=Incoming();fresh.fields.push_back({"TRACKNUMBER","12"});run(MetadataPlanState::Changed);
    // Compatible precision upgrades and missing year derive from the date.
    old=Original();old.fields.push_back("DATE=2024");fresh=Incoming();fresh.fields.push_back({"DATE","2024-02-29"});
    run(MetadataPlanState::Changed);
    assert(Values(delta,"DATE")==std::vector<std::string>({"2024-02-29"}));
    assert(Values(delta,"YEAR")==std::vector<std::string>({"2024"}));
    old.fields.push_back("YEAR=2023");
    run(MetadataPlanState::Unchanged);
    old=Original();old.fields.push_back("YEAR=2023");run(MetadataPlanState::Unchanged);
    old=Original();old.fields.push_back("DATE=2024-01");
    run(MetadataPlanState::Changed);assert(Values(delta,"DATE").empty());
    assert(Values(delta,"YEAR")==std::vector<std::string>({"2024"}));
    old.fields.push_back("DATE=2024-03");run(MetadataPlanState::Unchanged);
    for(const auto& date:{"0000","2023-02-29","1900-02-29","2024-13","2024-00",
        "2024-04-31","2024-01-00","2024-1-01","2024-01-01junk"}) {
        old=Original();fresh=Incoming();fresh.fields.push_back({"DATE",date});run(MetadataPlanState::Unchanged);
    }
    for(const auto& date:{"2024","2024-02","2024-02-29","2000-02-29","2023-02-28"}) {
        old=Original();fresh=Incoming();fresh.fields.push_back({"DATE",date});run(MetadataPlanState::Changed);
        assert(Values(delta,"YEAR")==std::vector<std::string>({std::string(date).substr(0,4)}));
    }
    fresh.fields.push_back({"YEAR","1900"});run(MetadataPlanState::Unchanged);
    // Existing lyrics, including synchronized or custom text, remain exact.
    old=Original();old.fields.push_back("LYRICS=[00:01.00]Original");fresh=Incoming();
    fresh.fields.push_back({"LYRICS","different"});run(MetadataPlanState::Unchanged);
    old=Original();run(MetadataPlanState::Changed);assert(Values(delta,"LYRICS")==std::vector<std::string>({"different"}));
    // Limits apply to both direct inputs and the merged output.
    old=Original();old.fields.push_back("HISTORY_SAMPLE_RATE=1000");
    old.fields.push_back("LYRICS=one\r\ntwo é");fresh=Incoming();
    fresh.fields.push_back({"LYRICS","[00:01.000]one\n[00:12.345]two é"});
    run(MetadataPlanState::Changed);assert(Values(delta,"LYRICS")==std::vector<std::string>({fresh.fields.back().second}));
    for(const auto* lyrics:{"[00:01.000]other\n[00:12.345]two é","[00:02.000]one\n[00:01.000]two é",
        "[00:01.000]one\n[00:12.346]two é","[00:60.000]one\n[00:12.345]two é",
        "[offset:1]\n[00:01.000]one\n[00:12.345]two é","[00:01.000]one\n[00:12.345]two é\n",
        "[00:01.000]one\n[00:12.]two é","[00:01.000]one\n[00:12.3]two é"}) {
        fresh.fields.back().second=lyrics;run(MetadataPlanState::Unchanged);
    }
    fresh.fields.back().second="[00:01.00]one\n[00:12.34]two é";
    run(MetadataPlanState::Changed);
    old.fields.push_back("LYRICS=different");run(MetadataPlanState::Unchanged);
    old=Original();fresh=Incoming();fresh.fields.push_back({"LYRICS","different"});
    // Limits apply to both direct inputs and the merged output.
    old.fields.resize(VorbisCommentFieldLimit,"CUSTOM=x");run(MetadataPlanState::Rejected);
    old.fields.back()="LYRICS=";run(MetadataPlanState::Changed);
    old.fields.push_back("CUSTOM=x");run(MetadataPlanState::Rejected);
    old=Original();fresh.fields.resize(VorbisCommentFieldLimit+1);run(MetadataPlanState::Rejected);
    fresh=Incoming();fresh.fields.push_back({"LYRICS",std::string(VorbisCommentByteLimit,'x')});run(MetadataPlanState::Rejected);
    fresh=Incoming();old.vendor=std::string(VorbisCommentByteLimit,'x');run(MetadataPlanState::Rejected);
    // An empty Ogg vendor expands during tagging; include its default length.
    old=Original();old.vendor.clear();fresh=Incoming();fresh.fields.push_back({"GENRE","x"});
    size_t size=16;for(const auto& field:old.fields)size+=4+field.size();
    size+=4+std::string("GENRE=x").size()+4+std::string("CUSTOM=").size();
    old.fields.push_back("CUSTOM="+std::string(VorbisCommentByteLimit-size,'x'));
    run(MetadataPlanState::Rejected);
    std::puts("PASS: metadata delta identity/provenance, preservation, repeated artists, date coherence and bounds");
}
