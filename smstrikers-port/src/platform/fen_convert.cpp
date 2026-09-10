// The .fen front-end package: a flat big-endian blob plus a table of the offsets holding pointers.
// A host address no longer fits its slot, so the graph is rebuilt into one arena.

// discover walks it from FEPackage tagging concrete types, emit converts field by field, and the
// root sits at arena offset 0 so FEScene still frees a package with one nlFree.

// 0xFFFFFFFF is the null pointer, not 0, so offset 0 is a legitimate target.

// FEAnimation's first four bytes are a stale authoring-tool vtable slot named by no relocation
// table; it is ignored and placement new supplies a real vptr.

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>

#include "port/endian.h"

#include "Game/FE/fePackage.h"
#include "Game/FE/fePresentation.h"
#include "Game/FE/tlSlide.h"
#include "Game/FE/tlInstance.h"
#include "Game/FE/tlImageInstance.h"
#include "Game/FE/tlTextInstance.h"
#include "Game/FE/tlComponentInstance.h"
#include "Game/FE/tlComponent.h"
#include "Game/FE/feLibObject.h"
#include "Game/FE/feImage.h"
#include "Game/FE/feText.h"
#include "Game/FE/feTextureResource.h"
#include "Game/FE/feFontResource.h"
#include "Game/FE/feAnimation.h"
#include "dolphin/os.h"
#include "NL/nlString.h"

// Declared, not included: NL/nlMemory.h redeclares operator new without an exception specification.
// size_t, not unsigned long, which is a different type on Windows.
void* nlMalloc(size_t size, unsigned int alignment, bool atEnd);

namespace
{

enum Kind
{
    // clang-format off
    K_PACKAGE, K_PRESENTATION, K_SLIDE,
    K_INSTANCE, K_IMAGE_INST, K_TEXT_INST, K_COMP_INST,
    K_LIBOBJ, K_FEIMAGE, K_FETEXT, K_TLCOMPONENT,
    K_TEXRES, K_FONTRES,
    K_ANIM, K_KF_F, K_KF_V3,
    // clang-format on
    K_COUNT
};

std::size_t hostSize(int k)
{
    switch (k)
    {
    case K_PACKAGE:      return sizeof(FEPackage);
    case K_PRESENTATION: return sizeof(FEPresentation);
    case K_SLIDE:        return sizeof(TLSlide);
    case K_INSTANCE:     return sizeof(TLInstance);
    case K_IMAGE_INST:   return sizeof(TLImageInstance);
    case K_TEXT_INST:    return sizeof(TLTextInstance);
    case K_COMP_INST:    return sizeof(TLComponentInstance);
    case K_LIBOBJ:       return sizeof(FELibObject);
    case K_FEIMAGE:      return sizeof(FEImage);
    case K_FETEXT:       return sizeof(FEText);
    case K_TLCOMPONENT:  return sizeof(TLComponent);
    case K_TEXRES:       return sizeof(FETextureResource);
    case K_FONTRES:      return sizeof(FEFontResource);
    case K_ANIM:         return sizeof(FEAnimation);
    case K_KF_F:         return sizeof(fAnimationKeyframe);
    default:             return sizeof(v3AnimationKeyframe);
    }
}

const u32 kNullOffset = 0xFFFFFFFFu;

struct Obj
{
    u32 src;   // offset in the file blob
    u32 dst;   // offset in the host arena
    int kind;
};

struct Ctx
{
    const u8* blob;
    u32 blobLen;
    const u32* table;   // relocation table, already in host order
    u32 tableCount;

    Obj* objs;
    u32 objCount;
    u32 objCap;

    s32* byWord;

    u8* isSlot;
    u32 mismatches;

    u8* arena;
    std::size_t arenaSize;
    bool failed;
};

u32 rd32(Ctx* c, u32 off) { return port_be32(c->blob + off); }
u16 rd16(Ctx* c, u32 off) { return port_be16(c->blob + off); }
float rdf32(Ctx* c, u32 off) { return port_bef32(c->blob + off); }

int kindOfInstance(Ctx* c, u32 off)
{
    switch (rd32(c, off + 0x78))
    {
    case TLAT_IMAGE:     return K_IMAGE_INST;
    case TLAT_TEXT:      return K_TEXT_INST;
    case TLAT_COMPONENT: return K_COMP_INST;
    default:             return K_INSTANCE;   // LAYER, GROUP
    }
}

int kindOfLibObj(Ctx* c, u32 off)
{
    switch (rd32(c, off + 0x64))
    {
    case FEOT_IMAGE:     return K_FEIMAGE;
    case FEOT_TEXT:      return K_FETEXT;
    case FEOT_COMPONENT: return K_TLCOMPONENT;
    default:             return K_LIBOBJ;   // LAYER, GROUP
    }
}

int kindOfResource(Ctx* c, u32 off)
{
    return rd32(c, off + 0x08) == FERT_FONT ? K_FONTRES : K_TEXRES;
}

s32 discover(Ctx* c, u32 srcSlot, u32 target, int kind)
{
    if (target == kNullOffset)
        return -1;

    if (srcSlot != kNullOffset && c->isSlot != nullptr && !c->isSlot[srcSlot >> 2])
        c->mismatches++;

    if (target >= c->blobLen)
    {
        c->failed = true;
        return -1;
    }

    s32 existing = c->byWord[target >> 2];
    if (existing >= 0)
        return existing;

    if (c->objCount == c->objCap)
    {
        u32 cap = c->objCap ? c->objCap * 2 : 256;
        Obj* grown = (Obj*)std::realloc(c->objs, cap * sizeof(Obj));
        if (grown == nullptr)
        {
            c->failed = true;
            return -1;
        }
        c->objs = grown;
        c->objCap = cap;
    }

    s32 index = (s32)c->objCount++;
    c->objs[index].src = target;
    c->objs[index].dst = 0;
    c->objs[index].kind = kind;
    c->byWord[target >> 2] = index;
    return index;
}

void edge(Ctx* c, u32 base, u32 fieldOff, int kind)
{
    u32 slot = base + fieldOff;
    discover(c, slot, rd32(c, slot), kind);
}

void edgeInstance(Ctx* c, u32 base, u32 fieldOff)
{
    u32 slot = base + fieldOff;
    u32 target = rd32(c, slot);
    discover(c, slot, target, target == kNullOffset ? K_INSTANCE : kindOfInstance(c, target));
}

void edgeLibObj(Ctx* c, u32 base, u32 fieldOff)
{
    u32 slot = base + fieldOff;
    u32 target = rd32(c, slot);
    discover(c, slot, target, target == kNullOffset ? K_LIBOBJ : kindOfLibObj(c, target));
}

void edgeResource(Ctx* c, u32 base, u32 fieldOff)
{
    u32 slot = base + fieldOff;
    u32 target = rd32(c, slot);
    discover(c, slot, target, target == kNullOffset ? K_TEXRES : kindOfResource(c, target));
}

void expand(Ctx* c, u32 index)
{
    const u32 b = c->objs[index].src;

    switch (c->objs[index].kind)
    {
    case K_PACKAGE:
        // +0x00 m_pComponentList is always literal 0 and never relocated.
        edge(c, b, 0x04, K_PRESENTATION);
        edgeResource(c, b, 0x08);
        edgeLibObj(c, b, 0x0C);
        break;

    case K_PRESENTATION:
        edge(c, b, 0x00, K_SLIDE);
        edge(c, b, 0x04, K_SLIDE);
        break;

    case K_SLIDE:
        edge(c, b, 0x00, K_SLIDE);   // m_next
        edge(c, b, 0x04, K_SLIDE);   // m_prev, `pad0` in the header
        edgeInstance(c, b, 0x08);
        edge(c, b, 0x0C, K_ANIM);
        break;

    case K_INSTANCE:
    case K_IMAGE_INST:
    case K_TEXT_INST:
    case K_COMP_INST:
        edgeInstance(c, b, 0x00);
        edgeInstance(c, b, 0x04);
        edgeInstance(c, b, 0x08);
        edgeLibObj(c, b, 0x0C);   // m_component: any FELibObject subclass
        if (c->objs[index].kind == K_IMAGE_INST)
            edgeResource(c, b, 0x80);
        break;

    case K_LIBOBJ:
    case K_FEIMAGE:
    case K_FETEXT:
    case K_TLCOMPONENT:
        edgeLibObj(c, b, 0x00);
        edgeLibObj(c, b, 0x04);
        if (c->objs[index].kind == K_FEIMAGE)
            edgeResource(c, b, 0x68);
        else if (c->objs[index].kind == K_FETEXT)
            edgeResource(c, b, 0x68);
        else if (c->objs[index].kind == K_TLCOMPONENT)
        {
            edge(c, b, 0x68, K_SLIDE);
            edge(c, b, 0x6C, K_SLIDE);
        }
        break;

    case K_TEXRES:
    case K_FONTRES:
        edgeResource(c, b, 0x00);
        edgeResource(c, b, 0x04);
        break;

    case K_ANIM:
    {
        // 0x00 is the dead vtable slot.
        edge(c, b, 0x04, K_ANIM);
        edge(c, b, 0x08, K_ANIM);
        edgeInstance(c, b, 0x0C);
        // m_DLRingHead is void*: only m_cast_type gives the keyframe stride.
        edge(c, b, 0x18, rd16(c, b + 0x10) == 1 ? K_KF_V3 : K_KF_F);
        break;
    }

    case K_KF_F:
        edge(c, b, 0x10, K_KF_F);
        edge(c, b, 0x14, K_KF_F);
        break;

    case K_KF_V3:
        edge(c, b, 0x30, K_KF_V3);
        edge(c, b, 0x34, K_KF_V3);
        break;

    default: break;
    }
}


void* resolve(Ctx* c, u32 slotOff)
{
    u32 target = rd32(c, slotOff);
    if (target == kNullOffset)
        return nullptr;
    s32 index = c->byWord[target >> 2];
    if (index < 0)
        return nullptr;
    return c->arena + c->objs[index].dst;
}

void convAttrs(Ctx* c, FELibObjectAttributes& d, u32 s)
{
    for (int i = 0; i < 3; i++)
    {
        d.v3Position.e[i] = rdf32(c, s + 0x00 + i * 4);
        d.v3Rotation.e[i] = rdf32(c, s + 0x0C + i * 4);
        d.v3Scale.e[i] = rdf32(c, s + 0x18 + i * 4);
        d.v3Pivot.e[i] = rdf32(c, s + 0x24 + i * 4);
    }
    d.bVisible = c->blob[s + 0x30] != 0;
    // Four bytes, so no swap, and unaligned on disc, hence the memcpy.
    std::memcpy(d.colour.c, c->blob + s + 0x31, 4);
}

void convKeyframe(Ctx* c, FEAnimationKeyframe& d, u32 s)
{
    d.m_fPoint = rdf32(c, s + 0x00);
    d.m_fControl1 = rdf32(c, s + 0x04);
    d.m_fControl2 = rdf32(c, s + 0x08);
    d.m_fTime = rdf32(c, s + 0x0C);
}

void convLibObject(Ctx* c, FELibObject* d, u32 s)
{
    d->next = (FELibObject*)resolve(c, s + 0x00);
    d->prev = (FELibObject*)resolve(c, s + 0x04);
    convAttrs(c, d->m_attributes, s + 0x08);
    d->m_hashID = rd32(c, s + 0x40);
    std::memcpy(d->m_szName, c->blob + s + 0x44, 32);
    d->m_type = (eFELibObjectType)rd32(c, s + 0x64);
}

void convInstance(Ctx* c, TLInstance* d, u32 s)
{
    d->m_next = (TLInstance*)resolve(c, s + 0x00);
    d->m_prev = (TLInstance*)resolve(c, s + 0x04);
    d->pChildren = (TLInstance*)resolve(c, s + 0x08);
    d->m_component = (TLComponent*)resolve(c, s + 0x0C);
    d->m_fStartTime = rdf32(c, s + 0x10);
    d->m_fDuration = rdf32(c, s + 0x14);
    std::memcpy(d->m_szName, c->blob + s + 0x18, 32);
    d->m_hash = rd32(c, s + 0x38);
    convAttrs(c, d->m_overloadedAttributes, s + 0x3C);
    d->m_overloadFlags = rd32(c, s + 0x74);
    d->m_type = (eTimeLineAssetType)rd32(c, s + 0x78);
    d->m_priority = rd16(c, s + 0x7C);
    d->m_bVisible = c->blob[s + 0x7E] != 0;
}

void convResourceHandle(Ctx* c, FEResourceHandle* d, u32 s)
{
    d->m_next = (FEResourceHandle*)resolve(c, s + 0x00);
    d->m_prev = (FEResourceHandle*)resolve(c, s + 0x04);
    d->m_type = (eFEResourceType)rd32(c, s + 0x08);
    d->m_hashID = rd32(c, s + 0x0C);
    d->m_bValid = c->blob[s + 0x10] != 0;
}

void emit(Ctx* c, u32 index)
{
    const u32 s = c->objs[index].src;
    u8* dst = c->arena + c->objs[index].dst;

    switch (c->objs[index].kind)
    {
    case K_PACKAGE:
    {
        FEPackage* d = (FEPackage*)dst;
        d->m_pComponentList = nullptr;   // dead field, see expand()
        d->m_pFEPresentation = (FEPresentation*)resolve(c, s + 0x04);
        d->m_pResourceList = (FEResourceHandle*)resolve(c, s + 0x08);
        d->m_pFEObjectLibrary = (FELibObject*)resolve(c, s + 0x0C);
        d->m_uUniqueID = rd32(c, s + 0x10);
        d->m_uResourceCount = rd32(c, s + 0x14);
        break;
    }

    case K_PRESENTATION:
    {
        // 0x0C..0x2F on disc is a name and hash the runtime never reads.
        FEPresentation* d = (FEPresentation*)dst;
        d->m_slides = (TLSlide*)resolve(c, s + 0x00);
        d->m_currentSlide = (TLSlide*)resolve(c, s + 0x04);
        d->m_fadeDuration = rdf32(c, s + 0x08);
        break;
    }

    case K_SLIDE:
    {
        TLSlide* d = (TLSlide*)dst;
        d->m_next = (TLSlide*)resolve(c, s + 0x00);
        d->m_prev = (TLSlide*)resolve(c, s + 0x04);
        d->m_instances = (TLInstance*)resolve(c, s + 0x08);
        d->m_animations = (FEAnimation*)resolve(c, s + 0x0C);
        d->m_start = rdf32(c, s + 0x10);
        d->m_duration = rdf32(c, s + 0x14);
        d->m_time = rdf32(c, s + 0x18);
        d->m_uPlayMode = (eTimeLinePlayMode)rd32(c, s + 0x1C);
        std::memcpy(d->m_szName, c->blob + s + 0x20, 32);
        d->m_hash = rd32(c, s + 0x40);
        break;
    }

    case K_INSTANCE: convInstance(c, (TLInstance*)dst, s); break;

    case K_IMAGE_INST:
    {
        TLImageInstance* d = (TLImageInstance*)dst;
        convInstance(c, d, s);
        d->m_pTextureResource = (FETextureResource*)resolve(c, s + 0x80);
        break;
    }

    case K_COMP_INST:
    {
        TLComponentInstance* d = (TLComponentInstance*)dst;
        convInstance(c, d, s);
        d->m_fCurrentTime = rdf32(c, s + 0x80);
        break;
    }

    case K_TEXT_INST:
    {
        TLTextInstance* d = (TLTextInstance*)dst;
        convInstance(c, d, s);
        d->m_LocStrId = rd32(c, s + 0x80);
        std::memcpy(d->m_OverloadedAttributes.EffectColour.c, c->blob + s + 0x84, 4);
        d->m_OverloadedAttributes.BoxSize.x = rdf32(c, s + 0x88);
        d->m_OverloadedAttributes.BoxSize.y = rdf32(c, s + 0x8C);
        d->m_OverloadFlags = rd32(c, s + 0x90);
        // 0x94..0xEF is runtime scratch, zero on disc and never relocated.
        std::memset(&d->m_DrawInfo, 0, sizeof d->m_DrawInfo);
        d->m_pFontString = nullptr;
        d->m_DrawOptions = rd32(c, s + 0xF0);
        d->m_wcUserString = nullptr;
        d->m_UseScissorRect = c->blob[s + 0xF8] != 0;
        d->m_ScissorRect.X = rd16(c, s + 0xFA);
        d->m_ScissorRect.Y = rd16(c, s + 0xFC);
        d->m_ScissorRect.Width = rd16(c, s + 0xFE);
        d->m_ScissorRect.Height = rd16(c, s + 0x100);
        break;
    }

    case K_LIBOBJ: convLibObject(c, (FELibObject*)dst, s); break;

    case K_FEIMAGE:
    {
        FEImage* d = (FEImage*)dst;
        convLibObject(c, d, s);
        d->m_pFeTextureResource = (FETextureResource*)resolve(c, s + 0x68);
        break;
    }

    case K_FETEXT:
    {
        FEText* d = (FEText*)dst;
        convLibObject(c, d, s);
        d->m_pFeFontResource = (const FEFontResource*)resolve(c, s + 0x68);
        std::memcpy(d->m_TextAttributes.EffectColour.c, c->blob + s + 0x6C, 4);
        d->m_TextAttributes.BoxSize.x = rdf32(c, s + 0x70);
        d->m_TextAttributes.BoxSize.y = rdf32(c, s + 0x74);
        break;
    }

    case K_TLCOMPONENT:
    {
        TLComponent* d = (TLComponent*)dst;
        convLibObject(c, d, s);
        d->pChildren = (TLSlide*)resolve(c, s + 0x68);
        d->m_pActiveSlide = (TLSlide*)resolve(c, s + 0x6C);
        std::memcpy(d->m_szName, c->blob + s + 0x70, 32);
        d->m_hashID = rd32(c, s + 0x90);
        break;
    }

    case K_TEXRES:
    {
        FETextureResource* d = (FETextureResource*)dst;
        convResourceHandle(c, d, s);
        d->m_glTextureHandle = rd32(c, s + 0x14);
        break;
    }

    case K_FONTRES:
    {
        FEFontResource* d = (FEFontResource*)dst;
        convResourceHandle(c, d, s);
        d->m_pFontReference = nullptr;
        break;
    }

    case K_ANIM:
    {
        // Placement new, because the class has a virtual destructor and needs a real vptr.
        FEAnimation* d = new (dst) FEAnimation();
        d->m_next = (FEAnimation*)resolve(c, s + 0x04);
        d->m_prev = (FEAnimation*)resolve(c, s + 0x08);
        d->m_pTLInstanceTarget = (TLInstance*)resolve(c, s + 0x0C);
        d->m_cast_type = rd16(c, s + 0x10);
        d->pad12[0] = 0;
        d->pad12[1] = 0;
        d->m_type = (AnimType)rd32(c, s + 0x14);
        d->m_DLRingHead = resolve(c, s + 0x18);
        break;
    }

    case K_KF_F:
    {
        fAnimationKeyframe* d = (fAnimationKeyframe*)dst;
        convKeyframe(c, d->pKeyFrameData, s + 0x00);
        d->m_next = (fAnimationKeyframe*)resolve(c, s + 0x10);
        d->m_prev = (fAnimationKeyframe*)resolve(c, s + 0x14);
        break;
    }

    case K_KF_V3:
    {
        v3AnimationKeyframe* d = (v3AnimationKeyframe*)dst;
        convKeyframe(c, d->pKeyFrameDataX, s + 0x00);
        convKeyframe(c, d->pKeyFrameDataY, s + 0x10);
        convKeyframe(c, d->pKeyFrameDataZ, s + 0x20);
        d->m_next = (v3AnimationKeyframe*)resolve(c, s + 0x30);
        d->m_prev = (v3AnimationKeyframe*)resolve(c, s + 0x34);
        break;
    }

    default: break;
    }
}

}   // namespace

extern "C" void* port_fen_convert(const void* blob, unsigned long blobLen, const void* table,
                                  unsigned long tableLen)
{
    if (blob == nullptr || blobLen < sizeof(u32))
        return nullptr;

    Ctx c;
    std::memset(&c, 0, sizeof c);
    c.blob = (const u8*)blob;
    c.blobLen = (u32)blobLen;
    c.table = (const u32*)table;
    c.tableCount = (u32)(tableLen / 4);

    const u32 words = (c.blobLen + 3) / 4;
    c.byWord = (s32*)std::malloc(words * sizeof(s32));
    c.isSlot = (u8*)std::calloc(words, 1);
    if (c.byWord == nullptr || c.isSlot == nullptr)
    {
        std::free(c.byWord);
        std::free(c.isSlot);
        return nullptr;
    }
    std::memset(c.byWord, 0xFF, words * sizeof(s32));

    for (u32 i = 0; i < c.tableCount; i++)
    {
        u32 off = c.table[i];
        if (off < c.blobLen)
            c.isSlot[off >> 2] = 1;
    }

    discover(&c, kNullOffset, 0, K_PACKAGE);
    for (u32 i = 0; i < c.objCount && !c.failed; i++)
        expand(&c, i);

    if (c.failed || c.objCount == 0)
    {
        OSReport("port_fen_convert: graph walk failed after %u object(s)\n", c.objCount);
        std::free(c.byWord);
        std::free(c.isSlot);
        std::free(c.objs);
        return nullptr;
    }

    std::size_t total = 0;
    for (u32 i = 0; i < c.objCount; i++)
    {
        total = (total + 7u) & ~(std::size_t)7u;
        c.objs[i].dst = (u32)total;
        total += hostSize(c.objs[i].kind);
    }

    c.arena = (u8*)nlMalloc((size_t)total, 0x20, false);
    if (c.arena == nullptr)
    {
        std::free(c.byWord);
        std::free(c.isSlot);
        std::free(c.objs);
        return nullptr;
    }
    c.arenaSize = total;
    std::memset(c.arena, 0, total);

    for (u32 i = 0; i < c.objCount; i++)
        emit(&c, i);

    if (c.mismatches != 0)
    {
        OSReport("port_fen_convert: %u pointer field(s) absent from the "
                 "relocation table; layout drift?\n",
                 c.mismatches);
    }

    if (std::getenv("STRIKERS_DUMP_FEN") != nullptr)
    {
        FEPackage* pkg = (FEPackage*)c.arena;
        OSReport("[fen] package: presentation=%p resources=%p library=%p count=%lu\n",
                 (void*)pkg->m_pFEPresentation, (void*)pkg->m_pResourceList,
                 (void*)pkg->m_pFEObjectLibrary, (unsigned long)pkg->m_uResourceCount);
        FEPresentation* pres = pkg->m_pFEPresentation;
        if (pres != nullptr && pres->m_slides != nullptr)
        {
            TLSlide* head = pres->m_slides;
            TLSlide* slide = head->m_next;
            for (int guard = 0; guard < 64; guard++)
            {
                // The front end matches on the hash, so a host hash that differs from the
                // exporter's looks like missing content.
                OSReport("[fen]   slide '%s' stored=%#x computed=%#x%s\n", slide->m_szName,
                         (unsigned)slide->m_hash, (unsigned)nlStringLowerHash(slide->m_szName),
                         nlStringLowerHash(slide->m_szName) == slide->m_hash ? ""
                                                                             : "   <-- MISMATCH");
                TLInstance* ihead = slide->m_instances;
                if (ihead != nullptr)
                {
                    TLInstance* inst = ihead->m_next;
                    for (int g2 = 0; g2 < 64; g2++)
                    {
                        OSReport("[fen]     inst '%s' type=%d children=%p\n", inst->m_szName,
                                 (int)inst->m_type, (void*)inst->pChildren);
                        if (inst == ihead)
                            break;
                        inst = inst->m_next;
                    }
                }
                if (slide == head)
                    break;
                slide = slide->m_next;
            }
        }
    }

    void* root = c.arena;
    std::free(c.byWord);
    std::free(c.isSlot);
    std::free(c.objs);
    return root;
}
