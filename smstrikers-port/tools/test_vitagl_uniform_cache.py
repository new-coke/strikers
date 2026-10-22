#!/usr/bin/env python3
"""Compile the shipped provider's cache, linker, setters and public draw guard."""
import argparse
import pathlib
import re
import subprocess
import tempfile
from test_vitagl_renderbuffer_lifetime import function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang")
    parser.add_argument("--mutation", choices=("ignore-draw-result", "ignore-alias"))
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    provider = root / "extern/vitaGL/source/custom_shaders.c"
    source = provider.read_text()
    api = (root / "extern/vitaGL/source/vitaGL.h").read_text()
    api = api[api.index("enum {\n\tVGL_UNIFORM_PACK_CALLS"):api.index("void vglGetUniformCacheStats")]
    api += "void vglGetUniformCacheStats(vglUniformCacheStats *stats);\n"
    types = source[source.index("typedef struct uniform {"):source.index("// Internal shaders and array")]
    attr = source[source.index("typedef struct {\n\tGLuint idx;"):source.index("#ifdef STRICT_UNIFORMS_COMPLIANCE")]
    cache = "static GLboolean uniform_cache_enabled = GL_FALSE;\n" + source[source.index("static GLboolean uniform_cache_profile"):source.index("#ifdef HAVE_SHARK_LOG")]
    if args.mutation == "ignore-alias":
        cache = cache.replace("u->packed_alias->packed_dirty = GL_TRUE;", "u->packed_alias->packed_dirty = GL_FALSE;")
    names = ["get_uniform_alias_data_ptr", "has_block_alias", "glBindAttribLocation", "glCreateProgram",
             "glDeleteProgram", "glLinkProgram", "glProgramBinary", "glUseProgram", "glGetUniformLocation"]
    names += [f"glUniform{n}{suffix}" for n in range(1, 5) for suffix in ("i", "iv", "f", "fv")]
    names += [f"glUniformMatrix{n}fv" for n in (2, 3, 4)]
    bodies = "\n\n".join(function(source, name, provider) for name in names)
    macro = source[source.index("#define upload_uniforms()"):source.index("\n#else\n#define upload_uniforms()")]
    binding = source[source.index("#define set_default_attrib_binding()"):source.index("#define disable_draw_attrib")]
    draw_path = root / "extern/vitaGL/source/draw.c"
    draw_names = ("glDrawArrays", "glDrawArraysInstanced", "glDrawElements", "glDrawElementsBaseVertex",
                  "glDrawRangeElements", "glDrawRangeElementsBaseVertex", "glDrawElementsInstanced")
    draw = "\n\n".join(function(draw_path.read_text(), name, draw_path) for name in draw_names)
    if args.mutation == "ignore-draw-result":
        draw = draw.replace("if (is_draw_legal)", "if (GL_TRUE)")
    with tempfile.TemporaryDirectory(prefix="strikers-uniform-cache-") as scratch:
        temp = pathlib.Path(scratch)
        (temp / "uniform_api.inc").write_text(api)
        (temp / "uniform_types.inc").write_text(attr + types)
        (temp / "uniform_cache.inc").write_text(cache)
        (temp / "uniform_bodies.inc").write_text(binding + bodies)
        (temp / "uniform_macro.inc").write_text(macro)
        (temp / "uniform_draw.inc").write_text(draw)
        binary = temp / "uniform-test"
        subprocess.run([args.compiler, "-x", "c", "-std=gnu11", "-fgnu89-inline", "-O1", "-UNDEBUG",
                        "-DSKIP_ERROR_HANDLING=1", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", "-Wno-pointer-to-int-cast",
                        "-Wno-static-in-inline", "-I", str(temp),
                        str(root / "tests/vitagl_uniform_cache_test.c"), "-o", str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)
        # Builds with automatic FFP/texture-size uniform mutation retain the
        # original path. Execute their real init gate without emulating FFP.
        config = "\n\n".join(function(source, name, provider) for name in
                              ("uniform_cache_error", "vglConfigureUniformCache", "vglGetUniformCacheStats"))
        fallback = '''#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef unsigned char GLboolean;
typedef unsigned GLenum;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_INVALID_OPERATION 0x502
#define MAX_CUSTOM_PROGRAMS 16
#define PROG_INVALID 0
#define vgl_memset memset
#include "uniform_api.inc"
static struct { int status; } progs[16];
static GLboolean uniform_cache_enabled, uniform_cache_profile, uniform_cache_verify;
static vglUniformCacheStats uniform_cache_stats;
static GLenum vgl_error;
''' + config + '''
int main(void) {
 assert(vglConfigureUniformCache(1,1,0));
 assert(!uniform_cache_enabled && !uniform_cache_profile && !uniform_cache_verify);
 assert(!vglConfigureUniformCache(1,0,1) && vgl_error==GL_INVALID_OPERATION);
 progs[0].status=1;
 assert(!vglConfigureUniformCache(0,0,0));
 return 0;
}
'''
        (temp / "fallback.c").write_text(fallback)
        for flag in ("HAVE_FFP_SHADER_SUPPORT", "HAVE_GLSL_TEXTURE_SIZE"):
            subprocess.run([args.compiler, "-x", "c", "-std=c11", "-O1", "-UNDEBUG", "-D" + flag,
                            "-I", str(temp), str(temp / "fallback.c"), "-o", str(binary)], check=True, timeout=30)
            subprocess.run([str(binary)], check=True, timeout=10)
        print("FFP/texture-size compile variants: original-path gate PASS")


if __name__ == "__main__":
    main()
