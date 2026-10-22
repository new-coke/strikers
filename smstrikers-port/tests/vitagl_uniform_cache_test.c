// Real provider bodies, a checked host allocator and a layout-sensitive SDK boundary.
// This verifies CPU packing/lifetime/error contracts; it does not measure Vita FPS.
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t GLboolean;
typedef unsigned GLuint, GLenum;
typedef intptr_t GLint; // The provider's opaque pointer location on this 64-bit host.
typedef int GLsizei;
typedef float GLfloat;
typedef char GLchar;
typedef void GLvoid;
#define GL_TRUE 1
#define GL_FALSE 0
#define GL_INVALID_OPERATION 0x502
#define GL_OUT_OF_MEMORY 0x505
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_TRIANGLES 4
#define GL_CG_VERTEX_SHADER_EXT 0x890e
#define GL_CG_FRAGMENT_SHADER_EXT 0x890f
#define GL_QUADS 7
#define GL_LINE_STRIP 3
#define GL_LINE_LOOP 2
#define MAX_CUSTOM_PROGRAMS 16
#define MAX_CUSTOM_SHADERS 8
#define TEXTURE_IMAGE_UNITS_NUM 8
#define VERTEX_ATTRIBS_NUM 8
#define UBOS_NUM 14
#define MEM_ALIGNMENT 16
#define VGL_MEM_EXTERNAL 9
#define VGL_MODE_POSTPONED 1
#define VGL_MODE_SHADER_PAIR 2
#define SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE 0
#define SCE_GXM_PARAMETER_CATEGORY_UNIFORM 1
#define SCE_GXM_PARAMETER_CATEGORY_SAMPLER 2
#define SCE_GXM_PARAMETER_CATEGORY_UNIFORM_BUFFER 3
#define SCE_GXM_INDEX_SOURCE_INDEX_16BIT 0
#define SCE_GXM_INDEX_SOURCE_INDEX_32BIT 1
#define SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT 2
#define SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT 3
#define SCE_GXM_INDEX_FORMAT_U16 0
#define SCE_GXM_INDEX_FORMAT_U32 1
typedef int SceGxmParameterCategory, SceGxmPrimitiveType;
typedef struct { uint32_t id, category, container, resource; } SceGxmProgramParameter;
typedef struct { uint32_t count; SceGxmProgramParameter params[16]; } SceGxmProgram;
typedef struct { uint32_t regIndex; } SceGxmVertexAttribute;
typedef struct { uint32_t stride; } SceGxmVertexStream;
typedef uint32_t SceGxmShaderPatcherId;
typedef void SceGxmVertexProgram;
typedef void SceGxmFragmentProgram;
typedef struct { unsigned raw; } blend_config;
typedef struct { const char *name; } block_uniform;
typedef struct {
    SceGxmProgram *prog;
    uint32_t unif_buf_size;
    unsigned ref_counter, dirty, is_glsl;
    void *source, *unif_blk;
} shader;
typedef struct { void *ptr; unsigned last_frame; } vbo;
#include "uniform_api.inc"
#include "uniform_types.inc"
static program progs[MAX_CUSTOM_PROGRAMS];
static GLuint cur_program;
static GLboolean dirty_vert_unifs, dirty_frag_unifs;
static GLenum vgl_error;
static void *gxm_context, *vgl_def_vert_buf, *vgl_def_frag_buf;
static unsigned vgl_framecount = 12;
static vbo *ubo_buf[8];
static unsigned ubo_offset[8];
static int glsl_sema_mode;
static SceGxmProgram vs, fs;
static shader vertex, fragment, binary_shaders[2];
static unsigned binaryCreates, binaryLoads, binaryDeletes, binaryReleases;
static unsigned translations, compilations;
static unsigned allocs, cacheAllocs, liveAllocs, packCalls, reserveCalls, bindCalls, drawCalls, finishes;
static unsigned failCache, failReserve, failPack, failBind, failCacheAt, cacheAttempts;
static int failBindStage = -1;
static unsigned ubos[2];
struct allocation { void *ptr; size_t size; };
static struct allocation allocations[256];
static void *tracked_malloc(size_t size, int cache) {
    if (cache && ++cacheAttempts == failCacheAt) return NULL;
    if (cache && failCache) { --failCache; return NULL; }
    void *ptr = calloc(1, size ? size : 1);
    assert(ptr);
    for (unsigned i = 0; i < 256; ++i) if (!allocations[i].ptr) {
        allocations[i] = (struct allocation){ptr, size};
        ++allocs; ++liveAllocs; cacheAllocs += cache;
        return ptr;
    }
    abort();
}
static void *vglMalloc(size_t size) { return tracked_malloc(size, 0); }
static void *vgl_memalign(unsigned align, size_t size, int type) {
    assert(align == 16 && type == VGL_MEM_EXTERNAL);
    return tracked_malloc(size, 1);
}
static void vgl_free(void *ptr) {
    if (!ptr) return;
    for (unsigned i = 0; i < 256; ++i) if (allocations[i].ptr == ptr) {
        allocations[i].ptr = NULL;
        --liveAllocs;
        free(ptr);
        return;
    }
    assert(!"unknown/double free");
}
#define vgl_memset memset
#define vgl_fast_memcpy memcpy
#define vgl_log(...) ((void)0)
struct metadata { const char *name; unsigned components, array, offset, stride; };
static struct metadata metadata[32];
static unsigned sceGxmProgramGetParameterCount(const SceGxmProgram *p) { return p->count; }
static uint32_t *vglProgramGetParameterBase(SceGxmProgram *p) { return (uint32_t *)p->params; }
static unsigned sceGxmProgramParameterGetCategory(const SceGxmProgramParameter *p) { return p->category; }
static unsigned sceGxmProgramParameterGetContainerIndex(const SceGxmProgramParameter *p) { return p->container; }
static unsigned sceGxmProgramParameterGetResourceIndex(const SceGxmProgramParameter *p) { return p->resource; }
static unsigned sceGxmProgramParameterGetComponentCount(const SceGxmProgramParameter *p) { return metadata[p->id].components; }
static unsigned sceGxmProgramParameterGetArraySize(const SceGxmProgramParameter *p) { return metadata[p->id].array; }
static const char *sceGxmProgramParameterGetName(const SceGxmProgramParameter *p) { return metadata[p->id].name; }
static int sceGxmProgramParameterIsSamplerCube(const SceGxmProgramParameter *p) { return p->resource == 2; }
static const SceGxmProgramParameter *sceGxmProgramFindParameterByName(const SceGxmProgram *p, const char *name) {
    for (unsigned i = 0; i < p->count; ++i) if (!strcmp(name, metadata[p->params[i].id].name)) return &p->params[i];
    return NULL;
}
static void *get_block_details(void *p, unsigned index) { (void)index; return p; }
static void release_shader(shader *s) {
    assert(s->dirty && !s->ref_counter && (s==&binary_shaders[0] || s==&binary_shaders[1]));
    s->prog=NULL; ++binaryReleases;
}
static GLuint glCreateShader(GLenum type) {
    assert(type==GL_CG_VERTEX_SHADER_EXT || type==GL_CG_FRAGMENT_SHADER_EXT);
    unsigned stage=type==GL_CG_FRAGMENT_SHADER_EXT;
    memset(&binary_shaders[stage],0,sizeof(shader)); ++binaryCreates;
    return stage+1;
}
static void glShaderBinary(GLsizei n,const GLuint *ids,GLenum format,const void *data,GLsizei bytes) {
    assert(n==1 && format==0 && bytes==4 && ids[0]>=1 && ids[0]<=2);
    unsigned stage=ids[0]-1; uint32_t token; memcpy(&token,data,4);
    assert(token==0xAAu+stage);
    binary_shaders[stage]=stage?fragment:vertex;
    ++binaryLoads;
}
static void glAttachShader(GLuint id,GLuint shader_id) {
    assert(shader_id>=1 && shader_id<=2);
    shader *s=&binary_shaders[shader_id-1];
    if(shader_id==1) progs[id-1].vshader=s; else progs[id-1].fshader=s;
    ++s->ref_counter;
}
static void glDeleteShader(GLuint id) {
    assert(id>=1 && id<=2 && binary_shaders[id-1].ref_counter);
    binary_shaders[id-1].dirty=1; ++binaryDeletes;
}
static void glsl_translator_set_process(shader *v, shader *f) { assert(v==&vertex && f==&fragment); ++translations; }
static void vgl_compile_shader(shader *s, int force) {
    assert(force && (s==&vertex || s==&fragment)); s->prog=s==&vertex?&vs:&fs; ++compilations;
}
static void sceGxmFinish(void *ctx) { (void)ctx; ++finishes; }
static void test_transpose(float *dst, const float *src, unsigned n) {
    for (unsigned y = 0; y < n; ++y) for (unsigned x = 0; x < n; ++x) dst[y*n+x] = src[x*n+y];
}
#define matrix2x2_transpose(d,s) test_transpose(d,s,2)
#define matrix3x3_transpose(d,s) test_transpose(d,s,3)
#define matrix4x4_transpose(d,s) test_transpose(d,s,4)
static _Alignas(64) uint8_t pool[16][4096];
static void *bound[2];
static void *vglReserveUniformCircularPoolBuffer(uint32_t bytes) {
    if (failReserve) { --failReserve; return NULL; }
    assert(bytes <= 4096);
    void *ptr = pool[reserveCalls++ % 16];
    memset(ptr, 0, bytes);
    return ptr;
}
static int sceGxmSetVertexDefaultUniformBuffer(void *ctx, void *ptr) {
    (void)ctx; ++bindCalls;
    if (failBind) { --failBind; return -1; }
    if (failBindStage == 0) { failBindStage = -1; return -1; }
    bound[0] = ptr; return 0;
}
static int sceGxmSetFragmentDefaultUniformBuffer(void *ctx, void *ptr) {
    (void)ctx; ++bindCalls;
    if (failBind) { --failBind; return -1; }
    if (failBindStage == 1) { failBindStage = -1; return -1; }
    bound[1] = ptr; return 0;
}
static void *vglReserveVertexUniformBuffer(uint32_t bytes) {
    void *p = vglReserveUniformCircularPoolBuffer(bytes);
    sceGxmSetVertexDefaultUniformBuffer(gxm_context, p);
    return vgl_def_vert_buf = p;
}
static void *vglReserveFragmentUniformBuffer(uint32_t bytes) {
    void *p = vglReserveUniformCircularPoolBuffer(bytes);
    sceGxmSetFragmentDefaultUniformBuffer(gxm_context, p);
    return vgl_def_frag_buf = p;
}
static int sceGxmSetUniformDataF(void *buffer, const SceGxmProgramParameter *p, unsigned offset, unsigned count, const float *values) {
    ++packCalls;
    if (failPack) { --failPack; return -1; }
    const struct metadata m = metadata[p->id];
    assert(offset == 0 && count == m.components * m.array);
    // Non-contiguous arrays and stage-specific offsets catch memcpy-of-raw-data
    // and shared-alias mistakes without claiming to emulate the SDK's format.
    for (unsigned i = 0; i < count; ++i) {
        unsigned at = m.offset + ((i / m.components) * m.stride + i % m.components) * 4;
        assert(at + 4 <= 4096);
        memcpy((uint8_t *)buffer + at, &values[i], 4);
    }
    return 0;
}
static void sceGxmSetVertexUniformBuffer(void *c, unsigned idx, void *ptr) { (void)c; assert(idx == 3 && ptr == (uint8_t *)ubo_buf[1]->ptr + 8); ++ubos[0]; }
static void sceGxmSetFragmentUniformBuffer(void *c, unsigned idx, void *ptr) { (void)c; assert(idx == 3 && ptr == (uint8_t *)ubo_buf[1]->ptr + 8); ++ubos[1]; }
#include "uniform_cache.inc"
#define get_uniform_from_ptr(ptr, offs) (-ptr)
// Independent vertex-program caching is OFF in this uniform-cache oracle.
static void initialize_vertex_program_cache(program *p) { assert(!p->vertex_cache); }
static void release_vertex_program_cache(program *p) { assert(!p->vertex_cache); }
#include "uniform_bodies.inc"
#include "uniform_macro.inc"
static GLboolean prepare_uniforms(void) {
    program *p = &progs[cur_program - 1];
    if (uniform_cache_enabled || uniform_cache_profile) return upload_packed_uniforms(p);
    upload_uniforms();
    return GL_TRUE;
}
static GLboolean _glDrawElements_CustomShadersIMPL(void *src, int count, int a, int b, int type) {
    (void)src;(void)count;(void)a;(void)b;(void)type; return prepare_uniforms();
}
static GLboolean _glDrawArrays_CustomShadersIMPL(GLint first, int count, GLboolean instanced) {
    (void)first;(void)count;(void)instanced; return prepare_uniforms();
}
static void _glDrawArrays_FixedFunctionIMPL(GLint first, int count) { (void)first;(void)count; abort(); }
static void _glDrawElements_FixedFunctionIMPL(void *s,int c,int a,int b,int t) { (void)s;(void)c;(void)a;(void)b;(void)t; abort(); }
static GLboolean scene_reset(void) { return GL_TRUE; }
static unsigned ffp_vertex_attrib_state;
static struct { uintptr_t index_array_unit; } vao, *cur_vao = &vao;
#define gl_primitive_to_gxm(mode, out, count) ((out) = (mode))
#define setup_elements_indices(type) type *ptr = (type *)src;
#define setup_8bit_elements_indices() uint16_t *ptr = src
#define restore_polygon_mode(p) ((void)(p))
static void *gpu_alloc_mapped_temp(size_t n) { return vglMalloc(n); }
static void sceGxmDraw(void *ctx, int prim, int fmt, void *ptr, int count) { (void)ctx;(void)prim;(void)fmt; assert(ptr && count == 3); ++drawCalls; }
static void sceGxmDrawInstanced(void *ctx, int prim, int fmt, void *ptr, int total, int count) {
    assert(total == count); sceGxmDraw(ctx,prim,fmt,ptr,count);
}
static uint16_t default_idx_ptr[3]={0,1,2}, default_quads_idx_ptr[6], default_line_strips_idx_ptr[6];
#include "uniform_draw.inc"

static void fixture(void) {
    assert(liveAllocs == 0);
    memset(&vs,0,sizeof vs); memset(&fs,0,sizeof fs);
    metadata[0] = (struct metadata){"shared",4,1,0,4};
    metadata[1] = (struct metadata){"bones",4,99,64,4};
    metadata[2] = (struct metadata){"_matrix",4,4,1664,8};
    metadata[3] = (struct metadata){"shared",4,1,64,4};
    metadata[4] = (struct metadata){"tint",3,2,0,4};
    metadata[5] = (struct metadata){"_sampler",1,1,0,1};
    metadata[6] = (struct metadata){"cube",1,1,0,1};
    vs.count = 3; fs.count = 4;
    vs.params[0]=(SceGxmProgramParameter){0,1,14,0};
    vs.params[1]=(SceGxmProgramParameter){1,1,14,0};
    vs.params[2]=(SceGxmProgramParameter){2,1,14,0};
    fs.params[0]=(SceGxmProgramParameter){3,1,14,0};
    fs.params[1]=(SceGxmProgramParameter){4,1,14,0};
    fs.params[2]=(SceGxmProgramParameter){5,2,0,0};
    fs.params[3]=(SceGxmProgramParameter){6,2,0,2};
    vertex=(shader){.prog=&vs,.unif_buf_size=2048};
    fragment=(shader){.prog=&fs,.unif_buf_size=128};
    dirty_vert_unifs=dirty_frag_unifs=0; vgl_error=0;
    failCache=failReserve=failPack=failBind=0;
    failCacheAt=0; glsl_sema_mode=0;
}
static GLuint link_program(void) {
    GLuint id = glCreateProgram(); assert(id && id <= MAX_CUSTOM_PROGRAMS);
    progs[id-1].vshader=&vertex; progs[id-1].fshader=&fragment;
    ++vertex.ref_counter; ++fragment.ref_counter;
    glLinkProgram(id);
    return id;
}
static void draw(GLuint id) {
    static const uint16_t indices[3]={0,1,2};
    glUseProgram(id); glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);
}
static void check_image(GLuint id) {
    program *p=&progs[id-1];
    for(unsigned stage=0;stage<2;stage++) {
        uint8_t expected[4096]={0};
        uniform *u=stage?p->frag_uniforms:p->vert_uniforms;
        unsigned n=stage?p->frag_uniforms_num:p->vert_uniforms_num;
        const unsigned before=packCalls;
        for(unsigned i=0;i<n;i++) if(u[i].size && u[i].size!=UINT32_MAX)
            assert(!sceGxmSetUniformDataF(expected,u[i].ptr,0,u[i].size,u[i].data));
        packCalls=before;
        assert(bound[stage]);
        assert(!memcmp(bound[stage],expected,stage?128:2048));
    }
}
static void check_off(void) {
    fixture(); assert(vglConfigureUniformCache(0,0,0));
    unsigned caches=cacheAllocs; GLuint id=link_program();
    assert(progs[id-1].status==PROG_LINKED && cacheAllocs==caches);
    unsigned packs=packCalls;
    draw(id); check_image(id); draw(id); check_image(id);
    assert(packCalls==packs+10);
    assert(!uniform_cache_stats.stage[0][0] && !uniform_cache_stats.cache_bytes);
    glDeleteProgram(id); assert(!liveAllocs);
}
static void check_cache(void) {
    fixture(); assert(vglConfigureUniformCache(1,1,0));
    GLuint a=link_program(), b=link_program();
    program *p=&progs[a-1];
    unsigned linkedAllocations=allocs;
    void *linkedCache=p->packed[0].data;
    glLinkProgram(a); // Existing provider contract: a linked object is not relinked.
    assert(p->status==PROG_LINKED && allocs==linkedAllocations && p->packed[0].data==linkedCache);
    assert(p->vert_uniforms[0].data==p->frag_uniforms[0].data);
    assert(p->vert_uniforms[0].packed_alias==&p->frag_uniforms[0]);
    assert(!vglConfigureUniformCache(0,0,0));
    unsigned packs=packCalls;
    draw(a); check_image(a); assert(packCalls==packs+5);
    packs=packCalls; void *old=bound[0];
    draw(a); check_image(a); assert(packCalls==packs && bound[0]!=old);
    GLint shared=glGetUniformLocation(a,"shared");
    glUniform4f(shared,1,2,3,4); glUniform4f(shared,9,8,7,6);
    draw(a); check_image(a); assert(packCalls==packs+2);
    packs=packCalls;
    float palette[396]; for(unsigned i=0;i<396;i++) palette[i]=(float)i/7;
    glUniform4fv(glGetUniformLocation(a,"bones"),99,palette);
    draw(a); check_image(a); assert(packCalls==packs+1);
    draw(b); check_image(b); // Same compiled shaders, different raw values/cache.
    packs=packCalls; draw(a); check_image(a); assert(packCalls==packs);
    for(unsigned i=0;i<40;i++) { draw(a); check_image(a); } // GPU pool wrap.
    assert(packCalls==packs);
    assert(uniform_cache_stats.stage[0][VGL_UNIFORM_REUSED_VALUES]>0);
    assert(uniform_cache_stats.stage[0][VGL_UNIFORM_CACHE_COPY_BYTES]>2048);
    unsigned before=cacheAllocs; glUniform1i(glGetUniformLocation(a,"sampler"),3);
    glUniform1i(glGetUniformLocation(a,"cube"),4);
    draw(a); check_image(a); assert(cacheAllocs==before && packCalls==packs);
    glDeleteProgram(a); glDeleteProgram(b);
    assert(!liveAllocs && !uniform_cache_stats.cache_bytes);
}
static void check_setters(void) {
    fixture(); assert(vglConfigureUniformCache(1,0,1));
    GLuint id=link_program(); draw(id);
    GLint u=glGetUniformLocation(id,"matrix"), s=glGetUniformLocation(id,"shared");
    assert(u != -1 && s != -1);
    GLint iv[16]; float fv[32]; for(unsigned i=0;i<16;i++)iv[i]=i-7;
    for(unsigned i=0;i<32;i++)fv[i]=(float)i/3;
#define CHECK(call) do { call; draw(id); check_image(id); } while(0)
    CHECK(glUniform1i(s,3)); CHECK(glUniform1iv(s,3,iv)); CHECK(glUniform1f(s,-0.0f)); CHECK(glUniform1fv(s,4,fv));
    CHECK(glUniform2i(s,2,3)); CHECK(glUniform2iv(s,2,iv)); CHECK(glUniform2f(s,2,3)); CHECK(glUniform2fv(s,2,fv));
    CHECK(glUniform3i(s,2,3,4)); CHECK(glUniform3iv(s,1,iv)); CHECK(glUniform3f(s,2,3,4)); CHECK(glUniform3fv(s,1,fv));
    CHECK(glUniform4i(s,2,3,4,5)); CHECK(glUniform4iv(s,1,iv)); CHECK(glUniform4f(s,2,3,4,5)); CHECK(glUniform4fv(s,1,fv));
    for(unsigned tr=0;tr<2;tr++) {
        CHECK(glUniformMatrix2fv(u,2,tr,fv));
        if(tr) assert(progs[id-1].vert_uniforms[2].data[1]==fv[2]);
        CHECK(glUniformMatrix3fv(u,1,tr,fv));
        if(tr) assert(progs[id-1].vert_uniforms[2].data[1]==fv[3]);
        CHECK(glUniformMatrix4fv(u,1,tr,fv));
        if(tr) assert(progs[id-1].vert_uniforms[2].data[1]==fv[4]);
    }
    assert(uniform_cache_stats.verify_checks>=46);
    glDeleteProgram(id); assert(!liveAllocs);
#undef CHECK
}
static void check_failures(void) {
    for(unsigned failure=0;failure<3;failure++) {
        fixture(); assert(vglConfigureUniformCache(1,1,0));
        GLuint id=link_program(); draw(id);
        unsigned draws=drawCalls;
        glUniform4f(glGetUniformLocation(id,"shared"),1,3,5,7);
        if(failure==0)failPack=1;
        if(failure==1)failReserve=1;
        if(failure==2)failBind=1;
        draw(id);
        assert(drawCalls==draws && vgl_error && dirty_vert_unifs);
        assert(progs[id-1].vert_uniforms[0].packed_dirty);
        vgl_error=0; draw(id); check_image(id); assert(drawCalls==draws+1);
        assert(uniform_cache_stats.errors==1);
        glDeleteProgram(id); assert(!liveAllocs);
    }
    fixture(); assert(vglConfigureUniformCache(1,1,0));
    failCache=2; GLuint id=link_program();
    assert(progs[id-1].status==PROG_LINKED && !progs[id-1].packed[0].data);
    draw(id); check_image(id);
    assert(uniform_cache_stats.fallback_uploads==2);
    glDeleteProgram(id); assert(!liveAllocs);
    fixture(); assert(vglConfigureUniformCache(1,0,1));
    failCache=1; id=link_program();
    assert(progs[id-1].status==PROG_UNLINKED && vgl_error==GL_OUT_OF_MEMORY);
    assert(!progs[id-1].vert_uniforms && !progs[id-1].frag_uniforms && !liveAllocs);
    assert(vertex.ref_counter==1 && fragment.ref_counter==1);
    vgl_error=0; glLinkProgram(id);
    assert(progs[id-1].status==PROG_LINKED && !vgl_error);
    assert(vertex.ref_counter==1 && fragment.ref_counter==1);
    draw(id); check_image(id);
    glDeleteProgram(id); assert(!liveAllocs && !uniform_cache_stats.cache_bytes);
    fixture(); assert(!vglConfigureUniformCache(1,1,1));
    assert(vglConfigureUniformCache(1,0,1)); id=link_program(); draw(id);
    unsigned draws=drawCalls, reservations=reserveCalls;
    progs[id-1].packed[0].data[2048]=0; draw(id);
    assert(drawCalls==draws && reserveCalls==reservations && vgl_error);
    progs[id-1].packed[0].data[2048]=UNIFORM_CACHE_GUARD_VALUE;
    draw(id); check_image(id);
    progs[id-1].packed[0].data[0]^=1; draws=drawCalls; draw(id);
    assert(drawCalls==draws && vgl_error); // Reference mismatch before publication.
    glDeleteProgram(id); assert(!liveAllocs);
}
static void check_ubo(void) {
    fixture(); assert(vglConfigureUniformCache(1,1,0));
    GLuint id=link_program();
    uint8_t storage[32]; vbo buffer={storage,0}; ubo_buf[1]=&buffer; ubo_offset[1]=8;
    ubo *f=vglMalloc(sizeof *f), *v=vglMalloc(sizeof *v);
    *f=(ubo){.bind=1,.idx=3}; *v=(ubo){.bind=0,.idx=0,.alias=f};
    progs[id-1].frag_ubos=f; progs[id-1].vert_ubos=v;
    unsigned vsBefore=ubos[0],fsBefore=ubos[1]; draw(id); check_image(id);
    assert(ubos[0]==vsBefore+1 && ubos[1]==fsBefore+1 && buffer.last_frame==vgl_framecount);
    glDeleteProgram(id); assert(!liveAllocs);
}
static void check_draw_guards(void) {
    fixture(); assert(vglConfigureUniformCache(1,1,0));
    GLuint id=link_program(); draw(id);
    uint16_t indices[3]={0,1,2};
    for(unsigned command=0;command<7;command++) {
        unsigned before=drawCalls;
        glUseProgram(id); failBindStage=1; // VS succeeded, FS binding fails.
        switch(command) {
        case 0: glDrawArrays(GL_TRIANGLES,0,3); break;
        case 1: glDrawArraysInstanced(GL_TRIANGLES,0,3,1); break;
        case 2: glDrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices); break;
        case 3: glDrawElementsBaseVertex(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices,0); break;
        case 4: glDrawRangeElements(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,indices); break;
        case 5: glDrawRangeElementsBaseVertex(GL_TRIANGLES,0,2,3,GL_UNSIGNED_SHORT,indices,0); break;
        case 6: glDrawElementsInstanced(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices,1); break;
        }
        assert(drawCalls==before && dirty_frag_unifs && !dirty_vert_unifs);
        draw(id); check_image(id); assert(drawCalls==before+1);
    }
    glDeleteProgram(id); assert(!liveAllocs);
}
static void check_sampler_only(void) {
    for(unsigned mode=0;mode<3;mode++) {
        fixture(); assert(vglConfigureUniformCache(mode!=0,mode!=2,mode==2));
        vs.count=fs.count=1;
        vs.params[0]=fs.params[0]=(SceGxmProgramParameter){5,2,0,0};
        vertex.unif_buf_size=fragment.unif_buf_size=0;
        unsigned packs=packCalls,caches=cacheAllocs;
        GLuint id=link_program(); assert(progs[id-1].status==PROG_LINKED);
        assert(!progs[id-1].packed[0].data && !progs[id-1].packed[1].data);
        draw(id);
        glUniform1i(glGetUniformLocation(id,"sampler"),2); draw(id);
        assert(packCalls==packs && cacheAllocs==caches && !vgl_error);
        assert(progs[id-1].vert_uniforms[0].data==(float *)(uintptr_t)2);
        glDeleteProgram(id); assert(!liveAllocs);
    }
}
static void check_program_binary(void) {
    for(unsigned mode=0;mode<2;mode++) {
        fixture(); assert(vglConfigureUniformCache(mode,1,0));
        uint32_t blob[1+VERTEX_ATTRIBS_NUM+4]={0};
        unsigned header=1+VERTEX_ATTRIBS_NUM;
        blob[header]=4; blob[header+1]=0xAA;
        blob[header+2]=4; blob[header+3]=0xAB;
        unsigned loads=binaryLoads,deletes=binaryDeletes,releases=binaryReleases;
        GLuint id=glCreateProgram();
        glProgramBinary(id,0,blob,sizeof blob);
        assert(progs[id-1].status==PROG_LINKED && binaryLoads==loads+2 && binaryDeletes==deletes+2);
        assert((progs[id-1].packed[0].data!=NULL)==(mode!=0));
        assert(progs[id-1].vshader->unif_buf_size==2048 && progs[id-1].fshader->unif_buf_size==128);
        draw(id); check_image(id);
        glUniform4f(glGetUniformLocation(id,"shared"),2,4,6,8); draw(id); check_image(id);
        glDeleteProgram(id);
        assert(binaryReleases==releases+2 && !liveAllocs && !uniform_cache_stats.cache_bytes);
    }
}
static void check_postponed_retry(void) {
    for(unsigned allocation=1;allocation<=4;allocation++) {
        fixture(); assert(vglConfigureUniformCache(1,0,1));
        metadata[7]=(struct metadata){"a_pos",4,1,0,4};
        vs.params[vs.count++]=(SceGxmProgramParameter){7,0,0,2};
        vs.params[vs.count++]=(SceGxmProgramParameter){7,3,0,3};
        fs.params[fs.count++]=(SceGxmProgramParameter){7,3,0,3};
        block_uniform block={"Globals"}; vertex.unif_blk=fragment.unif_blk=&block;
        vertex.is_glsl=fragment.is_glsl=1;
        vertex.prog=fragment.prog=NULL;
        glsl_sema_mode=VGL_MODE_POSTPONED;
        GLuint id=glCreateProgram(); program *p=&progs[id-1];
        p->vshader=&vertex; p->fshader=&fragment;
        vertex.ref_counter=fragment.ref_counter=1;
        glBindAttribLocation(id,3,"a_pos");
        void *originalMap=p->glsl_attr_map;
        SceGxmVertexAttribute originalAttr[VERTEX_ATTRIBS_NUM]; memcpy(originalAttr,p->attr,sizeof originalAttr);
        unsigned compilesBefore=compilations,translatesBefore=translations;
        failCacheAt=cacheAttempts+allocation;
        glLinkProgram(id);
        assert(p->status==PROG_UNLINKED && vgl_error==GL_OUT_OF_MEMORY);
        assert(p->glsl_attr_map==originalMap && p->num_glsl_attr==1);
        assert(!p->vert_uniforms && !p->frag_uniforms && !p->vert_uniforms_num && !p->frag_uniforms_num);
        assert(!p->vert_ubos && !p->frag_ubos && !p->attr_num && !p->attr_highest_idx);
        assert(!memcmp(p->attr,originalAttr,sizeof originalAttr));
        for(unsigned i=0;i<TEXTURE_IMAGE_UNITS_NUM;i++) assert(!p->vert_texunits[i] && !p->frag_texunits[i]);
        assert(!p->max_vert_texunit_idx && !p->max_frag_texunit_idx);
        assert(vertex.ref_counter==1 && fragment.ref_counter==1 && liveAllocs==1);
        assert(glsl_sema_mode==VGL_MODE_POSTPONED && compilations==compilesBefore+2 && translations==translatesBefore+1);
        vgl_error=0; glLinkProgram(id);
        assert(p->status==PROG_LINKED && !vgl_error && !p->glsl_attr_map);
        assert(compilations==compilesBefore+2 && translations==translatesBefore+1);
        assert(p->attr_num==1 && p->attr_highest_idx==4 && p->attr[3].regIndex==2 && p->attr_map[0]==3);
        assert(p->vert_uniforms_num==3 && p->frag_uniforms_num==4);
        assert(p->vert_uniforms[0].data==p->frag_uniforms[0].data && p->vert_uniforms[0].packed_alias==&p->frag_uniforms[0]);
        assert(p->vert_ubos->alias==p->frag_ubos && vertex.ref_counter==1 && fragment.ref_counter==1);
        uint8_t storage[32]; vbo buffer={storage,0}; ubo_buf[1]=&buffer; ubo_offset[1]=8;
        p->frag_ubos->bind=1; draw(id); check_image(id);
        glDeleteProgram(id); assert(!liveAllocs && !uniform_cache_stats.cache_bytes);
        assert(!vertex.ref_counter && !fragment.ref_counter);
    }
}
int main(void) {
    _Static_assert(sizeof(SceGxmProgramParameter)==16,"production parameter iteration");
    check_off(); check_cache(); check_setters(); check_failures(); check_ubo(); check_draw_guards();
    check_sampler_only(); check_program_binary();
    check_postponed_retry();
    puts("Uniform provider: OFF, aliases, 99 rows, 19 setters, program/pool switch, SDK errors, canaries, fallback and lifecycle PASS");
    return 0;
}
