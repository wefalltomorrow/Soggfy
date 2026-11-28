#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace history {
void StartLogger();
void LogActivity(const char* event,const std::string& detail);
void QueueDiagnostic(const char* message);
void QueueStreamDump(uint64_t stream_id,uint32_t epoch,bool flac,const char* event,
                     const char* state,unsigned status,const void* bytes,
                     size_t length,uint64_t input_length);
bool WriteCappedLog(const std::wstring& path,const std::string& line);
}
