// Appended after the original GLX configuration/program switch and shipped Cg
// fragment by tools/test_scene_materials.py. This is real-number stage algebra,
// with the original 8-bit konst inputs; it does not emulate GX per-stage rounding.
int main()
{
    using namespace port::render;
    constexpr unsigned masks[]{0,1,3,5,7,9,13,17,19,21,25,33,35,37,39,41,43,49,53,57};
    struct Config { unsigned mask; SceneAlphaProgram program; };
    std::vector<Config> configs;
    for(unsigned mask:masks) configs.push_back({mask,SceneAlphaProgram::Standard});
    configs.push_back({35,SceneAlphaProgram::Dirt});
    configs.push_back({1,SceneAlphaProgram::Crowd});
    configs.push_back({33,SceneAlphaProgram::Crowd});
    configs.push_back({7,SceneAlphaProgram::Movie});
    SceneAlphaState state;
    unsigned originalDirty=0;
    const auto reset=[&] {
        tev={};tevCount=0;glx_program=-1;glx_tevscale=0;state={};originalDirty=0;
    };
    const auto configure=[&](Config config,unsigned flags,float scale) {
        const int program=config.program==SceneAlphaProgram::Movie?prog_2d_movie:
            config.program==SceneAlphaProgram::Dirt?prog_3d_pointlit_dirt:
            config.program==SceneAlphaProgram::Crowd?prog_3d_crowd:0;
        glModelPacket packet{{config.mask,unsigned(program)}};
        const auto originalFlags=flags|originalDirty;
        originalDirty=0;
        if(originalFlags&2)glx_SwitchProgram(&packet);
        glx_tevscale=scale==2?1:0;
        if(originalFlags&0x80)glx_SwitchTexConfig(&packet);
        state.observe(flags,config.mask,config.program);
    };
    std::mt19937 random(0x54455631);
    std::uniform_int_distribution<unsigned> byte(0,255),level(0,63);
    const auto unit=[&] { return byte(random)/255.f; };
    const auto color=[&] { return make4(unit(),unit(),unit(),unit()); };
    unsigned checks=0,alphaChecks=0,failures=0;
    std::array<std::array<unsigned,4>,64> failuresByMask{};
    const auto check=[&](Config config,unsigned trial,float scale) {
        if(config.program==SceneAlphaProgram::Movie)return;
        const unsigned mask=config.mask;
        const bool dirt=config.program==SceneAlphaProgram::Dirt;
        const bool constantColor=trial%5==1,noRasterAlpha=trial%7==2;
        const bool environmentDiffuse=trial%11==4&&(mask&16);
        const float translucent=trial%3==0?unit():-1.f;
        float4 raster=color(),spec=color(),tint=color();
        spec.a=0.f;
        std::array<sampler2D,6> textures;
        for(auto& texture:textures)texture.value=color();
        const float diffuseLevel=level(random)*(1.f/63.f);
        const float shadowLevel=level(random)*(1.f/63.f);
        const float glossLevel=level(random)*(1.f/63.f);
        const auto originalKonst=[](float value) { return int(255.5f*value)/255.f; };
        std::array<float,4> k{originalKonst(diffuseLevel),originalKonst(shadowLevel),
                              originalKonst(1.f-shadowLevel),originalKonst(glossLevel)};
        const auto expected=original_tev(textures,raster,spec,k,tint,translucent,constantColor,noRasterAlpha,environmentDiffuse);
        const float blend=scene_material_konst(diffuseLevel),shadow=scene_material_konst(shadowLevel);
        const float inverseShadow=scene_material_konst(1.f-shadowLevel);
        const float gloss=scene_material_konst(glossLevel);
        const auto resolved=state.resolve(blend,shadow);
        const float overrideAlpha=noRasterAlpha?1.f:translucent>=0?(constantColor?tint.a:translucent):-1.f;
        if(environmentDiffuse)spec=make4(raster.a,raster.a,raster.a,raster.a);
        if(overrideAlpha>=0)raster.a=overrideAlpha;
        else if(constantColor&&mask==1)raster.a=tint.a;
        const float4 controls{float(mask),-1.f,mask&32?scale:1.f,0.f};
        const float4 effects{float(constantColor),overrideAlpha,blend,0.f};
        const float4 shadowColor{shadow,shadow,shadow,shadow};
        const float4 shadowAdd{inverseShadow,inverseShadow,inverseShadow,inverseShadow};
        const float4 material{gloss,float(dirt),0,float(state.stage1RasterColor&&!(mask==17&&environmentDiffuse))};
        const float4 alpha{resolved[0],resolved[1],resolved[2],resolved[3]};
        const float3 uv{.5f,.5f,1.f};
        const auto actual=native_fragment(raster,spec,uv,uv,uv,uv,uv,uv,0.f,
            textures[0],textures[1],textures[2],textures[3],textures[4],textures[5],
            controls,effects,tint,shadowColor,shadowAdd,make4(0,0,0,0),make4(0,0,0,0),material,alpha);
        const auto minimum=scene_material_minimum_alpha(mask,raster.a,textures[0].value.a,
            textures[1].value.a,blend,overrideAlpha,dirt,resolved,textures[2].value.a,textures[4].value.a);
        assert(minimum<=actual.a+0.000002f);
        for(unsigned reference:{0u,64u,128u,254u,255u})
            assert(!(minimum>reference/255.f)||actual.a>reference/255.f);
        for(unsigned channel=0;channel<4;++channel)
        {
            ++checks;
            alphaChecks+=channel==3;
            if(std::fabs(expected[channel]-actual[channel])>0.000002f)
            {
                ++failures;
                if(failuresByMask[mask][channel]++==0)
                    std::fprintf(stderr,"mask=0x%02x program=%u trial=%u channel=%u expected=%.7f actual=%.7f state=%.3f/%.3f/%.3f/%.0f override=%.3f constant=%u noRaster=%u env=%u cleanup=%.0f\n",
                        mask,unsigned(config.program),trial,channel,expected[channel],actual[channel],
                        resolved[0],resolved[1],resolved[2],resolved[3],overrideAlpha,unsigned(constantColor),unsigned(noRasterAlpha),unsigned(environmentDiffuse),material.w);
            }
        }
        glx_norasterizedalpha=noRasterAlpha;
        glx_translucent=translucent>=0;
        glx_constantcolour=constantColor;
        original_finish_draw();
        if(environmentDiffuse&&glx_GlossMapStage>=0)originalDirty|=0x80;
        state.note_draw(mask,constantColor,translucent>=0||noRasterAlpha,environmentDiffuse);
    };
    // RGB and alpha for every explicit configuration, including cold-start
    // 0x15/0x25 where stage 2 still contains GXInit's APREV input fields.
    for(const auto config:configs)
        for(unsigned trial=0;trial<512;++trial)
        {
            reset();const float scale=trial&1?2.f:1.f;
            configure(config,0x82,scale);check(config,trial,scale);
        }
    // Every predecessor pair, including movie enter/exit, dirt and crowd;
    // then the same config with only texture state changed (no config setter).
    for(const auto before:configs)
        for(const auto current:configs)
        {
            reset();configure(before,0x82,2.f);check(before,1,2.f);
            for(unsigned trial=0;trial<32;++trial)
            {
                unsigned flags=0x10;
                if(trial==0&&current.program!=before.program)flags|=0x82;
                if(trial==0&&current.mask!=before.mask)flags|=0x80;
                // Explicit refreshes must also clear inherited RGB cleanup.
                if(trial==16)flags|=0x80;
                configure(current,flags,2.f);
                check(current,trial,2.f);
                configure(current,0x10,2.f);
                check(current,trial+32,2.f);
            }
        }
    // Long histories catch selectors inherited across more than one unused
    // stage/configuration; every third draw deliberately skips both setters.
    reset();Config previous=configs[0];configure(previous,0x82,2.f);
    for(unsigned trial=0;trial<10000;++trial)
    {
        const auto current=trial%3?configs[random()%configs.size()]:previous;
        unsigned flags=0x10;
        if(current.program!=previous.program)flags|=0x82;
        if(current.mask!=previous.mask)flags|=0x80;
        configure(current,flags,2.f);check(current,trial,2.f);previous=current;
    }
    for(unsigned mask:masks)
        if(failuresByMask[mask]!=std::array<unsigned,4>{})
            std::fprintf(stderr,"0x%02x mismatches RGBA=%u/%u/%u/%u\n",mask,failuresByMask[mask][0],failuresByMask[mask][1],failuresByMask[mask][2],failuresByMask[mask][3]);
    std::printf("Original TEV oracle: %u comparisons (%u alpha), %u failures; 20 RGB/alpha masks, predecessor pairs and persistent sequences\n",checks,alphaChecks,failures);
    return failures?1:0;
}
