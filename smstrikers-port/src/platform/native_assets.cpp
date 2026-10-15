#if defined(PORT_VITA) && (defined(PORT_USE_AURORA) || defined(STRIKERS_VITA_NATIVE_RUNTIME))
#include "port/native_assets.hpp"
#include "port/asset_archive.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
namespace port {
namespace {PortAssetArchive* archive=nullptr;}
void shutdown_native_assets() noexcept {if(archive)port_asset_archive_close(archive);archive=nullptr;}
void initialize_native_assets() noexcept {
  shutdown_native_assets();
  const char* enabled=std::getenv("STRIKERS_NATIVE_ASSETS");
  if(!enabled)enabled=std::getenv("STRIKERS_GXM_NATIVE_ASSETS");
  const char* path=std::getenv("STRIKERS_NATIVE_ASSET_ARCHIVE");
  if(!path||!*path)path=std::getenv("STRIKERS_ASSET_ARCHIVE");
  if(!enabled||std::strcmp(enabled,"1")||!path||!*path)return;
  char error[256];archive=port_asset_archive_open(path,error,sizeof error);
  if(!archive)std::fprintf(stderr,"[native-assets] archive unavailable; native texture decoder: %s\n",error);
}
bool read_native_asset(const char* path,std::vector<uint8_t>& bytes,void*) noexcept {
  if(!archive)return false;
  const int entry=port_asset_archive_find(archive,path);if(entry<0)return false;
  const uint64_t size=port_asset_archive_size(archive,entry);
  if(!size||size>32u*1024u*1024u)return false;
  bytes.resize(size);
  if(port_asset_archive_read(archive,entry,bytes.data(),bytes.size(),0)!=long(bytes.size())){bytes.clear();return false;}
  return true;
}
} // namespace port
#endif
