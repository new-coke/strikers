#!/usr/bin/env python3
"""Execute the shipped provider's scene, transfer and GC code with a GPU model.

The cooperative semaphore scheduler explores a concrete producer/GC interleave.
It is a memory-lifetime regression, not a substitute for device GPU validation.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile


def function(source, name, path):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError("provider function not found: " + name)
    opening = source.index("{", match.start())
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\{|\}', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                line = source.count("\n", 0, match.start()) + 1
                return f'#line {line} "{path}"\n' + source[match.start():token.end()]
    raise ValueError("unbalanced provider function: " + name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--scenario", choices=("all", "attachment", "blit", "gc", "unsafe", "mips", "scene", "profile"), default="all")
    parser.add_argument("--baseline-git", action="store_true",
                        help="negative control: run against provider HEAD without changing the checkout")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    provider = root / "extern/vitaGL"
    prefix = subprocess.run(["git", "-C", str(provider), "rev-parse", "--show-prefix"],
                            capture_output=True, text=True, check=True, timeout=10).stdout.strip() if args.baseline_git else ""

    def read(name):
        if args.baseline_git:
            return subprocess.run(["git", "-C", str(provider), "show", "HEAD:" + prefix + "source/" + name],
                                  capture_output=True, text=True, check=True, timeout=10).stdout
        return (provider / "source" / name).read_text()

    sources = {name: read(name) for name in ("gxm.c", "framebuffers.c", "utils/gpu_utils.c", "utils/gpu_utils.h")}
    items = [("utils/gpu_utils.h", "gpu_free_texture_data"), ("gxm.c", "scene_end")]
    if "gxm_scene_is_active(void)" in sources["gxm.c"]:
        items.append(("gxm.c", "gxm_scene_is_active"))
    items += [("gxm.c", "scene_reset"), ("gxm.c", "glFinish"),
              ("framebuffers.c", "glBlitNamedFramebuffer"),
              ("utils/gpu_utils.c", "gpu_alloc_texture"),
              ("utils/gpu_utils.c", "gpu_alloc_mipmaps")]
    bodies = [function(sources[file], name, provider / "source" / file) for file, name in items]
    gxm = sources["gxm.c"]
    present_profile = "static PortVglPresentProfile *portPresentProfile" in gxm
    if args.scenario == "profile" and not present_profile:
        raise ValueError("provider has no present-profile hooks")
    if present_profile:
        begin = gxm.index("static PortVglPresentProfile *portPresentProfile")
        end = gxm.index("#define MAX_SCENES_PER_FRAME", begin)
        bodies.insert(0, gxm[begin:end])
    begin = gxm.index("// Garbage collector\n")
    end = gxm.index("GLboolean start_shader_compiler", begin)
    bodies.append(gxm[begin:end])
    bodies.append(function(gxm, "vglSwapBuffers", provider / "source/gxm.c"))
    bodies.append(function(sources["utils/gpu_utils.c"], "gpu_alloc_mapped_aligned_unsafe",
                           provider / "source/utils/gpu_utils.c"))
    # Execute the exact initialization statements as well as the worker body.
    semaphores = re.findall(r'^\s*gc_mutex\[[01]\] = sceKernelCreateSema\([^;]+;', gxm, re.M)
    if len(semaphores) != 2:
        raise ValueError("expected exactly two GC semaphore initialization statements")
    bodies.append("static void initialize_gc_semaphores() {\n" + "\n".join(semaphores) + "\n}")
    with tempfile.TemporaryDirectory(prefix="strikers-gpu-lifetime-") as scratch:
        temp = pathlib.Path(scratch)
        (temp / "vitagl_gpu_provider.inc").write_text("\n\n".join(bodies))
        binary = temp / "gpu-lifetime-test"
        profile_flags = ["-DSTRIKERS_PRESENT_PROFILE_TEST=1"] if present_profile else []
        subprocess.run([args.compiler, "-std=c++20", "-O1", "-UNDEBUG", *profile_flags,
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "-I", str(temp), "-I", str(root / "include"), str(root / "tests/vitagl_gpu_lifetime_test.cpp"),
                        "-o", str(binary)], check=True, timeout=60)
        subprocess.run([str(binary), args.scenario], check=True, timeout=30)


if __name__ == "__main__":
    main()
