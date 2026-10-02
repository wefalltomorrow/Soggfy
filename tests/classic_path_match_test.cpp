#include "../native/classic_path_match.h"
#include "../native/library_layout.h"
#include <cstdio>
#include <cstdlib>
using namespace history;
static void check(bool ok,const char* name){if(!ok){std::fprintf(stderr,"FAIL: %s\\n",name);std::exit(1);}}
int main(){
    const std::wstring templ=L"{artist_name}\\\\{album_name}\\\\{track_num}. {track_name}.{ext}";

    Catalog ac;ac.artist=L"AC/DC";ac.album_artist=L"AC/DC";ac.album=L"Back in Black";ac.title=L"Hells Bells";ac.track=1;
    auto ac_path=RelativePathTemplate(ac,L".ogg",templ,true,L"unicode");
    ClassicPathQuery ac_q{ac.title,L"AC/DC",ac.album,L"AC/DC"};
    check(ac_path==L"AC／DC\\\\Back in Black\\\\1. Hells Bells.ogg","AC/DC output path");
    check(ClassicPathMatches(ac_path,ac_q,templ,L"",true,L"unicode"),
          "issue 150 AC/DC downloaded-track lookup");

    Catalog g;g.artist=L"Gary Numan / Tubeway Army";g.album_artist=g.artist;g.album=L"Replicas";g.title=L"Down in the Park";g.track=4;
    auto g_path=RelativePathTemplate(g,L".ogg",templ,true,L"unicode");
    ClassicPathQuery g_q{g.title,L"Gary Numan / Tubeway Army",g.album,L"Gary Numan / Tubeway Army"};
    check(g_path==L"Gary Numan, Tubeway Army\\\\Replicas\\\\4. Down in the Park.ogg",
          "spaced slash normalized output path");
    check(ClassicPathMatches(g_path,g_q,templ,L"",true,L"unicode"),
          "issue 150 spaced-slash downloaded-track lookup");

    auto dash_path=RelativePathTemplate(g,L".ogg",templ,false,L"-");
    check(dash_path==L"Gary Numan - Tubeway Army\\\\Replicas\\\\4. Down in the Park.ogg",
          "dash invalid-character mode output");
    check(ClassicPathMatches(dash_path,g_q,templ,L"",false,L"-"),
          "issue 150 dash-mode downloaded-track lookup");

    auto mp3=RelativePathTemplate(ac,L".mp3",templ,true,L"unicode");
    check(ClassicPathMatches(mp3,ac_q,templ,L"mp3",true,L"unicode"),
          "converted MP3 downloaded-track lookup");
    check(ClassicPathMatches(mp3,ac_q,templ,L"",true,L"unicode",true),
          "existing MP3 matches while current output is native");

    ClassicPathQuery techno{L"Pump Up The Jam",L"Technotronic",L"Pump Up The Jam",L"Technotronic"};
    check(ClassicLegacyFlatPathMatches(L"Technotronic - Pump Up The Jam.mp3",techno),
          "legacy flat artist-title MP3");
    check(ClassicLegacyFlatPathMatches(L"Old Downloads\\\\Technotronic - Pump Up The Jam.mp3",techno),
          "legacy flat file inside a subfolder");
    check(!ClassicLegacyFlatPathMatches(L"Other Artist - Pump Up The Jam.mp3",techno),
          "legacy flat lookup requires artist identity");

    ClassicPathQuery multi{
        L"Everyday",
        L"A$AP Rocky",
        L"AT.LONG.LAST.A$AP",
        L"A$AP Rocky, Rod Stewart, Miguel, Mark Ronson"
    };
    check(ClassicLegacyFlatPathMatches(
              L"A$AP Rocky, Rod Stewart, Miguel, Mark Ronson - Everyday.mp3",multi),
          "legacy flat all-artists MP3");

    check(ClassicLegacyFlatPathMatches(L"AC／DC - Hells Bells.mp3",ac_q),
          "legacy flat unicode slash escaping");
    check(ClassicLegacyFlatPathMatches(L"AC, DC - Hells Bells.mp3",ac_q),
          "legacy old all-artists slash normalization");
    check(!ClassicLegacyFlatPathMatches(L"Hells Bells.mp3",ac_q),
          "legacy fallback does not use unsafe title-only matching");
    check(!ClassicLegacyFlatPathMatches(L"AC／DC - Hells Bells.txt",ac_q),
          "legacy fallback ignores non-audio files");

    std::puts("PASS: classic and legacy Soggfy downloaded-track matching");
}
