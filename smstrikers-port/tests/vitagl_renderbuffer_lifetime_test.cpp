// The harness inserts the shipped provider entry points without rewriting them.
// GPU allocations and their deferred retirement are the only mocked operations.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "renderbuffer lifetime: %s\n", message);
        std::exit(1);
    }
}
#define CHECK(condition) require(bool(condition), #condition)

using GLsizei = int;
using GLenum = unsigned;
using GLboolean = bool;
// Vita GLuint and pointers have the same width. Preserve that relationship on
// the host so the provider's pointer-valued object names remain lossless.
using GLuint = uintptr_t;
constexpr GLboolean GL_FALSE = false, GL_TRUE = true;
enum : GLenum
{
    GL_RENDERBUFFER, GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER, GL_DRAW_FRAMEBUFFER,
    GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT,
    GL_DEPTH24_STENCIL8, GL_DEPTH32F_STENCIL8, GL_DEPTH_COMPONENT,
    GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32,
    GL_DEPTH_COMPONENT32F, GL_INVALID_ENUM, GL_INVALID_VALUE, GL_INVALID_OPERATION,
    GL_OUT_OF_MEMORY
};

struct SceGxmDepthStencilSurface
{
    void* depthData = nullptr;
    void* stencilData = nullptr;
};
struct renderbuffer
{
    GLboolean active = false;
    SceGxmDepthStencilSurface depthbuffer;
    SceGxmDepthStencilSurface* depthbuffer_ptr = nullptr;
};
struct framebuffer
{
    SceGxmDepthStencilSurface* depthbuffer_ptr = nullptr;
    GLboolean is_depth_hidden = false;
};
constexpr int BUFFERS_NUM = 4;
static renderbuffer renderbuffers[BUFFERS_NUM];
static renderbuffer* active_rb;
static framebuffer* active_read_fb;
static framebuffer* active_write_fb;

static std::array<unsigned, 64> allocations{}, retired{}, freed{};
static unsigned allocationCount = 0, retirementCount = 0;
static GLenum vgl_error = 0;
static bool failDepthAllocation = false;

static void* allocate()
{
    CHECK(allocationCount < allocations.size());
    return &allocations[allocationCount++];
}
static void retire(void* pointer)
{
    for (unsigned i = 0; i < allocationCount; ++i)
        if (pointer == &allocations[i])
        {
            require(!retired[i], "the same GPU allocation was retired twice");
            require(!freed[i], "an already freed GPU allocation was retired");
            ++retired[i];
            ++retirementCount;
            return;
        }
    require(false, "retirement did not refer to a live test allocation");
}
static void collect()
{
    for (unsigned i = 0; i < allocationCount; ++i)
        if (retired[i] && !freed[i])
            freed[i] = 1;
}
static GLboolean init_depth_stencil_buffer(uint32_t width, uint32_t height,
                                          SceGxmDepthStencilSurface* surface, GLboolean stencil)
{
    CHECK(width && height);
    if (failDepthAllocation)
    {
        vgl_error = GL_OUT_OF_MEMORY;
        return GL_FALSE;
    }
    surface->depthData = allocate();
    surface->stencilData = stencil ? allocate() : nullptr;
    return GL_TRUE;
}

// Match the production build: error checks disabled, real depth/stencil enabled.
#define SKIP_ERROR_HANDLING 1
#define mark_as_dirty(pointer) retire(pointer)
#define SET_GL_ERROR_WITH_VALUE(error, value) \
    do { (void)(error); (void)(value); require(false, "unexpected GL error"); } while (0);
#define SET_GL_ERROR(error) SET_GL_ERROR_WITH_VALUE(error, 0)
#include "vitagl_renderbuffer_provider.inc"

static void attach(framebuffer& read, framebuffer& draw, GLuint readBuffer, GLuint drawBuffer)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, reinterpret_cast<GLuint>(&read));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, reinterpret_cast<GLuint>(&draw));
    glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, readBuffer);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, drawBuffer);
}

int main()
{
    framebuffer read, draw;
    GLuint first = 0;
    glGenRenderbuffers(1, &first);
    CHECK(first);
    glBindRenderbuffer(GL_RENDERBUFFER, first);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 960, 544);
    CHECK(allocationCount == 2 && retirementCount == 0);
    auto* buffer = reinterpret_cast<renderbuffer*>(first);
    const auto firstDepth = buffer->depthbuffer.depthData;
    attach(read, draw, first, first);
    CHECK(read.depthbuffer_ptr == &buffer->depthbuffer);
    CHECK(draw.depthbuffer_ptr == &buffer->depthbuffer);

    // A failed resize must keep both attachment pointers and their live storage.
    // The production provider previously retired the old surfaces first.
    const auto firstStencil = buffer->depthbuffer.stencilData;
    failDepthAllocation = true;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 320, 240);
    CHECK(vgl_error == GL_OUT_OF_MEMORY);
    CHECK(allocationCount == 2 && retirementCount == 0);
    CHECK(buffer->depthbuffer.depthData == firstDepth);
    CHECK(buffer->depthbuffer.stencilData == firstStencil);
    CHECK(read.depthbuffer_ptr == &buffer->depthbuffer && draw.depthbuffer_ptr == &buffer->depthbuffer);
    vgl_error = 0;
    glNamedRenderbufferStorage(first, GL_DEPTH_COMPONENT24, 320, 240);
    CHECK(vgl_error == GL_OUT_OF_MEMORY);
    CHECK(allocationCount == 2 && retirementCount == 0);
    CHECK(buffer->depthbuffer.depthData == firstDepth);
    CHECK(buffer->depthbuffer.stencilData == firstStencil);
    CHECK(read.depthbuffer_ptr == &buffer->depthbuffer && draw.depthbuffer_ptr == &buffer->depthbuffer);
    failDepthAllocation = false;
    vgl_error = 0;

    // A normal resize retires the previous surfaces once and keeps attachment
    // pointers valid because the provider's surface object itself is stable.
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 320, 240);
    CHECK(allocationCount == 4 && retirementCount == 2);
    CHECK(buffer->depthbuffer.depthData != firstDepth);
    CHECK(read.depthbuffer_ptr == buffer->depthbuffer_ptr);
    CHECK(draw.depthbuffer_ptr == buffer->depthbuffer_ptr);
    glDeleteRenderbuffers(1, &first);
    CHECK(!read.depthbuffer_ptr && !draw.depthbuffer_ptr && !active_rb);
    CHECK(retirementCount == 4);
    collect();

    // Reuse the same provider slot after the old GPU storage has been freed.
    // Before the fix, Storage retires the old depth/stencil for a second time.
    GLuint reused = 0;
    glGenRenderbuffers(1, &reused);
    CHECK(reused == first);
    glBindRenderbuffer(GL_RENDERBUFFER, reused);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 480, 272);
    CHECK(allocationCount == 6 && retirementCount == 4);
    attach(read, draw, reused, reused);
    glDeleteRenderbuffers(1, &reused);
    CHECK(!read.depthbuffer_ptr && !draw.depthbuffer_ptr && !active_rb);
    CHECK(retirementCount == 6);

    // Deleting the inactive name again must not enqueue its old surfaces.
    glDeleteRenderbuffers(1, &reused);
    CHECK(retirementCount == 6);
    const GLuint zero = 0;
    glDeleteRenderbuffers(1, &zero);
    CHECK(retirementCount == 6);

    // Delete one attachment while a different renderbuffer remains bound.
    // Also exercise a depth-only allocation and reuse before the next GC pass.
    GLuint pair[2]{};
    glGenRenderbuffers(2, pair);
    CHECK(pair[0] == first && pair[1] && pair[0] != pair[1]);
    glBindRenderbuffer(GL_RENDERBUFFER, pair[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 320, 240);
    CHECK(!reinterpret_cast<renderbuffer*>(pair[0])->depthbuffer.stencilData);
    glBindRenderbuffer(GL_RENDERBUFFER, pair[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 960, 544);
    auto* kept = reinterpret_cast<renderbuffer*>(pair[1]);
    attach(read, draw, pair[1], pair[0]);
    glDeleteRenderbuffers(1, pair);
    CHECK(!draw.depthbuffer_ptr);
    CHECK(read.depthbuffer_ptr == kept->depthbuffer_ptr && active_rb == kept);
    CHECK(retirementCount == 7);
    glDeleteRenderbuffers(1, pair + 1);
    CHECK(!read.depthbuffer_ptr && !active_rb);
    CHECK(retirementCount == 9 && allocationCount == 9);

    GLuint empty = 0;
    glGenRenderbuffers(1, &empty);
    glBindRenderbuffer(GL_RENDERBUFFER, empty);
    failDepthAllocation = true;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 320, 240);
    CHECK(vgl_error == GL_OUT_OF_MEMORY);
    CHECK(!reinterpret_cast<renderbuffer*>(empty)->depthbuffer_ptr);
    CHECK(retirementCount == 9 && allocationCount == 9);
    glDeleteRenderbuffers(1, &empty);
    CHECK(!active_rb && retirementCount == 9);
    failDepthAllocation = false;
    vgl_error = 0;

    GLuint named = 0;
    glGenRenderbuffers(1, &named);
    glNamedRenderbufferStorage(named, GL_DEPTH_COMPONENT24, 320, 240);
    CHECK(reinterpret_cast<renderbuffer*>(named)->depthbuffer_ptr);
    CHECK(!reinterpret_cast<renderbuffer*>(named)->depthbuffer.stencilData);
    CHECK(allocationCount == 10 && retirementCount == 9);
    glDeleteRenderbuffers(1, &named);
    CHECK(retirementCount == 10);
    collect();
    for (unsigned i = 0; i < allocationCount; ++i)
        CHECK(retired[i] == 1 && freed[i] == 1);
    std::puts("VitaGL renderbuffer lifetime: ten allocations retired exactly once; resize failure, retry, reuse, delete and attachment checks passed");
}
