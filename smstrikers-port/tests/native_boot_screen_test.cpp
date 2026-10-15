#include "port/native_boot_screen.hpp"
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <algorithm>
namespace {
void* memory=nullptr;
void* current=nullptr;void* next=nullptr;
unsigned queues=0,waits=0,frees=0;
int allocationFailure=0,baseFailure=0,displayFailure=0,queryFailure=0;
}
extern "C" {
SceUID sceKernelAllocMemBlock(const char*,SceKernelMemBlockType type,SceSize size,SceKernelAllocMemBlockOpt*) {
    assert(!memory&&type==SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW&&!(size%0x40000));
    if(allocationFailure)return -1;
    memory=std::calloc(1,size);assert(memory);return 99;
}
int sceKernelGetMemBlockBase(SceUID id,void** out){assert(id==99);*out=memory;return baseFailure;}
int sceKernelFreeMemBlock(SceUID id){assert(id==99&&memory);assert(current!=memory&&next!=memory);std::free(memory);memory=nullptr;++frees;return 0;}
int sceDisplaySetFrameBuf(const SceDisplayFrameBuf* fb,SceDisplaySetBufSync mode) {
    assert(mode==SCE_DISPLAY_SETBUF_NEXTFRAME);if(displayFailure)return -1;
    if(fb)assert(fb->width==960&&fb->height==544&&fb->pitch==1024&&fb->base==memory);
    next=fb?fb->base:nullptr;++queues;return 0;
}
int sceDisplayGetFrameBuf(SceDisplayFrameBuf* fb,SceDisplaySetBufSync mode) {
    fb->base=mode==SCE_DISPLAY_SETBUF_IMMEDIATE?current:next;return queryFailure;
}
int sceDisplayWaitVblankStart(){current=next;++waits;return 0;}
}
int main() {
    using namespace port::startup;
    allocationFailure=-1;assert(!begin()&&!memory);allocationFailure=0;
    baseFailure=-1;assert(!begin()&&!memory&&frees==1);baseFailure=0;
    displayFailure=-1;assert(!begin()&&!memory&&frees==2);displayFailure=0;
    assert(begin()&&memory&&current==memory&&waits==1);
    const auto* pixels=static_cast<const uint32_t*>(memory);
    assert(std::count(pixels,pixels+1024*544,0xffffffff)>500);
    show("PREPARING SHADERS",~0u,41); // clamp and wide multiply
    const auto color=pixels[346*1024+715];assert(color==0xffe0e0e0);
    // A queued game frame does not make the current scanout safe to free.
    next=reinterpret_cast<void*>(uintptr_t(1));assert(!retire_if_replaced()&&memory);
    const auto before=queues;show("LATE UPDATE",1,1);assert(queues==before);
    current=next;queryFailure=-1;assert(!retire_if_replaced()&&memory);queryFailure=0;
    assert(retire_if_replaced()&&!memory&&frees==3);
    shutdown();assert(frees==3&&!begin());
    std::puts("Startup display: early image, allocation/display errors, bounded progress and scanout lifetime passed");
}
