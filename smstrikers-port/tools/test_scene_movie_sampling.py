#!/usr/bin/env python3
"""Check movie UV routing in generated Cg against the original GLX TEV setup.

The default fixture is synthetic I8 data; no game assets or FFmpeg are needed.
--yuv accepts a planar full-range 4:2:0 frame for an additional asset regression.
The existing native colour equation is held fixed so this isolates coordinates,
not GX stage quantization or GPU filtering precision.
"""
import argparse
import pathlib
import re
import subprocess
import tempfile

from test_scene_materials import function


def host_fragment(source, name):
    # This oracle isolates movie UVs with fog disabled. Newer shader sources
    # also carry range-fog registers/WPOS; supply inert fixture values without
    # changing the movie sampling or colour expressions under test.
    if "float4 fragmentPosition:WPOS," in source:
        source = source.replace(" float4 fragmentPosition:WPOS,\n", "")
        source = source.replace(",\n uniform float4 u_fogrange[3],uniform float4 u_fograngecontrol", "")
        source = source.replace("):COLOR {", "):COLOR {\n float4 fragmentPosition=float4(0,0,0,0);"
            "float4 u_fogrange[3]={float4(1,1,1,1),float4(1,1,1,1),float4(1,1,1,1)};"
            "float4 u_fograngecontrol=float4(0,0,0,0);", 1)
    source = re.sub(r"\buniform\s+", "", source)
    source = re.sub(r":(?:COLOR\d*|TEXCOORD\d+|TEXUNIT\d+)\b", "", source)
    source = re.sub(r"\bfloat([234])\(", r"make\1(", source)
    source = re.sub(r"\b(\d+\.\d+)\b(?![fF])", r"\1f", source)
    source = source.replace("float4 main(", f"float4 {name}(")
    source = source.replace("scene_fog_range_pair(", f"{name}_fog_range_pair(")
    source = re.sub(r"\bclamp\(", "std::clamp(", source)
    source = source.replace("discard;", "throw Discard{};")
    return "#undef SCENE_UV\n#undef STRIKERS_AFFINE_UV\n#undef STRIKERS_VERTEX_FOG\n" + source


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--yuv", type=pathlib.Path)
    parser.add_argument("--width", type=int, default=512)
    parser.add_argument("--height", type=int, default=416)
    args = parser.parse_args()
    if args.yuv and (args.width <= 0 or args.height <= 0 or args.width % 2 or args.height % 2):
        parser.error("--yuv requires positive even dimensions")
    root = pathlib.Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="strikers-movie-sampling-") as directory:
        scratch = pathlib.Path(directory)
        generator = scratch / "generate.cpp"
        generator.write_text(r'''#include "port/scene_shader_variants.hpp"
#include <iostream>
int main(int argc,char**) {
    const unsigned variant=unsigned(argc-1);
    std::cout<<port::render::scene_fragment_shader(7,true,variant<2,variant%2,false);
}
''')
        generate = scratch / "generate"
        flags = [args.compiler, "-std=c++20", "-O2", "-ffp-contract=off",
                 "-Wno-unused-value", "-I", str(root / "include")]
        subprocess.run(flags + [str(generator), "-o", str(generate)], check=True, timeout=60)
        names = ["movie_projective", "movie_affine", "standard_projective", "standard_affine"]
        fragments = []
        for variant, name in enumerate(names):
            source = subprocess.check_output([str(generate)] + ["option"] * variant, text=True, timeout=10)
            if variant >= 2:
                assert "u_controls.w==" not in source, "standard shader retained a dynamic movie branch"
            fragments.append(host_fragment(source, name))
        prelude = (root / "tests/scene_tev_oracle.hpp").read_text()
        constant_sampler = ("struct sampler2D { float4 value; };\n"
                            "float4 tex2D(sampler2D sampler, float2) { return sampler.value; }")
        coordinate_sampler = r'''struct sampler2D {
    float4 value{}; const uint8_t* pixels=nullptr;
    unsigned width=0,height=0; bool linear=false;
};
float4 tex2D(sampler2D s,float2 uv) {
    if(!s.pixels)return s.value;
    const auto at=[&](int x,int y) {
        return s.pixels[std::clamp(y,0,int(s.height)-1)*s.width+std::clamp(x,0,int(s.width)-1)]/255.f;
    };
    float value;
    if(!s.linear)value=at(int(std::floor(uv.x*s.width)),int(std::floor(uv.y*s.height)));
    else {
        const float x=uv.x*s.width-.5f,y=uv.y*s.height-.5f;
        const int x0=int(std::floor(x)),y0=int(std::floor(y));
        value=lerp(lerp(at(x0,y0),at(x0+1,y0),x-x0),
                   lerp(at(x0,y0+1),at(x0+1,y0+1),x-x0),y-y0);
    }
    return make4(value,value,value,value);
}'''
        assert prelude.count(constant_sampler) == 1
        prelude = prelude.replace(constant_sampler, coordinate_sampler)
        glx = (root / "src/NL/glx/glxSend.cpp").read_text()
        original = function(glx, "static unsigned long glx_SwitchTexConfig(")
        source = scratch / "sampling.cpp"
        source.write_text(prelude + "\n" + original + "\n" + "\n".join(fragments) + "\n" +
                          (root / "tests/scene_movie_sampling_test.cpp").read_text())
        binary = scratch / "sampling-test"
        subprocess.run(flags + [str(source), "-o", str(binary)], check=True, timeout=60)
        command = [str(binary)]
        if args.yuv:
            command += [str(args.yuv.resolve()), str(args.width), str(args.height)]
        return subprocess.run(command, timeout=60).returncode


if __name__ == "__main__":
    raise SystemExit(main())
