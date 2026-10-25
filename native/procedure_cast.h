#pragma once
#include <cstring>
#include <type_traits>

namespace history {
// Win32 exports use a generic function pointer. Copy its representation without
// an incompatible-function-type cast; a mismatched ABI size fails at compile time.
template<class Target,class Source> Target ProcedureCast(Source source) {
    static_assert(std::is_pointer<Target>::value&&std::is_pointer<Source>::value,"Expected procedure pointers");
    static_assert(sizeof(Target)==sizeof(Source),"Procedure pointer size mismatch");
    Target target=nullptr;
    std::memcpy(&target,&source,sizeof(target));
    return target;
}
}
