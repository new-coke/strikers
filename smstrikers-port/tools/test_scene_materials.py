#!/usr/bin/env python3
"""Execute the shipped Cg fragment body against the original GLX TEV stages.

Clang's vector extension supplies Cg swizzles for this host algebra check.
Texture fetches are constant samples: GPU rasterization/filtering are outside
this test. The TEV configuration function is extracted verbatim from glxSend,
so the expected stage sequence is independent of the native shader formulas.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile


def function(text, declaration):
    start = text.index(declaration)
    opening = text.index("{", start)
    while ";" in text[start:opening]:
        start = text.index(declaration, start + len(declaration))
        opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    shader_header = (root / "include/port/scene_shaders.hpp").read_text()
    fragment = re.search(r'kSceneFragmentShader\s*=\s*R"\((.*?)\)";', shader_header, re.S).group(1)
    if "float4 fragmentPosition:WPOS," in fragment:
        fragment = fragment.replace(" float4 fragmentPosition:WPOS,\n", "")
        fragment = fragment.replace(",\n uniform float4 u_fogrange[3],uniform float4 u_fograngecontrol", "")
        fragment = fragment.replace("):COLOR {", "):COLOR {\n float4 fragmentPosition=float4(0,0,0,0);"
            "float4 u_fogrange[3]={float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1)};"
            "float4 u_fograngecontrol=float4(0,0,0,0);", 1)
    fragment = re.sub(r"\bclamp\(", "std::clamp(", fragment)
    fragment = re.sub(r"\buniform\s+", "", fragment)
    fragment = re.sub(r":(?:COLOR\d*|TEXCOORD\d+|TEXUNIT\d+)\b", "", fragment)
    fragment = re.sub(r"\bfloat([234])\(", r"make\1(", fragment)
    fragment = re.sub(r"\b(\d+\.\d+)\b(?![fF])", r"\1f", fragment)
    fragment = fragment.replace("float4 main(", "float4 native_fragment(")
    fragment = fragment.replace("discard;", "throw Discard{};")
    glx = (root / "src/NL/glx/glxSend.cpp").read_text()
    original = function(glx, "static unsigned long glx_SwitchTexConfig(")
    original += "\n" + function(glx, "static inline void glx_SwitchProgram(")
    draw = function(glx, "static void glx_DrawPacket(")
    cleanup_start = draw.rindex("    if (glx_norasterizedalpha)")
    cleanup_end = draw.index("    if (glx_viewport)", cleanup_start)
    original += "\nvoid original_finish_draw() {\n" + draw[cleanup_start:cleanup_end] + "}\n"
    prelude = (root / "tests/scene_tev_oracle.hpp").read_text()
    checks = (root / "tests/scene_materials_test.cpp").read_text()
    with tempfile.TemporaryDirectory(prefix="strikers-material-") as scratch:
        source = pathlib.Path(scratch) / "material.cpp"
        binary = pathlib.Path(scratch) / "material-test"
        source.write_text(prelude + "\n" + original + "\n" + fragment + "\n" + checks)
        subprocess.run([args.compiler, "-std=c++20", "-O2", "-ffp-contract=off",
                        "-Wno-unused-value", "-I", str(root / "include"), str(source), "-o", str(binary)], check=True,
                       timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
