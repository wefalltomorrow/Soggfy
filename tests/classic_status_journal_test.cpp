#include "../native/classic_status_journal.h"
#include <cassert>
#include <string>
using namespace history;
int main(){
    StatusJournalRow src{"Wonderwall","Oasis","(What's The Story) Morning Glory?","DONE",
        "C:\\Users\\Shimon\\Music\\Spotify\\Oasis - Wonderwall.mp3",""};
    const auto line=EncodeStatusJournalRow(src);
    assert(!line.empty()&&line.back()=='\n');
    StatusJournalRow dst{};
    assert(DecodeStatusJournalRow(std::string_view(line).substr(0,line.size()-1),dst));
    assert(dst.title==src.title&&dst.path==src.path&&dst.status=="DONE");
    const StatusJournalRow err{"Talk Like That","The Presets","Apocalypso","ERROR","",
        "Canceled: Invalid Vorbis tags\nretry pending\t✓"};
    auto line2=EncodeStatusJournalRow(err);
    assert(DecodeStatusJournalRow(std::string_view(line2).substr(0,line2.size()-1),dst));
    assert(dst.message==err.message);
    auto tampered=line2;tampered[tampered.find("5461")]='0';
    assert(!DecodeStatusJournalRow(std::string_view(tampered).substr(0,tampered.size()-1),dst));
    assert(!DecodeStatusJournalRow(std::string_view(line2).substr(0,20),dst));
    assert(EncodeStatusJournalRow({"x","y","z","IN_PROGRESS","",""}).empty());
    assert(EncodeStatusJournalRow({"","","","DONE","",""}).empty());
}
