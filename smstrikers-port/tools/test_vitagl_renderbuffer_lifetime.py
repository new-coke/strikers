#!/usr/bin/env python3
"""Run the shipped VitaGL renderbuffer entry points with a checked GPU allocator.

Only the narrow provider API is extracted so this lifetime regression does not
require Vita SDK headers or attempt to replace its GXM rendering implementation.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile


def function(source, name, path):
    match = re.search(r"^[A-Za-z_][^\n;{}]*\b" + re.escape(name) + r"\([^;{]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError("provider function not found: " + name)
    opening = source.index("{", match.start())
    depth = 0
    branches = []
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'
                        r'^[ \t]*#(?:if(?:n?def)?|elif|else|endif)\b[^\n]*|\{|\}', re.S | re.M)
    for token in tokens.finditer(source, opening):
        value = token.group()
        if value.lstrip().startswith("#"):
            directive = value.lstrip()[1:].split()[0]
            if directive in ("if", "ifdef", "ifndef"):
                branches.append((depth, []))
            elif directive in ("else", "elif"):
                start, ends = branches[-1]
                ends.append(depth)
                depth = start
            else:
                _, ends = branches.pop()
                if any(end != depth for end in ends):
                    raise ValueError("inconsistent provider branch braces: " + name)
        elif value == "{":
            depth += 1
        elif value == "}":
            depth -= 1
            if not depth and not branches:
                line = source.count("\n", 0, match.start()) + 1
                return f'#line {line} "{path.as_posix()}"\n' + source[match.start():token.end()]
    raise ValueError("unbalanced provider function: " + name)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--provider-file", type=pathlib.Path)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[1]
    provider = args.provider_file or root / "extern/vitaGL/source/framebuffers.c"
    source = provider.read_text()
    names = ("glGenRenderbuffers", "glDeleteRenderbuffers", "glBindFramebuffer",
             "glBindRenderbuffer", "glFramebufferRenderbuffer", "glRenderbufferStorage",
             "glNamedRenderbufferStorage")
    bodies = "\n\n".join(function(source, name, provider) for name in names)
    with tempfile.TemporaryDirectory(prefix="strikers-renderbuffer-") as scratch:
        temp = pathlib.Path(scratch)
        (temp / "vitagl_renderbuffer_provider.inc").write_text(bodies)
        binary = temp / "renderbuffer-test"
        subprocess.run([args.compiler, "-std=c++20", "-O1", "-UNDEBUG",
                        "-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                        "-I", str(temp), str(root / "tests/vitagl_renderbuffer_lifetime_test.cpp"),
                        "-o", str(binary)], check=True, timeout=60)
        subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
