#!/usr/bin/env python3
"""Execute the shipped PN and generated rigid Cg transforms against an oracle.

The extracted block is the production vertex shader, not a rewritten shader
model. Host Clang supplies Cg swizzles; this checks palette addressing and FP32
position/normal algebra, not Vita compiler precision or GPU rasterization.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    header = (root / "include/port/scene_shaders.hpp").read_text()
    shader = re.search(r'kSceneVertexShader\s*=\s*R"\((.*?)\)";', header, re.S).group(1)
    capacity = re.search(r"uniform float4 u_bones\[([^]]+)\]", shader).group(1)
    def transform_body(source):
        opening = source.index("{", source.index("out float4 position:"))
        # Keep every operation from main's opening through the skin transform.
        body = source[opening + 1:source.index(" float4 eye=", opening)]
        return re.sub(r"\bfloat([34])\(", r"make\1(", body)

    body = transform_body(shader)
    prelude = r'''
#include "port/renderer.hpp"
#include <algorithm>
#include <cassert>
#include <bit>
#include <cmath>
#include <cstdio>
#include <random>
using float3 = float __attribute__((ext_vector_type(3)));
using float4 = float __attribute__((ext_vector_type(4)));
float3 make3(float x,float y,float z) { return {x,y,z}; }
float4 make4(float x,float y,float z,float w) { return {x,y,z,w}; }
float dot(float3 a,float3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float dot(float4 a,float4 b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
struct SkinResult { float4 position; float3 normal; };
#define STRIKERS_SCENE_BONE_ROWS port::render::kSceneBoneRows
'''
    implementation = (
        "constexpr unsigned shaderCapacity = " + capacity + ";\n"
        "static_assert(shaderCapacity == port::render::kSceneBoneRows);\n"
        "SkinResult shader_skin(float4 a_position,float3 a_normal,float a_bone,\n"
        "                       const std::array<float4,shaderCapacity>& u_bones) {\n"
        + body + "\n return {p,n};\n}\n"
    )
    checks = (root / "tests/scene_skinning_test.cpp").read_text()
    with tempfile.TemporaryDirectory(prefix="strikers-skin-") as scratch:
        # Read the output of the real source generator, rather than applying
        # the production substitutions a second time inside this oracle.
        generator_source = pathlib.Path(scratch) / "generate.cpp"
        generator = pathlib.Path(scratch) / "generate"
        generator_source.write_text(
            '#include "port/scene_shader_variants.hpp"\n#include <iostream>\n'
            'int main() { const auto s = port::render::scene_vertex_shader(false, false);\n'
            ' if (s.empty()) return 1; std::cout << s; }\n'
        )
        subprocess.run([args.compiler, "-std=c++20", "-O2", "-I", str(root / "include"),
                        str(generator_source), "-o", str(generator)], check=True, timeout=60)
        rigid = subprocess.run([str(generator)], check=True, timeout=30,
                               capture_output=True, text=True).stdout
        assert len(rigid) < 65536 and "a_bone" not in rigid and "u_bones" not in rigid
        implementation += ("SkinResult shader_rigid(float4 a_position,float3 a_normal) {\n"
                           + transform_body(rigid) + "\n return {p,n};\n}\n")
        source = pathlib.Path(scratch) / "skinning.cpp"
        binary = pathlib.Path(scratch) / "skinning-test"
        source.write_text(prelude + implementation + checks)
        subprocess.run([args.compiler, "-std=c++20", "-O2", "-ffp-contract=off",
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "-I", str(root / "include"), str(source), "-o", str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
