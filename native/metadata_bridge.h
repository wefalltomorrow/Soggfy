#pragma once
#include <windows.h>
#include "media_session.h"
#include "ogg_tags.h"
#include "library_layout.h"
namespace history {
void StartMetadataCollector(HMODULE cef);
void EnrichTags(const Media&,Tags&);
void EnrichCatalog(const Media&,Catalog&);
std::string ReadClientPlaybackQuality(const Media&);
}
