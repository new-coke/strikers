#if defined(STRIKERS_VITA_NATIVE_RUNTIME) || defined(STRIKERS_BOOT_SCREEN_TEST)
#include "port/native_boot_screen.hpp"
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <pthread.h>
namespace port::startup {
namespace {
constexpr unsigned Width=960, Height=544, Pitch=1024;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
struct DisplayLock {
    DisplayLock(){pthread_mutex_lock(&mutex);}
    ~DisplayLock(){pthread_mutex_unlock(&mutex);}
};
SceUID block=-1;
uint32_t* pixels=nullptr;
bool handedOff=false;
unsigned lastPercent=~0u;
char lastStatus[64]{};
static const unsigned char kBootFont[26][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // A
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // B
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // C
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, // D
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // E
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // F
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // G
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // H
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // I
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // J
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // K
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // L
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // M
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, // N
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // O
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // P
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // Q
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // R
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, // S
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // T
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // U
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // V
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, // W
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // X
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // Y
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // Z
};


void release() {
    if(block>=0)sceKernelFreeMemBlock(block);
    block=-1;pixels=nullptr;
}
void rect(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t color) {
    if(!pixels||x>=Width||y>=Height)return;
    w=std::min(w,Width-x);h=std::min(h,Height-y);
    for(unsigned row=0;row<h;++row)std::fill_n(pixels+(y+row)*Pitch+x,w,color);
}
void text(const char* s,unsigned y,unsigned scale,uint32_t color) {
    const unsigned length=std::min<size_t>(std::strlen(s),Width/(6*scale));
    unsigned x=(Width-length*6*scale)/2;
    for(unsigned n=0;n<length;++n,x+=6*scale) {
        const char c=s[n]>='a'&&s[n]<='z'?s[n]-'a'+'A':s[n];
        if(c<'A'||c>'Z')continue;
        for(unsigned r=0;r<7;++r)for(unsigned col=0;col<5;++col)
            if(kBootFont[c-'A'][r]&(1u<<(4-col)))rect(x+col*scale,y+r*scale,scale,scale,color);
    }
}
bool queue() {
    SceDisplayFrameBuf fb{};fb.size=sizeof fb;fb.base=pixels;fb.pitch=Pitch;
    fb.width=Width;fb.height=Height;fb.pixelformat=SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    return sceDisplaySetFrameBuf(&fb,SCE_DISPLAY_SETBUF_NEXTFRAME)>=0;
}
void draw(const char* status,unsigned current,unsigned total) {
    const unsigned percent=total?unsigned(uint64_t(std::min(current,total))*100/total):0;
    if(!std::strcmp(lastStatus,status)&&lastPercent==percent)return;
    std::strncpy(lastStatus,status,sizeof lastStatus-1);lastStatus[sizeof lastStatus-1]=0;lastPercent=percent;
    rect(0,0,Width,Height,0xff101010);rect(120,152,720,240,0xff202020);
    text("STRIKERS",180,4,0xffe0e0e0);text(lastStatus,246,3,0xffffffff);
    text("PLEASE WAIT",294,2,0xffb0b0b0);
    if(total){rect(240,342,480,12,0xff505050);rect(242,344,476*percent/100,8,0xffe0e0e0);}
}
}
bool begin() noexcept {
    DisplayLock lock;
    if(pixels)return true;
    if(handedOff)return false;
    constexpr unsigned bytes=(Pitch*Height*4+0x3ffffu)&~0x3ffffu;
    block=sceKernelAllocMemBlock("strikers_startup",SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,bytes,nullptr);
    if(block<0)return false;
    void* base=nullptr;
    if(sceKernelGetMemBlockBase(block,&base)<0||!base){release();return false;}
    pixels=static_cast<uint32_t*>(base);lastPercent=~0u;lastStatus[0]=0;
    draw("PREPARING GAME",0,0);
    if(!queue()){release();return false;}
    sceDisplayWaitVblankStart();
    return true;
}
void show(const char* status,unsigned current,unsigned total) noexcept {
    DisplayLock lock;
    if(!pixels||handedOff||!status)return;
    draw(status,current,total);
    // Initialization may reconfigure scanout. Reassert this image until the
    // first game swap; never publish a boot frame after the handoff begins.
    queue();
}
bool retire_if_replaced() noexcept {
    DisplayLock lock;handedOff=true;
    if(!pixels)return true;
    for(auto mode:{SCE_DISPLAY_SETBUF_IMMEDIATE,SCE_DISPLAY_SETBUF_NEXTFRAME}){
        SceDisplayFrameBuf fb{};fb.size=sizeof fb;
        if(sceDisplayGetFrameBuf(&fb,mode)<0||fb.base==pixels)return false;
    }
    release();return true;
}
void shutdown() noexcept {
    DisplayLock lock;handedOff=true;
    if(!pixels)return;
    bool ours=false;
    for(auto mode:{SCE_DISPLAY_SETBUF_IMMEDIATE,SCE_DISPLAY_SETBUF_NEXTFRAME}) {
        SceDisplayFrameBuf fb{};fb.size=sizeof fb;
        if(sceDisplayGetFrameBuf(&fb,mode)<0)return; // Keep storage if ownership is unknown.
        ours|=fb.base==pixels;
    }
    if(ours) {
        if(sceDisplaySetFrameBuf(nullptr,SCE_DISPLAY_SETBUF_NEXTFRAME)<0)return;
        sceDisplayWaitVblankStart();
    }
    release();
}
}
#endif
