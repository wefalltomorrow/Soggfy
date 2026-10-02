#pragma once
#include <string_view>
namespace history {
bool ShouldBlockClassicTelemetryUrl(std::wstring_view url);
}
