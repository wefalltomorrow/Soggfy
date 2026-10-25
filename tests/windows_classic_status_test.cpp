#include "../native/classic_ui_backend.h"
#include "../native/history_settings.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace fs=std::filesystem;
static history::Settings settings;
namespace history {
Settings GetSettings(){return settings;}
std::wstring WindowsPath(const std::wstring& path){return path;}
bool DebugLoggingEnabled(){return false;}
void HistoryLog(const char*){}
}
using namespace history;
static void write(const fs::path& path,const char* data="audio"){fs::create_directories(path.parent_path());std::ofstream f(path);f<<data;assert(f.good());}
static ClassicTrackQuery query(const std::wstring& title){return {"spotify:track:test",title,L"Artist",L"Album",L"Artist"};}
static void published(const ClassicTrackQuery& q,const fs::path& path) {
    Media media;media.title=q.title;media.artist=q.all_artists;media.album=q.album;
    SetClassicTrackStatus(media,"DONE",{},path.wstring());
}
static std::string status(const ClassicTrackQuery& q){return QueryClassicTrackStatuses({q}).at(0).status;}
int main() {
    const auto base=fs::temp_directory_path()/(L"soggfy-status-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    assert(fs::create_directory(base));settings.root=base.wstring();settings.path_template=L"{all_artist_names} - {track_name}.{ext}";settings.output_ext=L"mp3";
    auto q=query(L"Song");const auto file=base/L"Artist - Song.mp3";
    assert(status(q).empty());write(file);published(q,file);assert(status(q)=="DONE");
    ClassicStatusMetrics warm;QueryClassicTrackStatuses({q},&warm);assert(warm.scans==0&&warm.compilations==0&&warm.regex_checks==0&&warm.cache_hits==1);
    fs::remove(file);assert(status(q).empty()); // Both recent DONE and indexed DONE must verify the file.
    write(file,"");published(q,file);assert(status(q).empty());
    fs::remove(file);fs::create_directory(file);published(q,file);assert(status(q).empty());fs::remove(file);
    write(file);published(q,file);assert(status(q)=="DONE");
    const auto duplicate=base/L"old"/L"Artist - Song.mp3";write(duplicate);published(q,file);assert(status(q)=="WARN");
    fs::remove(duplicate);assert(status(q)=="DONE");
    HANDLE lock=CreateFileW(file.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
    assert(status(q).empty());CloseHandle(lock);assert(status(q)=="DONE");
    write(duplicate);published(q,file);
    lock=CreateFileW(duplicate.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);assert(lock!=INVALID_HANDLE_VALUE);
    assert(status(q)=="DONE");CloseHandle(lock);assert(status(q)=="WARN");
    fs::remove(duplicate);assert(status(q)=="DONE");
    settings.root=(base/L"other").wstring();fs::create_directory(settings.root);assert(status(q).empty());
    settings.root=base.wstring();assert(status(q)=="DONE");
    q={"spotify:track:unicode",L"曲",L"AC/DC",L"盤",L"AC/DC, Guest"};
    const auto unicode=base/L"AC／DC; Guest - 曲.mp3";write(unicode);published(q,unicode);assert(status(q)=="DONE");
    q={"spotify:track:legacy",L"Old",L"Artist",L"Album",L"Artist"};
    const auto legacy=base/L"Artist"/L"Album"/L"1. Old.mp3";write(legacy);published(q,legacy);assert(status(q)=="DONE");
    q={"spotify:episode:episode",L"Talk",L"Host",L"Show",L"Host"};settings.podcast_template=L"Podcasts/{album_name}/{track_name}.{ext}";settings.output_ext=L"customaudio";
    const auto podcast=base/L"Podcasts"/L"Show"/L"Talk.customaudio";write(podcast);published(q,podcast);assert(status(q)=="DONE");
    settings.podcast_template=L"Changed/{track_name}.{ext}";assert(status(q).empty());
    q=query(L"Same");q.artist=q.all_artists=L"Air";auto other=q;other.artist=other.all_artists=L"Air Supply";
    const auto wrong=base/L"unrelated.mp3";write(wrong);published(other,wrong);assert(status(q).empty());
    // A template need not contain the title. Unknown nested tokens must not
    // accidentally enable the literal-title prefilter either.
    settings.root=(base/L"custom").wstring();fs::create_directory(settings.root);
    write(fs::path(settings.root)/L"Artist.mp3");
    settings.path_template=L"{artist_name}.{ext}";assert(status(q)=="");
    q=query(L"Absent title");assert(status(q)=="DONE");
    settings.path_template=L"{foo{track_name}.{ext}";assert(status(q)=="DONE");
    settings.path_template=L"{all_artist_names} - {track_name}.{ext}";
    // Partial scans must not claim absence or uniqueness, even if one matching
    // file happens to be visible before the limit.
    settings.root=(base/L"limits").wstring();q=query(L"Limited");
    const auto limited=fs::path(settings.root)/L"Artist - Limited.mp3";write(limited);
    write(fs::path(settings.root)/L"deep"/L"nested"/L"Artist - Limited.mp3");
    ClassicStatusControl control;control.index_lifetime_ms=0;
    control.max_files=1;assert(QueryClassicTrackStatuses({q},nullptr,control).at(0).status=="WARN");
    control.max_files=100000;control.max_directories=1;
    assert(QueryClassicTrackStatuses({q},nullptr,control).at(0).status=="WARN");
    control.max_directories=10000;control.max_depth=0;
    assert(QueryClassicTrackStatuses({q},nullptr,control).at(0).status=="WARN");
    control.max_depth=12;
    assert(QueryClassicTrackStatuses({q},nullptr,control).at(0).status=="WARN"); // Actual ambiguity.
    fs::remove(fs::path(settings.root)/L"deep"/L"nested"/L"Artist - Limited.mp3");
    assert(QueryClassicTrackStatuses({q},nullptr,control).at(0).status=="DONE");
    settings.root=(base/L"unavailable").wstring();assert(status(q)=="WARN");
    // Deterministic work-count benchmark: a repeated 128-track miss batch must
    // reuse all expressions and per-index matches, independent of CPU speed.
    settings.root=(base/L"benchmark").wstring();settings.output_ext=L"mp3";
    for(unsigned i=0;i<32768;++i)write(fs::path(settings.root)/(L"Artist - Library "+std::to_wstring(i)+L".mp3"));
    std::vector<ClassicTrackQuery> batch;
    for(unsigned i=0;i<128;++i){auto item=query(L"Missing "+std::to_wstring(i));item.uri+=std::to_string(i);batch.push_back(item);}
    ClassicStatusMetrics cold;auto results=QueryClassicTrackStatuses(batch,&cold);
    assert(results.size()==batch.size());for(const auto& result:results)assert(result.status.empty());
    QueryClassicTrackStatuses(batch,&warm);
    assert(cold.scans==1&&cold.compilations==384&&cold.regex_checks==0);
    assert(warm.scans==0&&warm.compilations==0&&warm.regex_checks==0&&warm.cache_hits==128);
    std::printf("STATUS BENCHMARK: files=32768 queries=128 cold_ms=%llu scan_ms=%llu compile_ms=%llu match_ms=%llu regex=%llu warm_ms=%llu warm_regex=%llu\n",
        (unsigned long long)cold.elapsed_ms,(unsigned long long)cold.scan_ms,(unsigned long long)cold.compile_ms,
        (unsigned long long)cold.match_ms,(unsigned long long)cold.regex_checks,(unsigned long long)warm.elapsed_ms,(unsigned long long)warm.regex_checks);
    QueryClassicTrackStatuses(batch,&warm,control);
    assert(warm.scans==1&&warm.cache_hits==128&&warm.regex_checks==0); // Identical rescan keeps cached matches.
    const auto added=fs::path(settings.root)/L"Artist - Missing 0.mp3";write(added);
    assert(QueryClassicTrackStatuses({batch.front()},&warm,control).at(0).status=="DONE");
    assert(warm.cache_hits==0&&warm.regex_checks>0);
    fs::remove(added);assert(QueryClassicTrackStatuses({batch.front()},nullptr,control).at(0).status.empty());
    size_t checks=0;control.canceled=[&]{return ++checks>20;};
    assert(QueryClassicTrackStatuses(batch,nullptr,control).empty()); // Mid-scan cancellation.
    control.canceled={};control.timeout_ms=0;assert(QueryClassicTrackStatuses(batch,nullptr,control).empty());
    control.timeout_ms=7000;assert(QueryClassicTrackStatuses(batch,nullptr,control).size()==128);
    // Cancel in matching against a fresh cached index, then recover without
    // accepting a partially computed negative cache entry.
    auto canceledQuery=query(L"Cancel missing");checks=0;control.index_lifetime_ms=10000;
    control.canceled=[&]{return ++checks>100;};
    assert(QueryClassicTrackStatuses({canceledQuery},nullptr,control).empty());
    control.canceled={};assert(QueryClassicTrackStatuses({canceledQuery},&warm,control).size()==1);
    assert(warm.cache_hits==0);
    for(unsigned i=128;i<640;++i){auto item=query(L"Missing "+std::to_wstring(i));item.uri+=std::to_string(i);batch.push_back(item);}
    QueryClassicTrackStatuses(batch);
    QueryClassicTrackStatuses(batch,&warm);assert(warm.compilations==0&&warm.cache_hits==640);
    // The small regex LRU may evict, but stable results survive a 640-song pass.
    // Result storage itself still has a finite 4096-entry limit.
    for(unsigned i=640;i<4740;++i){auto item=query(L"Missing "+std::to_wstring(i));item.uri+=std::to_string(i);QueryClassicTrackStatuses({item});}
    QueryClassicTrackStatuses({batch.front()},&warm);assert(warm.compilations==3&&warm.cache_hits==0);
    fs::remove_all(base);
    std::puts("PASS: real-file status reconciliation, publication/settings invalidation, ambiguity, Unicode/multi-artist/legacy/podcast paths and cached work counts");
}
