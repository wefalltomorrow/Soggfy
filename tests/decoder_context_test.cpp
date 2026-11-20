#include "../native/decoder_context.h"
#include <cassert>
#include <thread>
#include <cstdio>
history::DecoderContext ReadOtherTranslationUnit();
int main(){
    using namespace history;
    assert(!ReadOtherTranslationUnit().active);
    {
        DecoderScope outer(123,7);
        assert(ReadOtherTranslationUnit().owner==123&&ReadOtherTranslationUnit().epoch==7);
        try {DecoderScope inner(456,8);assert(ReadOtherTranslationUnit().owner==456);throw 1;}catch(int){}
        assert(ReadOtherTranslationUnit().owner==123&&ReadOtherTranslationUnit().epoch==7);
        std::thread other([]{
            assert(!ReadOtherTranslationUnit().active);
            DecoderScope scope(789,9);assert(ReadOtherTranslationUnit().owner==789);
        });other.join();
        assert(ReadOtherTranslationUnit().owner==123);
    }
    assert(!ReadOtherTranslationUnit().active);
    CaptureOwner owner;
    assert(owner.Accept(0)&&!owner.owner);
    assert(owner.Accept(123)&&owner.Accept(123)&&owner.Accept(0));
    assert(!owner.Accept(456)&&owner.owner==123);
    puts("PASS: cross-translation-unit decoder scope, nesting, exception restore, thread isolation and owner conflicts");
}
