#include "../native/library_layout.h"
#include <cstdio>
#include <cstdlib>
using namespace history;
static void check(bool b,const char* n) { if(!b) { std::fprintf(stderr,"FAIL: %s\n",n); std::exit(1); } }
int main() {
    Catalog c; c.artist=L"Test/Artist"; c.album=L"Test Album"; c.title=L"Test Track"; c.track=1;
    check(RelativePath(c,L".ogg")==L"Music\\Artists\\Test-Artist\\Test Album\\01 - Test Track.ogg",
          "artist album layout without year or filename timestamp");
    check(OutputPath(L"C:\\Users\\TestUser\\Music",c,L".ogg",true)==L"C:\\Users\\TestUser\\Music\\Artists\\Test-Artist\\Test Album\\01 - Test Track.ogg","default Music folder avoids duplicate Music directory");
    check(OutputPath(L"D:\\Media\\",c,L".ogg")==L"D:\\Media\\Music\\Artists\\Test-Artist\\Test Album\\01 - Test Track.ogg","custom Media root retains hierarchy");
    c.kind=MediaKind::Compilation;
    check(RelativePath(c,L".ogg")==L"Music\\Compilations\\Test Album\\01 - Test Track.ogg","compilation routing");
    c.kind=MediaKind::Soundtrack;
    check(RelativePath(c,L".ogg").find(L"Music\\Soundtracks\\")==0,"soundtrack routing");
    c.kind=MediaKind::VariousArtists;
    check(RelativePath(c,L".ogg").find(L"Music\\Various Artists\\")==0,"various artist routing");
    c.kind=MediaKind::Audiobook; c.author=L"Author"; c.series=L"Series"; c.book=L"Book";
    check(RelativePath(c,L".flac")==L"Audiobooks\\Author\\Series\\Book\\01 - Test Track.flac","book series routing");
    c.kind=MediaKind::Podcast; c.show=L"Podcast Name"; c.episode_year=2026; c.track=0;
    check(RelativePath(c,L".ogg")==L"Podcasts\\Podcast Name\\2026\\Test Track.ogg","podcast year routing");
    c.kind=MediaKind::MusicVideo;
    check(RelativePath(c,L".ogg")==L"Music Videos\\Test Track.ogg","music video routing");
    c.kind=MediaKind::Concert;
    check(RelativePath(c,L".ogg")==L"Concerts\\Test Track.ogg","concert routing");
    c.kind=MediaKind::Music; c.artist=L".."; c.album=L"CON"; c.title=L"../bad:name";
    auto path=RelativePath(c,L".ogg");
    check(path.find(L"..\\")==std::wstring::npos && path.find(L":")==std::wstring::npos && path.find(L"\\CON\\")==std::wstring::npos,
          "path traversal and reserved Windows names prevented");

    Catalog t; t.artist=L"Album Artist"; t.album_artist=L"Album Artist"; t.all_artists=L"One / Two";
    t.album=L"Album: Deluxe"; t.title=L"Track/Name"; t.track=3; t.disc=2; t.total_discs=2; t.release_year=2026; t.release_date=L"2026-10-02";
    auto templated=RelativePathTemplate(t,L".flac",L"{artist_name}\\{release_year} - {album_name}{multi_disc_path}\\{track_num_2} - {track_name}.{ext}");
    check(templated==L"Album Artist\\2026 - Album： Deluxe\\CD 2\\03 - Track／Name.flac",
          "Soggfy style template renders safe Unicode-preserving paths");
    check(RelativePathTemplate(t,L".ogg",L"{release_date} - {track_name}.{ext}")==L"2026-10-02 - Track／Name.ogg","release date template token");
    t.all_artists=L"AC/DC / Guest";
    auto artists=RelativePathTemplate(t,L".ogg",L"{all_artist_names} - {track_name}.{ext}",true);
    check(artists==L"AC／DC, Guest - Track／Name.ogg","artist separator normalization preserves AC/DC");
    check(RelativePathTemplate(t,L".ogg",L"..\\{track_name}",true).empty(),"template traversal rejected");
    Catalog issue150; issue150.artist=L"AC/DC"; issue150.album_artist=L"AC/DC"; issue150.album=L"Back in Black";
    issue150.title=L"Hells Bells"; issue150.track=1;
    check(RelativePathTemplate(issue150,L".ogg",L"{artist_name}\\{album_name}\\{track_num}. {track_name}.{ext}",true,L"unicode")==
          L"AC／DC\\Back in Black\\1. Hells Bells.ogg","issue 150 AC/DC path uses the same safe slash");
    issue150.artist=L"Gary Numan / Tubeway Army"; issue150.album_artist=issue150.artist;
    check(RelativePathTemplate(issue150,L".ogg",L"{artist_name}\\{track_name}.{ext}",true,L"unicode")==
          L"Gary Numan, Tubeway Army\\Hells Bells.ogg","spaced artist separator normalization is path-only");
    check(RelativePathTemplate(issue150,L".ogg",L"{artist_name}\\{track_name}.{ext}",false,L"-")==
          L"Gary Numan - Tubeway Army\\Hells Bells.ogg","legacy dash replacement mode");

    check(OutputPath(L"D:\\Music",t,L".ogg",false,L"{artist_name}\\{track_name}",true)==
          L"D:\\Music\\Album Artist\\Track／Name.ogg","template appends real capture extension");

    Quality high{Codec::Vorbis,44100,2,0,320000},low{Codec::Vorbis,44100,2,0,160000};
    check(HigherQuality(high,low),"higher Vorbis bitrate upgrades");
    check(!HigherQuality(low,high) && !HigherQuality(high,high),"equal or better existing Vorbis skipped");
    Quality flac{Codec::Flac,44100,2,16,0},hires{Codec::Flac,48000,2,24,0};
    check(HigherQuality(flac,high) && !HigherQuality(high,flac),"lossless existing file prevents Vorbis downgrade");
    check(HigherQuality(hires,flac) && !HigherQuality(flac,hires),"FLAC resolution upgrades");
    check(!HigherQuality(Quality{Codec::Vorbis,44100,2,0,0},high),"unknown encoding quality cannot replace file");
    std::puts("PASS: smart layout, templates, Windows-safe names and conservative quality comparison");
}
