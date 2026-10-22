#!/usr/bin/env python3
"""Exercise production GPU allocation consumers with deterministic failures."""
import argparse
import pathlib
import subprocess
import tempfile

from test_vitagl_renderbuffer_lifetime import function


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--provider-source", type=pathlib.Path)
    parser.add_argument("--case", choices=("all", "depth", "buffer", "pool"), default="all")
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    provider = args.provider_source or root / "extern/vitaGL/source"
    selected = (("gxm.c", "init_depth_stencil_buffer"),
                ("vgl.c", "vgl_reserve_data_pool"),
                ("buffers.c", "glNamedBufferData"))
    bodies = []
    for filename, name in selected:
        path = provider / filename
        bodies.append(function(path.read_text(), name, path))
    with tempfile.TemporaryDirectory(prefix="strikers-allocation-") as scratch:
        temp = pathlib.Path(scratch)
        (temp / "vitagl_allocation_provider.inc").write_text("\n\n".join(bodies))
        binary = temp / "allocation-test"
        subprocess.run([args.compiler, "-std=c++20", "-O1", "-UNDEBUG",
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "-I", str(temp), "-I", str(provider),
                        str(root / "tests/vitagl_allocation_failure_test.cpp"),
                        "-o", str(binary)], check=True, timeout=60)
        subprocess.run([str(binary), args.case], check=True, timeout=30)


if __name__ == "__main__":
    main()
