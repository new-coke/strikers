// CTest needs no output environment. The standalone gate can keep its explicit
// paths for writer/parser evidence; otherwise own one unique temporary directory.
struct FrameTimelineTestOutputs
{
 std::filesystem::path temporary;
 bool timeline=false,net=false;
 FrameTimelineTestOutputs()
 {
  timeline=!std::getenv("TIMELINE_GATE_OUTPUT");
  net=!std::getenv("NET_GATE_OUTPUT");
  if(!timeline&&!net)return;
  const auto pattern=(std::filesystem::temp_directory_path()/"strikers-frame-timeline-XXXXXX").string();
  std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
  const auto* created=mkdtemp(name.data());assert(created);temporary=created;
  if(timeline)assert(setenv("TIMELINE_GATE_OUTPUT",temporary.c_str(),1)==0);
  if(net)assert(setenv("NET_GATE_OUTPUT",(temporary/"nets").c_str(),1)==0);
 }
 ~FrameTimelineTestOutputs()
 {
  if(timeline)unsetenv("TIMELINE_GATE_OUTPUT");
  if(net)unsetenv("NET_GATE_OUTPUT");
  if(!temporary.empty())std::filesystem::remove_all(temporary);
 }
};
static auto material_fields_for_timeline(const port::render::SceneMaterial& m)
{
 return std::tie(m.textures,m.wrapU,m.wrapV,m.filter,m.tint,m.ambient,m.diffuse,
  m.controls,m.effects,m.shadow,m.shadowAdd,m.fog,m.fogColor,m.specular,m.specularColor,
  m.material,m.alpha,m.lightPosition,m.lightColor,m.bones,m.modelView,m.viewport,m.scissor,
  m.environment,m.lit);
}
static auto raster_fields_for_timeline(const port::render::Raster& r)
{
 return std::tie(r.depthTest,r.depthWrite,r.depthFunction,r.cull,r.blend,r.writeRgb,r.writeAlpha,r.primitive);
}
static void check_frame_timeline_runtime()
{
 namespace match=port::directmatch;
 using Access=port::directmatch::detail::MatchRuntimeTestAccess;
 using P=port::render::PacketProfile;
 const auto output=std::filesystem::path(std::getenv("TIMELINE_GATE_OUTPUT"));
 match::shutdown();Access::reset_profile();match::enable(true);
 unsetenv("STRIKERS_VITAGL_PACKET_PROFILE");unsetenv("STRIKERS_VITAGL_NET_CONVERSION");
 netCapture={};
 auto clocks=netClockCalls;
 assert(match::begin_frame()&&!lastBeginTimeline&&match::end_frame());
 assert(netClockCalls==clocks);
 match::shutdown();
 assert(Access::profile().configure((output/"runtime-material.csv").c_str(),6));
 netCapture={PORT_CAPTURE_COLLECTING,0,6,1,0,700};
 assert(match::begin_frame()&&!lastBeginTimeline&&match::end_frame());
 assert(netClockCalls==clocks);
 PlatTexture texture;texture.m_Width=texture.m_Height=8;texture.m_OwnerTextureLevel=3;
 texture.m_Format=GXTex_RGBA8;
 port::render::TextureUpload upload;upload.width=upload.height=8;
 auto* oldFile=fileTexture;fileTexture=&texture;
 nlMatrix4 model;identity(model);model.e[12]=3;
 struct Points { uint32_t count; GLLightUserData values[7]; } points{};
 struct Directions { uint32_t count; GLDirectionalLightUserData values[5]; } directions{};
 struct Specs { uint32_t count; GLSpecularUserData values[5]; } specs{};
 points.count=7;directions.count=3;specs.count=1;
 for(auto& light:points.values) {light.outerRadius=10;light.intensity=.5f;light.colour.c[0]=1;}
 for(auto& light:directions.values) {light.direction.x=1;light.colour.c[1]=1;}
 for(auto& light:specs.values) {light.worldDirection.x=1;light.colour.c[2]=1;light.intensity=.5f;light.exponent=8;}
 points.values[4].intensity=std::numeric_limits<float>::quiet_NaN();
 std::array<uintptr_t,GLUD_Num> userdata{};
 GLViewportUserData viewport{};
 viewport.view=reinterpret_cast<uintptr_t>(&viewMatrix);
 viewport.projection=reinterpret_cast<uintptr_t>(&projectionMatrix);
 viewport.w=640;viewport.h=448;
 for(unsigned test=0;test<6;++test) {
  identity(viewMatrix);identity(projectionMatrix);viewState.vpWidth=640;viewState.vpHeight=448;
  userdata.fill(0);profileOwnedMatrix=nullptr;
  glModelPacket packet{};packet.state.matrix=1;
  packet.state.program=fixture_program("3d pointlit");
  packet.userData=reinterpret_cast<uintptr_t>(userdata.data());
  if(test==1) {
   profileOwnedMatrix=&model;packet.state.matrix=reinterpret_cast<uintptr_t>(&model);
   userdata[GLUD_Viewport]=reinterpret_cast<uintptr_t>(&viewport);
   userdata[GLUD_Light]=reinterpret_cast<uintptr_t>(&points);
   userdata[GLUD_DirectionalLight]=reinterpret_cast<uintptr_t>(&directions);
   userdata[GLUD_Specular]=reinterpret_cast<uintptr_t>(&specs);
  }
  if(test==2||test==3) {
   packet.state.texconfig=63;
   for(auto& id:packet.state.texture)id=77;
   if(test==3)packet.state.texture[2]=0;
  }
  if(test==4)projectionMatrix.e[0]=std::numeric_limits<float>::quiet_NaN();
  netCapture={PORT_CAPTURE_COLLECTING,test,6,0,0,700+test};
  assert(match::begin_frame());
  assert(lastBeginTimeline==Access::profile().capture_timeline()&&lastBeginTimeline);
  if(test==2)assert(match::admit_texture(&texture,upload));
  unsigned lookups=materialTextureLookups;
  if(test<5) {
   Access::Material plain,measured;
   clocks=netClockCalls;
   const bool expected=Access::material(GLV_Characters,packet,plain,nullptr);
   assert(expected==(test!=3&&test!=4));
   assert(netClockCalls==clocks);
   lookups=materialTextureLookups;
   {
    P::Scope scope(Access::profile(),GLV_Characters,3,nullptr);
    scope.next(P::Material);
    assert(Access::material(GLV_Characters,packet,measured,&scope)==expected);
    scope.next(P::Geometry);
   }
   assert(material_fields_for_timeline(plain.scene)==material_fields_for_timeline(measured.scene));
   assert(raster_fields_for_timeline(plain.raster)==raster_fields_for_timeline(measured.raster));
   // Only the matrix's defined float elements, including a preserved NaN bit pattern; no padding oracle.
   assert(std::memcmp(plain.matrix.data(),measured.matrix.data(),plain.matrix.size()*sizeof(float))==0);
   assert(plain.matrixOwned==measured.matrixOwned);
  } else {
   float positions[]{0,0,0,1,0,0,0,1,0};uint16_t indices[]{0,1,2};
   glModelStream stream{reinterpret_cast<uintptr_t>(positions),0,12,0,sizeof positions};
   packet.numVertices=3;packet.numStreams=1;packet.streams=&stream;
   packet.indexBuffer=reinterpret_cast<uintptr_t>(indices);
   const auto old=arenaOwns;arenaOwns=false;
   const auto draws=submitted.size();match::on_packet(GLV_Characters,0,&packet);
   assert(submitted.size()==draws+1);arenaOwns=old;
  }
  assert(match::end_frame()&&!lastBeginTimeline);
  netCapture.samples=test+1;netCapture.lastFrame=701+test;
  if(test==5)netCapture.state=PORT_CAPTURE_COMPLETE;
  match::finish_packet_profile_frame();
  const auto& row=Access::profile().samples().back();
  const auto& n=row.materials.counts;
  assert(n[P::MaterialAttempts]==1&&n[P::ModelConcats]==unsigned(test==1));
  assert(n[P::ViewOnlyMatrices]==unsigned(test!=1)&&n[P::InvalidMatrices]==unsigned(test==4));
  assert(n[P::ViewportOverrides]==unsigned(test==1)&&n[P::ViewportMatrixOverrides]==unsigned(test==1));
  assert(n[P::PointLights]==(test==1?4u:0u)&&n[P::DirectionalLights]==(test==1?3u:0u)&&n[P::SpecularLights]==unsigned(test==1));
  assert(n[P::TextureRequests]==(test==2?6u:test==3?3u:0u));
  assert(n[P::TextureResolutions]==(test==2?6u:test==3?2u:0u)&&n[P::TextureFailures]==unsigned(test==3));
  assert(materialTextureLookups-lookups==n[P::TextureRequests]);
  assert(row.materials.ns[0]==100&&row.materials.ns[1]==(test==4?0u:100u)&&row.materials.ns[2]==(test==4?0u:100u));
  assert(row.producer.ns[P::Material]==row.materials.ns[0]+row.materials.ns[1]+row.materials.ns[2]);
 }
 assert(!Access::profile().output_error());
 clocks=netClockCalls;
 assert(match::begin_frame()&&!lastBeginTimeline&&match::end_frame());
 assert(netClockCalls==clocks);
 match::shutdown();Access::reset_profile();netCapture={};profileOwnedMatrix=nullptr;fileTexture=oldFile;
 identity(viewMatrix);identity(projectionMatrix);
 std::puts("Material timeline runtime: real body/packet hooks, semantic equality, exact child clocks, actual capped light/texture counts, failures and quiet lifetime passed");
}
