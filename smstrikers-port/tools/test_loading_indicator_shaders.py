#!/usr/bin/env python3
"""Execute the production loading Cg body against raw retail YUVA operations.

Clang vector extensions adapt Cg syntax only. Texture fetches implement nearest
sampling; the independent oracle applies StorePixel's Y/UV masks and the chosen
RGB backdrop conversion. GPU interpolation and shader compilation need a Vita.
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
    header = (root / "include/port/loading_indicator_shaders.hpp").read_text()
    chunks = []
    for name in ("loadingVertexShader", "loadingColorMath", "loadingFragmentBody"):
        match = re.search(rf'{name}\s*=\s*R"cg\((.*?)\)cg";', header, re.S)
        if not match:
            raise ValueError(f"Production Cg block missing: {name}")
        body = match.group(1)
        body = re.sub(r"\buniform\s+", "", body)
        body = re.sub(r":(?:POSITION|COLOR\d*|TEXCOORD\d+|TEXUNIT\d+)\b", "", body)
        body = re.sub(r"\bout\s+(float[234])\s+", r"\1& ", body)
        body = re.sub(r"\bfloat([234])\(", r"make\1(", body)
        body = re.sub(r"(?<![\w.])(\d+\.\d*|\.\d+)(?![\w.])", r"\1f", body)
        body = body.replace("void main(", "void native_loading_vertex(")
        body = body.replace("float4 main(", "float4 native_loading_fragment(")
        chunks.append(body)
    test = (root / "tests/loading_indicator_shaders_test.cpp").read_text()
    marker = "// PRODUCTION_SHADERS"
    assert test.count(marker) == 1
    source_text = test.replace(marker, "\n".join(chunks))
    with tempfile.TemporaryDirectory(prefix="strikers-loading-shader-") as scratch:
        source = pathlib.Path(scratch) / "loading.cpp"
        binary = pathlib.Path(scratch) / "loading-test"
        source.write_text(source_text)
        subprocess.run([args.compiler, "-std=c++20", "-O2", "-ffp-contract=off",
                        "-I", str(root / "include"), str(source), "-o", str(binary)],
                       check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
