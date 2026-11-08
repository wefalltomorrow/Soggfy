#pragma once
#include "ogg_tags.h"
#include "spotify_identity.h"
#include "rich_metadata.h"
#include "vorbis_comments.h"
#include <map>
#include <set>
#include <string_view>

namespace history {
enum class MetadataPlanState { Rejected, Unchanged, Changed };
namespace metadata_plan_detail {
inline std::string Key(std::string_view value) {
    std::string key(value);
    for(char& c:key)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    return key;
}
using Fields=std::map<std::string,std::vector<std::string>>;
inline bool Single(const Fields& fields,const char* key,std::string& value) {
    value.clear();
    auto it=fields.find(key);
    if(it==fields.end())return true;
    for(const auto& v:it->second) {
        if(v.empty())continue;
        if(!value.empty()&&value!=v)return false;
        value=v;
    }
    return true;
}
inline bool Exact(const Fields& fields,const char* key,const std::string& expected) {
    auto it=fields.find(key);
    if(it==fields.end()||it->second.empty())return false;
    for(const auto& value:it->second)if(value!=expected)return false;
    return true;
}
inline bool Populated(const Fields& fields,const std::string& key) {
    auto it=fields.find(key);
    if(it!=fields.end())for(const auto& value:it->second)if(!value.empty())return true;
    return false;
}
inline bool Date(const std::string& value) {
    if(value.size()!=4&&value.size()!=7&&value.size()!=10)return false;
    for(size_t i=0;i<value.size();++i) {
        if(i==4||i==7){if(value[i]!='-')return false;}
        else if(value[i]<'0'||value[i]>'9')return false;
    }
    const unsigned year=unsigned(std::stoi(value.substr(0,4)));
    if(!year)return false;
    if(value.size()==4)return true;
    const unsigned month=unsigned(std::stoi(value.substr(5,2)));
    if(!month||month>12)return false;
    if(value.size()==7)return true;
    const unsigned day=unsigned(std::stoi(value.substr(8,2)));
    const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=year%4==0&&(year%100!=0||year%400==0);
    return day&&day<=days[month-1]+unsigned(month==2&&leap);
}
inline bool Year(const std::string& value) {return value.size()==4&&Date(value);}
inline bool Text(const std::string& key,const std::string& value) {
    if(!MetadataTextValid(value))return false;
    if(key=="TRACKNUMBER"||key=="DISCNUMBER"||key=="TRACKTOTAL"||key=="DISCTOTAL") {
        if(value.empty()||value.size()>6||value[0]<'1'||value[0]>'9')return false;
        for(char c:value)if(c<'0'||c>'9')return false;
    }
    return true;
}
inline bool AddSize(size_t& bytes,size_t amount) {
    if(amount>VorbisCommentByteLimit-bytes)return false;
    bytes+=amount;return true;
}
}

// Policy only. A successful plan NEVER authorizes a file rewrite: the caller
// must validate full audio, hold/recheck file identity and settings, verify the
// retagged payload, and publish transactionally. Artwork is handled separately.
// Returned fields are the minimal replacement set for TagOgg/TagFlac.
inline MetadataPlanState PlanMetadataRefresh(const VorbisComments& existing,
    const std::string& expected_uri,const Tags& incoming,Tags& delta) {
    using namespace metadata_plan_detail;
    delta={};
    if((!ValidSpotifyUri(expected_uri,"track")&&!ValidSpotifyUri(expected_uri,"episode"))||
       existing.fields.size()>VorbisCommentFieldLimit||incoming.fields.size()>VorbisCommentFieldLimit)
        return MetadataPlanState::Rejected;
    // Bound both inputs before copying any fields, even when called directly.
    size_t bytes=8;
    if(!AddSize(bytes,existing.vendor.size()))return MetadataPlanState::Rejected;
    for(const auto& field:existing.fields)
        if(!AddSize(bytes,4)||!AddSize(bytes,field.size()))return MetadataPlanState::Rejected;
    bytes=8;
    for(const auto& field:incoming.fields)
        if(!AddSize(bytes,5)||!AddSize(bytes,field.first.size())||!AddSize(bytes,field.second.size()))
            return MetadataPlanState::Rejected;
    Fields old,fresh;
    for(const auto& field:existing.fields) {
        const auto split=field.find('=');
        if(split!=std::string::npos)old[Key(std::string_view(field).substr(0,split))].push_back(field.substr(split+1));
        else old[Key(field)].push_back("<opaque field>");
    }
    for(const auto& field:incoming.fields)fresh[Key(field.first)].push_back(field.second);
    if(!Exact(old,"SPOTIFY_URI",expected_uri)||!Exact(fresh,"SPOTIFY_URI",expected_uri)||
       !Exact(old,"HISTORY_COMPLETE_LISTEN","1")||!Exact(old,"HISTORY_TRANSCODED","0")||
       (!Exact(old,"HISTORY_CAPTURE","native compressed Ogg pages")&&
        !Exact(old,"HISTORY_CAPTURE","native compressed FLAC input")))return MetadataPlanState::Rejected;
    std::string old_album,new_album;
    if(!Single(old,"SPOTIFY_ALBUM_URI",old_album)||!Single(fresh,"SPOTIFY_ALBUM_URI",new_album)||
       (!old_album.empty()&&!ValidSpotifyUri(old_album,"album"))||
       (!new_album.empty()&&!ValidSpotifyUri(new_album,"album"))||
       (!old_album.empty()&&!new_album.empty()&&old_album!=new_album))return MetadataPlanState::Rejected;
    Tags plan;
    // Capture/quality evidence, custom fields, artwork and album provenance
    // cannot be replaced through the general text policy. Keep every populated
    // repeated field, including artist lists, even when only some entries are blank.
    static const std::set<std::string> text={"TITLE","ARTIST","ALBUM","ALBUMARTIST",
        "GENRE","LYRICS","TRACKNUMBER","DISCNUMBER","DISCTOTAL","TRACKTOTAL",
        "ISRC","LABEL","ORGANIZATION","PUBLISHER","COPYRIGHT","PHONOGRAMCOPYRIGHT",
        "COMPOSER","REMIXER","CONDUCTOR","LANGUAGE","COMMENT","SHOW","AUTHOR"};
    for(const auto& group:fresh) {
        if(!text.count(group.first)||Populated(old,group.first))continue;
        bool valid=true;
        for(const auto& value:group.second)if(!value.empty()&&!Text(group.first,value))valid=false;
        if(!valid)continue;
        for(const auto& value:group.second)
            if(!value.empty())plan.fields.emplace_back(group.first,value);
    }
    std::string old_date,old_year,new_date,new_year;
    const bool coherent=Single(old,"DATE",old_date)&&Single(old,"YEAR",old_year)&&
        Single(fresh,"DATE",new_date)&&Single(fresh,"YEAR",new_year)&&
        (old_date.empty()||Date(old_date))&&(new_date.empty()||Date(new_date))&&
        (old_year.empty()||Year(old_year))&&(new_year.empty()||Year(new_year))&&
        (old_date.empty()||old_year.empty()||old_date.substr(0,4)==old_year)&&
        (new_date.empty()||new_year.empty()||new_date.substr(0,4)==new_year);
    if(coherent) {
        std::string date=old_date,year=old_year;
        // Refine compatible precision only; never move a populated date to
        // another release, month or day or create a conflicting DATE/YEAR pair.
        if(!new_date.empty()&&(year.empty()||new_date.substr(0,4)==year)&&
           (date.empty()||(new_date.size()>date.size()&&new_date.compare(0,date.size(),date)==0)))
            date=new_date;
        if(year.empty())year=date.empty()?new_year:date.substr(0,4);
        if(date!=old_date)plan.fields.emplace_back("DATE",date);
        if(year!=old_year)plan.fields.emplace_back("YEAR",year);
    }
    if(plan.fields.empty())return MetadataPlanState::Unchanged;
    // Check the prospective merged size/count without constructing another
    // copy of every old field. Reserve the Ogg signature and framing overhead
    // so either native container can serialize a successful text plan.
    std::set<std::string> replaced;
    for(const auto& field:plan.fields)replaced.insert(field.first);
    size_t count=plan.fields.size();bytes=16;
    // The Ogg tagger supplies this vendor when the source vendor is empty.
    const size_t vendor_size=existing.vendor.empty()?std::string_view("SpotifyRepair native history").size():existing.vendor.size();
    if(!AddSize(bytes,vendor_size))return MetadataPlanState::Rejected;
    for(const auto& field:existing.fields) {
        const auto split=field.find('=');
        if(replaced.count(Key(std::string_view(field).substr(0,split))))continue;
        if(++count>VorbisCommentFieldLimit||!AddSize(bytes,4)||!AddSize(bytes,field.size()))
            return MetadataPlanState::Rejected;
    }
    if(count>VorbisCommentFieldLimit)return MetadataPlanState::Rejected;
    for(const auto& field:plan.fields)
        if(!AddSize(bytes,5)||!AddSize(bytes,field.first.size())||!AddSize(bytes,field.second.size()))
            return MetadataPlanState::Rejected;
    delta=std::move(plan);return MetadataPlanState::Changed;
}
}
