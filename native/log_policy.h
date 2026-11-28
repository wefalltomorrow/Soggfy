#pragma once
#include <string_view>
namespace history {
inline bool ValidLogLevel(unsigned value) {return value==0||value==1||value==2||value==4;}
inline unsigned ReadLogLevel(std::wstring_view text,bool legacy_log,bool legacy_debug) {
    if(text.empty())return !legacy_log?0:legacy_debug?4:2;
    if(text.size()==1 && (text[0]==L'0'||text[0]==L'1'||text[0]==L'2'||text[0]==L'4'))return unsigned(text[0]-L'0');
    return 1;
}
inline unsigned ActivityLogLevel(std::string_view event) {
    return event=="finished"||event=="conversion_retry_done"||event=="warning"||event.find("failed")!=std::string_view::npos?1:2;
}
}
