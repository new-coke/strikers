// Appended to the actual generated Cg programs and original GLX setup by
// test_scene_movie_sampling.py. Synthetic I8 planes keep the CTest asset-free.
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    assert(argc == 1 || argc == 4);
    const unsigned width = argc == 4 ? unsigned(std::stoul(argv[2])) : 64;
    const unsigned height = argc == 4 ? unsigned(std::stoul(argv[3])) : 48;
    std::vector<uint8_t> bytes;
    if (argc == 4)
    {
        std::ifstream input(argv[1], std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(input), {});
    }
    else
    {
        bytes.resize(width * height * 3 / 2);
        for (unsigned y=0;y<height;++y)
            for (unsigned x=0;x<width;++x)
                bytes[y*width+x]=uint8_t(31+(13*x+7*y)%190);
        for (unsigned y=0;y<height/2;++y)
            for (unsigned x=0;x<width/2;++x)
            {
                const unsigned index=y*(width/2)+x;
                bytes[width*height+index]=uint8_t(80+(29*x+7*y)%94);
                bytes[width*height*5/4+index]=uint8_t(63+(11*x+19*y)%129);
            }
    }
    assert(bytes.size() == width * height * 3 / 2);
    std::array<sampler2D,6> planes{};
    planes[0]={make4(0,0,0,0),bytes.data(),width,height,false};
    planes[1]={make4(0,0,0,0),bytes.data()+width*height,width/2,height/2,false};
    planes[2]={make4(0,0,0,0),bytes.data()+width*height*5/4,width/2,height/2,false};
    for (unsigned i=3;i<6;++i) planes[i].value=make4(1,1,1,1);

    glModelPacket packet{{7,prog_2d_movie}};
    const auto coordinateMap=[&](bool movie) {
        glx_program=movie?prog_2d_movie:0;
        glx_SwitchTexConfig(&packet);
        std::array<int,3> coordinates{-1,-1,-1};
        for (int stage=0;stage<tevCount;++stage)
            if (tev[stage].map>=0 && tev[stage].map<3)
                coordinates[tev[stage].map]=tev[stage].coord;
        return coordinates;
    };
    const auto movieCoordinates=coordinateMap(true),standardCoordinates=coordinateMap(false);
    assert((movieCoordinates==std::array<int,3>{0,1,1}));
    assert((standardCoordinates==std::array<int,3>{0,1,2}));

    const auto render=[](auto fragment,float family,const std::array<sampler2D,6>& images,
                         float3 uv0,float3 uv1,float3 uv2) {
        const float4 zero=make4(0,0,0,0),one=make4(1,1,1,1);
        const float3 unused=make3(0,0,1);
        return fragment(one,zero,uv0,uv1,uv2,unused,unused,unused,0,
                        images[0],images[1],images[2],images[3],images[4],images[5],
                        make4(7,-1,1,family),make4(0,-1,1,0),one,one,zero,zero,zero,zero,one);
    };
    const auto differs=[](float4 a,float4 b) {
        for(unsigned c=0;c<4;++c) if(std::fabs(a[c]-b[c])>.000002f)return true;
        return false;
    };
    uint64_t movieChecks=0,movieErrors=0,invarianceChecks=0,invarianceErrors=0;
    uint64_t standardChecks=0,standardErrors=0,standardSensitive=0;
    for(bool affine:{false,true})
        for(bool linear:{false,true})
        {
            const auto generic=affine?movie_affine:movie_projective;
            const auto specialized=affine?standard_affine:standard_projective;
            for(unsigned i=0;i<3;++i) planes[i].linear=linear;
            for(unsigned y=0;y<height;++y)
                for(unsigned x=0;x<width;++x)
                {
                    const float3 uv0=make3((x+.5f)/width,(y+.5f)/height,1);
                    // Equal streams reproduce the live movie packet. Every
                    // other pixel also proves that Cr follows UV1, not UV0.
                    const float3 uv1=x&1?make3(1.f-uv0.x,uv0.y,1):uv0;
                    const float3 corner=make3(0,0,1),other=make3(.75f,.25f,1);
                    const auto expected=[&](bool movie,float3 uv2) {
                        const std::array<float3,3> coordinates{uv0,uv1,uv2};
                        const auto& mapping=movie?movieCoordinates:standardCoordinates;
                        auto samples=planes;
                        for(unsigned map=0;map<3;++map)
                        {
                            const float3 coord=coordinates[mapping[map]];
                            const float2 xy=affine?float2(coord.xy):coord.xy/std::max(std::abs(coord.z),.000001f);
                            samples[map].value=tex2D(planes[map],xy);
                            samples[map].pixels=nullptr;
                        }
                        // Use constant samples to hold the native colour
                        // algebra fixed while independently selecting the
                        // source coordinates from the original TEV stages.
                        return render(generic,movie?2.f:0.f,samples,corner,corner,corner);
                    };
                    const auto actual=render(generic,2,planes,uv0,uv1,corner);
                    ++movieChecks; movieErrors+=differs(actual,expected(true,corner));
                    ++invarianceChecks;
                    invarianceErrors+=differs(actual,render(generic,2,planes,uv0,uv1,other));
                    if(!affine)
                    {
                        ++invarianceChecks;
                        invarianceErrors+=differs(actual,render(generic,2,planes,uv0,2.f*uv1,corner));
                    }
                    for(float3 uv2:{corner,other})
                    {
                        const auto originalSamples=expected(false,uv2);
                        ++standardChecks;
                        standardErrors+=differs(render(generic,0,planes,uv0,uv1,uv2),originalSamples);
                        // A specialized program must ignore the generic
                        // family selector as well as retain coordinate 2.
                        ++standardChecks;
                        standardErrors+=differs(render(specialized,2,planes,uv0,uv1,uv2),originalSamples);
                    }
                    standardSensitive+=differs(render(specialized,2,planes,uv0,uv1,corner),
                                               render(specialized,2,planes,uv0,uv1,other));
                }
        }
    std::printf("{\"fixture\":\"%s\",\"width\":%u,\"height\":%u,"
                "\"movie_checks\":%llu,\"movie_errors\":%llu,"
                "\"invariance_checks\":%llu,\"invariance_errors\":%llu,"
                "\"standard_checks\":%llu,\"standard_errors\":%llu,"
                "\"standard_uv2_sensitive_pixels\":%llu}\n",
                argc==4?"yuv420":"synthetic-i8",width,height,
                (unsigned long long)movieChecks,(unsigned long long)movieErrors,
                (unsigned long long)invarianceChecks,(unsigned long long)invarianceErrors,
                (unsigned long long)standardChecks,(unsigned long long)standardErrors,
                (unsigned long long)standardSensitive);
    assert(standardSensitive>0);
    return movieErrors||invarianceErrors||standardErrors?1:0;
}
