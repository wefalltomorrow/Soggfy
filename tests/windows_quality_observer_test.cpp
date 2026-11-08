// Exercise real hook event handling with downloads off: no complete compressed
// buffers are retained, and original parser return values remain unchanged.
#include "../native/audio_history.cpp"
#include "../native/history_settings.cpp"
#include <cassert>
namespace history {
std::string ReadClientPlaybackQuality(const Media&) {return {};}
void EnrichTags(const Media &, Tags &) { assert(false && "observer test must not publish audio"); }
void EnrichCatalog(const Media &, Catalog &) { assert(false && "observer test must not publish audio"); }
// No Spotify decoder module is loaded by this observer-only fixture.
bool PlaybackSpeedSupported() { return false; }
void SetPlaybackSpeedProbeEnabled(bool) {}
DecodeProbeSnapshot ReadPlaybackSpeedProbe() { return {}; }
} // namespace history
static int parser_calls = 0;
static bool switch_epoch=false;
static int32_t Parser(void *, OggPage *) {
  ++parser_calls;
  if(switch_epoch){assert(SetDownloads(false));assert(SetDownloads(true));}
  return 58;
}
static int FlacReader(void*,unsigned char*,size_t*) {
  if(switch_epoch){assert(SetDownloads(false));assert(SetDownloads(true));}
  return 0;
}
static int FlacFramer(void*,const void*,const void*) {
  if(switch_epoch){assert(SetDownloads(false));assert(SetDownloads(true));}
  return 0;
}
static void FlacErrorCallback(void*,unsigned) {
  if(switch_epoch){assert(SetDownloads(false));assert(SetDownloads(true));}
}
static void* FlacInitializer(void*,void* result) {return result;}
int main() {
  assert(!GetSettings().downloads);
  queue =
      static_cast<Slot *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Slot) * capacity));
  assert(queue);
  event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  assert(event);
  quality_enabled = true;
  std::vector<uint8_t> data(70000);
  FlacEvent(reinterpret_cast<void *>(2), BeginEvent(true), 3, data.data(), data.size());
  Slot first, second;
  assert(Pop(first) && Pop(second) && !Pop(first));
  assert(!first.capture && !second.capture && first.length == 42 && second.length == 42);
  assert(first.input_length + second.input_length == 70000);
  unsigned char header[28] = {}, body[30] = {};
  OggPage page{header, 28, body, 30};
  original = Parser;
  assert(Hook(reinterpret_cast<void *>(1), &page) == 58 && parser_calls == 1);
  Slot observed;
  assert(Pop(observed) && observed.kind == 0 && !observed.capture && observed.length == 58);
  quality_enabled = false;
  FlacEvent(reinterpret_cast<void *>(2), BeginEvent(true), 3, data.data(), data.size());
  assert(Hook(reinterpret_cast<void *>(1), &page) == 58 && parser_calls == 2 && !Pop(observed));
  // Real setters, no settings worker or disk writes. Reproduce a toggle while
  // original decoding is in flight; old audio must not acquire the new epoch.
  history::settings_signal=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  assert(history::settings_signal&&SetDownloads(true));
  switch_epoch=true;
  assert(Hook(reinterpret_cast<void*>(1),&page)==58&&!Pop(observed));
  original_flac_read=FlacReader;size_t length=32;
  assert(FlacReadHook(reinterpret_cast<void*>(2),data.data(),&length)==0&&!Pop(observed));
  original_flac_frame=FlacFramer;
  assert(FlacFrameHook(reinterpret_cast<void*>(2),data.data(),nullptr)==0&&!Pop(observed));
  original_flac_error=FlacErrorCallback;
  FlacErrorHook(reinterpret_cast<void*>(2),9);assert(!Pop(observed));
  switch_epoch=false;
  {
    DecoderScope scope(123,CaptureEpoch());
    assert(Hook(reinterpret_cast<void*>(1),&page)==58&&Pop(observed));
    assert(observed.owner==123&&observed.capture&&observed.epoch==CaptureEpoch());
    original_flac_init=FlacInitializer;
    assert(FlacInitHook(reinterpret_cast<void*>(2),data.data())==data.data());
    assert(Pop(observed)&&observed.kind==2&&observed.owner==123);
    FlacErrorHook(reinterpret_cast<void*>(2),9);
    assert(Pop(observed)&&observed.kind==5&&observed.owner==123&&observed.format==9);
    assert(SetDownloads(false)&&SetDownloads(true));
    assert(Hook(reinterpret_cast<void*>(1),&page)==58&&!Pop(observed));
  }
  assert(Hook(reinterpret_cast<void*>(1),&page)==58&&Pop(observed)&&!observed.owner);
  const auto before_loss=BeginEvent(true);
  InterlockedIncrement(&dropped);
  FlacEvent(reinterpret_cast<void*>(2),before_loss,3,data.data(),32);
  assert(!Pop(observed));
  FlacEvent(reinterpret_cast<void*>(2),BeginEvent(true),3,data.data(),32);
  assert(Pop(observed)&&observed.loss==dropped);
  // Fill the actual ring, then overflow in the second chunk of one read.
  const auto before_overflow=BeginEvent(true);
  for(unsigned i=0;i<capacity-1;++i)
    FlacEvent(reinterpret_cast<void*>(2),before_overflow,2);
  FlacEvent(reinterpret_cast<void*>(2),before_overflow,3,data.data(),data.size());
  assert(count==capacity&&dropped==before_overflow.loss+1&&!EventCurrent(before_overflow));
  unsigned drained=0;
  while(Pop(observed)){assert(observed.loss==before_overflow.loss);++drained;}
  assert(drained==capacity);
  FlacEvent(reinterpret_cast<void*>(2),before_overflow,2);assert(!Pop(observed));
  assert(SetDownloads(false));CloseHandle(history::settings_signal);history::settings_signal=nullptr;
  CloseHandle(event);
  event = nullptr;
  HeapFree(GetProcessHeap(), 0, queue);
  queue = nullptr;
  std::puts("PASS: downloads-off observation, bounded FLAC prefix copy, complete byte accounting "
            "unchanged decoder returns, entry epochs, decoder context and queue-loss rejection");
}
