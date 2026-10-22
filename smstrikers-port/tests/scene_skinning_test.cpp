// Appended to the actual PN and generated rigid Cg blocks by tools/test_scene_skinning.py.
// The expected palette uses original stitch/userdata equations independently
// of the native runtime helpers. Both decoder modes must preserve that mapping.
namespace
{
using Matrix = std::array<float,12>;
double original_component(const Matrix& bone, const std::array<float,4>& input,
                          unsigned row, bool normal)
{
    double result = 0;
    for (unsigned c = 0; c < (normal ? 3u : 4u); ++c)
        result += double(bone[row*4+c]) * double(input[c]);
    return result;
}
void near(float actual,double expected)
{
    assert(std::isfinite(actual));
    assert(std::fabs(double(actual)-expected) <= .00002 * std::max(1.0,std::fabs(expected)));
}
void check_rigid(float4 position, float3 normal, const std::array<float4,shaderCapacity>& palette)
{
    const auto pn = shader_skin(position,normal,0.f,palette);
    const auto rigid = shader_rigid(position,normal);
    for (unsigned i = 0; i < 4; ++i)
    {
        assert(std::bit_cast<uint32_t>(pn.position[i]) == std::bit_cast<uint32_t>(rigid.position[i]));
        assert(std::bit_cast<uint32_t>(position[i]) == std::bit_cast<uint32_t>(rigid.position[i]));
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        assert(std::bit_cast<uint32_t>(pn.normal[i]) == std::bit_cast<uint32_t>(rigid.normal[i]));
        assert(std::bit_cast<uint32_t>(normal[i]) == std::bit_cast<uint32_t>(rigid.normal[i]));
    }
}
}
int main()
{
    using namespace port::render;
    using namespace port::nativegxm;
    static_assert(kSceneBoneSlots == 33 && kSceneBoneRows == 99);
    SceneMaterial material;
    assert(material.bones.size() == shaderCapacity);
    std::mt19937 random(0x534b494e);
    std::uniform_real_distribution<float> value(-2.f,2.f);
    unsigned checkedVertices = 0, rigidVertices = 0;
    for (unsigned pose = 0; pose < 64; ++pose)
    {
        std::array<Matrix,kSceneBoneSlots> original{};
        original[0] = {1,0,0,0, 0,1,0,0, 0,0,1,0};
        std::array<BoneTransform,kSceneSkinBoneCount> bones;
        std::array<float4,shaderCapacity> shaderRows;
        // Slot zero must bypass the palette. Poison it to detect accidental
        // selection of bone zero or an unskinned matrix read in the shader.
        for (auto& row : shaderRows) row = {111,222,333,444};
        for (unsigned i = 0; i < kSceneSkinBoneCount; ++i)
        {
            auto& matrix = original[i+1];
            for (auto& x : matrix) x = value(random);
            matrix[3] += float(i*5);
            matrix[7] -= float(i*3);
            matrix[11] += float(i*7);
            const int registerIndex = int(i)*3-96; // UserDataBuilder::AddEntry
            const unsigned firstRow = unsigned(registerIndex+99); // glud_Skin
            bones[i].selector = firstRow;
            bones[i].matrix = matrix;
            for (unsigned row = 0; row < 3; ++row)
            {
                const auto* m = matrix.data()+row*4;
                shaderRows[firstRow+row] = {m[0],m[1],m[2],m[3]};
                std::copy(m,m+4,material.bones[firstRow+row].begin());
            }
        }
        for (unsigned slot = 0; slot < kSceneBoneSlots; ++slot)
        {
            std::array<std::array<float,4>,3> positions;
            std::array<std::array<float,3>,3> normals;
            for (unsigned vertex = 0; vertex < 3; ++vertex)
            {
                positions[vertex] = {value(random),value(random),value(random),float(vertex)*.5f};
                normals[vertex] = {value(random),value(random),value(random)};
            }
            ModelStreams streams;
            streams.position = {{reinterpret_cast<const uint8_t*>(positions.data()),sizeof positions},16,true};
            streams.normal = {{reinterpret_cast<const uint8_t*>(normals.data()),sizeof normals},12,true};
            const std::array<uint16_t,3> indices{0,1,2};
            // StitchModel emits 3*(i+1), while an ordinary unskinned packet
            // has no PN selector stream and keeps ModelVertex::boneSlot zero.
            std::array<uint8_t,3> selectors;
            selectors.fill(uint8_t(slot*3));
            const std::span<const uint8_t> selected = slot ? std::span<const uint8_t>(selectors)
                                                         : std::span<const uint8_t>{};
            ModelVertices cpu,gpu;
            assert(prepare_model_vertices(streams,indices,3,0,selected,bones,cpu,true,true));
            assert(prepare_model_vertices(streams,indices,3,0,selected,bones,gpu,true,false));
            assert(cpu.vertices.size()==3 && gpu.vertices.size()==3);
            for (unsigned vertex = 0; vertex < 3; ++vertex)
            {
                const auto& v = gpu.vertices[vertex];
                assert(v.boneSlot == float(slot));
                assert(v.position == positions[vertex] && v.normal == normals[vertex]);
                const auto actual = shader_skin(
                    make4(v.position[0],v.position[1],v.position[2],v.position[3]),
                    make3(v.normal[0],v.normal[1],v.normal[2]),v.boneSlot,shaderRows);
                if (!slot)
                {
                    check_rigid(make4(v.position[0],v.position[1],v.position[2],v.position[3]),
                                make3(v.normal[0],v.normal[1],v.normal[2]),shaderRows);
                    ++rigidVertices;
                }
                std::array<float,4> normalInput{v.normal[0],v.normal[1],v.normal[2],0};
                for (unsigned row = 0; row < 3; ++row)
                {
                    const double position = original_component(original[slot],positions[vertex],row,false);
                    const double normal = original_component(original[slot],normalInput,row,true);
                    near(actual.position[row],position);
                    near(actual.normal[row],normal);
                    near(cpu.vertices[vertex].position[row],position);
                    near(cpu.vertices[vertex].normal[row],normal);
                }
                assert(actual.position.w == positions[vertex][3]);
                assert(cpu.vertices[vertex].boneSlot == 0);
                ++checkedVertices;
            }
        }
    }
    // Pure bypass preserves even signed zero, denormals and non-finite input
    // bits. Poison every palette row; neither variant may consult it at slot 0.
    constexpr uint32_t patterns[]{0, 0x80000000, 1, 0x80000001, 0x3f800000, 0xbf800000,
                                  0x7f7fffff, 0x7f800000, 0xff800000, 0x7fc12345};
    std::array<float4,shaderCapacity> poison;
    for (auto& row : poison) row = {std::bit_cast<float>(0x7fc23456u), 222, 333, 444};
    for (unsigned a = 0; a < std::size(patterns); ++a)
        for (unsigned b = 0; b < std::size(patterns); ++b)
        {
            const float x = std::bit_cast<float>(patterns[a]), y = std::bit_cast<float>(patterns[b]);
            check_rigid(make4(x,y,y,x),make3(y,x,y),poison);
            ++rigidVertices;
        }
    std::printf("Scene skinning: %u vertices, all 33 slots, CPU/GPU decoder; %u rigid Cg bypass cases bit-identical\n",
                checkedVertices,rigidVertices);
}
