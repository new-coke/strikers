#include "port/native_audio.hpp"
#include "port/native_audio_record.hpp"
#include "port/asset_archive.h"
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <cstdlib>
namespace {
std::vector<uint8_t> indexBytes,recordBytes;
std::array<uint8_t,16> source{0,1,2,3,4,5,6,7,0,8,9,10,11,12,13,14};
std::array<int16_t,16> coefficients{};
std::thread::id caller;
std::mutex gateMutex;std::condition_variable gate;
bool unblock=false,corrupt=false;
std::atomic<unsigned> recordReads{0},closes{0};
void le32(std::vector<uint8_t>& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(n>>(i*8));}
void le16(std::vector<uint8_t>& b,int16_t n){b.push_back(uint16_t(n)&255);b.push_back(uint16_t(n)>>8);}
void setup() {
    recordBytes={'S','T','P','C','M','0','0','1'};le32(recordBytes,16);le32(recordBytes,28);
    for(auto c:coefficients)le16(recordBytes,c);le16(recordBytes,3);le16(recordBytes,4);
    recordBytes.insert(recordBytes.end(),source.begin(),source.end());
    le16(recordBytes,3);le16(recordBytes,4);for(unsigned i=0;i<14;++i)le16(recordBytes,100+i);
    le16(recordBytes,113);le16(recordBytes,112);for(unsigned i=0;i<14;++i)le16(recordBytes,200+i);
    const auto hash=port::audio_hash(recordBytes.data(),recordBytes.size());for(unsigned i=0;i<8;++i)recordBytes.push_back(hash>>(i*8));
    indexBytes={'S','T','A','I','D','X','0','1'};le32(indexBytes,1);le32(indexBytes,1);
    const auto key=port::audio_frame_key(source.data(),coefficients.data(),3,4);le32(indexBytes,key);le32(indexBytes,key>>32);le32(indexBytes,recordBytes.size());indexBytes.resize(indexBytes.size()+32);
}
void waitRead() {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!recordReads&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();assert(recordReads==1);
}
void waitResult(bool bad) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(std::chrono::steady_clock::now()<deadline){const auto s=PortNativeAudioGetStats();if(bad?s.rejected:s.loads)return;std::this_thread::yield();}
    assert(false);
}
}
extern "C" {
PortAssetArchive* port_asset_archive_open(const char*,char*,size_t){return reinterpret_cast<PortAssetArchive*>(uintptr_t(1));}
void port_asset_archive_close(PortAssetArchive*){++closes;}
int port_asset_archive_find(const PortAssetArchive*,const char* path){return !std::strcmp(path,"native/v1/audio-index.bin")?0:1;}
uint64_t port_asset_archive_size(const PortAssetArchive*,unsigned n){return n?recordBytes.size():indexBytes.size();}
long port_asset_archive_read(PortAssetArchive*,unsigned n,void* out,size_t bytes,uint64_t at) {
    assert(!at);
    if(n){assert(std::this_thread::get_id()!=caller);++recordReads;std::unique_lock lock(gateMutex);gate.wait(lock,[]{return unblock;});}
    const auto& b=n?recordBytes:indexBytes;assert(bytes==b.size());std::memcpy(out,b.data(),bytes);
    if(n&&corrupt)static_cast<uint8_t*>(out)[60]^=1;
    return bytes;
}
int PortDiagnosticsEnabled(){return 0;}
unsigned long long port_monotonic_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
int main() {
    caller=std::this_thread::get_id();setup();setenv("STRIKERS_ASSET_ARCHIVE","fixture.psarc",1);
    std::array<int16_t,14> pcm;pcm.fill(-1);
    PortNativeAudioInitialize();assert(!PortNativeAudioBlock(source.data(),coefficients.data(),3,4,pcm.data())&&recordReads==0);
    setenv("STRIKERS_VITA_NATIVE_AUDIO","1",1);PortNativeAudioInitialize();
    assert(!PortNativeAudioBlock(source.data(),coefficients.data(),3,4,pcm.data()));waitRead();
    // The archive reader is deliberately blocked. The caller still returns
    // immediately, queues no duplicate read and leaves its output untouched.
    for(unsigned i=0;i<1000;++i)assert(!PortNativeAudioBlock(source.data(),coefficients.data(),3,4,pcm.data()));
    assert(pcm[0]==-1&&recordReads==1);
    {std::lock_guard lock(gateMutex);unblock=true;}gate.notify_all();waitResult(false);
    assert(PortNativeAudioBlock(source.data()+8,coefficients.data(),113,112,pcm.data())&&pcm[0]==200&&pcm[13]==213);
    const auto copy=source;assert(PortNativeAudioBlock(copy.data(),coefficients.data(),3,4,pcm.data())&&pcm[13]==113&&recordReads==1);
    pcm.fill(-1);assert(!PortNativeAudioBlock(source.data()+8,coefficients.data(),112,112,pcm.data())&&pcm[0]==-1);
    const auto s=PortNativeAudioGetStats();assert(s.loads==1&&s.hits==2&&s.cacheBytes==recordBytes.size());
    PortNativeAudioShutdown();assert(closes==1&&!PortNativeAudioBlock(source.data(),coefficients.data(),3,4,pcm.data()));
    corrupt=true;recordReads=0;PortNativeAudioInitialize();assert(!PortNativeAudioBlock(source.data(),coefficients.data(),3,4,pcm.data()));waitResult(true);
    assert(!PortNativeAudioGetStats().loads);PortNativeAudioShutdown();assert(closes==2);
    std::puts("Native PCM cache: blocked worker I/O never blocks mixer; late binding, deduplication, exact history, rejection and shutdown passed");
}
