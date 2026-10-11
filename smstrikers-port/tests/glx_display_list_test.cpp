#include "NL/glx/glxDisplayList.h"
#include "NL/gl/glMemory.h"
#include "dolphin/os/OSCache.h"
#include "dolphin/PPCArch.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned frameAllocations, resourceAllocations, syncs;
void* flushed;
u32 flushedBytes;
void* allocate(unsigned long bytes) {
    void* result = std::malloc(bytes);
    assert(result != nullptr);
    std::memset(result, 0xA5, bytes);
    return result;
}

void check(unsigned primitive, unsigned streamCount, int colorSlot, unsigned vertexCount) {
    std::vector<glModelStream> streams(streamCount);
    for (unsigned i = 0; i < streamCount; ++i)
        streams[i].id = int(i) == colorSlot ? 0x0C : i;
    std::vector<u16> source(vertexCount);
    for (unsigned i = 0; i < vertexCount; ++i)
        source[i] = u16(i * 7937 + 0x80F1);
    if (vertexCount > 0) source[0] = 0;
    if (vertexCount > 1) source[1] = 0xFFFF;
    if (vertexCount > 2) source[2] = 0x0123;

    glModelPacket packet{};
    packet.numVertices = vertexCount;
    packet.primType = primitive;
    packet.numStreams = streamCount;
    packet.streams = streams.data();
    const bool hasColor = colorSlot >= 0;
    const unsigned indexedStreams = streamCount - (hasColor ? 1 : 0);
    packet.indexBuffer = indexedStreams != 0 ? reinterpret_cast<uintptr_t>(source.data()) : 0;

    // Independent FIFO contract: opcode, BE vertex count, optional direct
    // color followed by the same BE source index for every indexed attribute.
    const u8 opcodes[] = {0x90, 0x98, 0xA0, 0x80, 0xA8, 0xB0};
    std::vector<u8> expected{opcodes[primitive], u8(vertexCount >> 8), u8(vertexCount)};
    for (u16 index : source) {
        if (hasColor) expected.push_back(0xFF);
        for (unsigned i = 0; i < indexedStreams; ++i) {
            expected.push_back(u8(index >> 8));
            expected.push_back(u8(index));
        }
    }
    expected.resize((expected.size() + 31) & ~size_t(31), 0);

    for (bool permanent : {false, true}) {
        frameAllocations = resourceAllocations = syncs = 0;
        flushed = nullptr;
        flushedBytes = 0;
        DisplayList* list = dlMakeDisplayList(&packet, permanent);
        const uintptr_t handle = reinterpret_cast<uintptr_t>(list);
        assert(dlIsDisplayList(handle));
        assert(dlGetStruct(handle) == list);
        assert(dlGetDisplayList(handle) == list->list);
        assert(dlGetSize(handle) == expected.size());
        assert(list->indices[0] == streamCount);
        assert(list->indices[1] == hasColor);
        assert(std::memcmp(list->list, expected.data(), expected.size()) == 0);
        assert(resourceAllocations == (permanent ? 2u : 0u));
        assert(frameAllocations == (permanent ? 0u : 2u));
        assert(flushed == list->list && flushedBytes == list->size && syncs == 1);
        std::free(list->list);
        std::free(list);
    }
}
}

void* glResourceAlloc(unsigned long bytes, eGLMemory) { ++resourceAllocations; return allocate(bytes); }
void* glFrameAlloc(unsigned long bytes, eGLMemory) { ++frameAllocations; return allocate(bytes); }
void nlZeroMemory(void* p, unsigned long bytes) { std::memset(p, 0, bytes); }
extern "C" void DCFlushRangeNoSync(void* p, u32 bytes) { flushed = p; flushedBytes = bytes; }
extern "C" void PPCSync() { ++syncs; }

int main() {
    unsigned cases = 0;
    for (unsigned primitive = 0; primitive < 6; ++primitive)
        for (unsigned streams = 0; streams <= 8; ++streams)
            for (int color : {-1, 0, int(streams) - 1}) {
                if (color >= int(streams)) continue;
                for (unsigned vertices : {0u, 1u, 2u, 3u, 31u, 32u, 257u}) {
                    check(primitive, streams, color, vertices);
                    ++cases;
                }
            }
    check(0, 8, -1, 65535);
    check(5, 8, 7, 65535);
    DisplayList invalid{};
    for (uintptr_t handle : {uintptr_t(0), reinterpret_cast<uintptr_t>(&invalid)}) {
        assert(!dlIsDisplayList(handle));
        assert(dlGetStruct(handle) == nullptr);
        assert(dlGetDisplayList(handle) == nullptr);
        assert(dlGetSize(handle) == 0);
    }
    std::printf("PASS: %u FIFO cases in frame/resource storage, max vertex count, null/bad magic\n", cases + 2);
}
