#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Import/Importer.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"

#include <chrono>
#include <fstream>
#include <rapidjson/document.h>
#include <yaml-cpp/yaml.h>

using namespace Crowny;

namespace
{
    class TestImporter final : public SpecificImporter
    {
    public:
        explicit TestImporter(String extension) : m_Extension(std::move(extension)) {}

        bool IsExtensionSupported(const String& extension) const override
        {
            ++ExtensionChecks;
            return extension == m_Extension;
        }

        bool IsMagicNumSupported(uint8_t*, uint32_t) const override { return false; }
        Ref<Asset> Import(const Path&, Ref<const ImportOptions>) override { return nullptr; }

        mutable uint32_t ExtensionChecks = 0;

    private:
        String m_Extension;
    };

    struct ImporterFixture
    {
        ImporterFixture() : Registry(CreateScope<Importer>()) {}

        Scope<Importer> Registry;
    };

    class TemporaryMeshFile
    {
    public:
        explicit TemporaryMeshFile(StringView contents, StringView extension = "obj")
        {
            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            m_Path = std::filesystem::temp_directory_path() / ("crowny_mesh_" + std::to_string(unique) + "." + String(extension));
            std::ofstream stream(m_Path, std::ios::binary);
            stream << contents;
        }

        ~TemporaryMeshFile()
        {
            std::error_code error;
            std::filesystem::remove(m_Path, error);
        }

        const Path& GetPath() const { return m_Path; }

    private:
        Path m_Path;
    };

    constexpr StringView TRIANGLE_BUFFER_BASE64 = "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/"
                                                  "AACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AAABAAIA";

    String BuildGltfDocument(uint32_t byteLength, StringView encodedBuffer, StringView bufferViews, StringView accessors, StringView meshes,
                             StringView nodes, StringView sceneNodes, StringView extraRoot = {})
    {
        String document = R"GLTF({"asset":{"version":"2.0"},"buffers":[{"byteLength":)GLTF";
        document += std::to_string(byteLength);
        document += R"GLTF(,"uri":"data:application/octet-stream;base64,)GLTF";
        document.append(encodedBuffer.data(), encodedBuffer.size());
        document += R"GLTF("}],"bufferViews":)GLTF";
        document.append(bufferViews.data(), bufferViews.size());
        document += R"GLTF(,"accessors":)GLTF";
        document.append(accessors.data(), accessors.size());
        document += R"GLTF(,"meshes":)GLTF";
        document.append(meshes.data(), meshes.size());
        document += R"GLTF(,"nodes":)GLTF";
        document.append(nodes.data(), nodes.size());
        document.append(extraRoot.data(), extraRoot.size());
        document += R"GLTF(,"scenes":[{"nodes":)GLTF";
        document.append(sceneNodes.data(), sceneNodes.size());
        document += R"GLTF(}],"scene":0})GLTF";
        return document;
    }

    String BuildTriangleGltf(StringView meshes, StringView nodes, StringView sceneNodes)
    {
        return BuildGltfDocument(
          126, TRIANGLE_BUFFER_BASE64,
          R"GLTF([{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":72,"byteLength":48,"target":34962},{"buffer":0,"byteOffset":120,"byteLength":6,"target":34963}])GLTF",
          R"GLTF([{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"}])GLTF",
          meshes, nodes, sceneNodes);
    }
} // namespace

TEST_CASE_METHOD(ImporterFixture, "Importer indexes normalized exact extensions", "[Assets][Importer]")
{
    auto* exact = new TestImporter("png");
    auto* fallback = new TestImporter("dynamic");
    Registry->RegisterImporter(exact, { "png" });
    Registry->RegisterImporter(fallback);

    CHECK(Registry->SupportsFileType("PNG"));
    CHECK(Registry->SupportsFileType(".PnG"));
    CHECK(Registry->GetImporterForFile("asset.PNG") == exact);
    CHECK(exact->ExtensionChecks == 0);
    CHECK(fallback->ExtensionChecks == 0);
}

TEST_CASE("Importer threading policy defaults to main-thread-only", "[Assets][Importer][Threading]")
{
    const TestImporter importer("default");
    CHECK(importer.GetThreadingPolicy() == ImporterThreadingPolicy::MainThreadOnly);
}

TEST_CASE_METHOD(ImporterFixture, "Importer falls back for dynamic extensions", "[Assets][Importer]")
{
    auto* exact = new TestImporter("png");
    auto* fallback = new TestImporter("blend");
    Registry->RegisterImporter(exact, { "png" });
    Registry->RegisterImporter(fallback);

    CHECK(Registry->SupportsFileType("BLEND"));
    CHECK(Registry->GetImporterForFile("mesh.BlEnD") == fallback);
    CHECK(exact->ExtensionChecks == 0);
    CHECK(fallback->ExtensionChecks == 2);
}

TEST_CASE("Mesh parser preserves topology and combines different vertex layouts", "[Assets][Importer][Mesh]")
{
    const TemporaryMeshFile source(R"OBJ(
o Quad
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
vt 0 0.25
vt 1 0.25
vt 1 1
vt 0 1
vn 0 0 1
f 1/1/1 2/2/1 3/3/1 4/4/1
o Line
v 2 0 0
v 2 1 0
l 5 6
)OBJ");

    MeshImportOptions options;
    options.ScaleFactor = 2.0f;
    options.FlipUVs = true;

    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);
    REQUIRE(result);
    REQUIRE(result.Data != nullptr);
    CHECK(result.Data->GetVertexCount() == 6);
    CHECK(result.Data->GetIndexCount() == 8);
    CHECK(result.Data->GetBufferLayout().HasAttribute(VertexAttribute::Normal));
    CHECK(result.Data->GetBufferLayout().HasAttribute(VertexAttribute::TexCoord0));
    REQUIRE(result.SubMeshes.size() == 2);

    bool hasTriangles = false;
    bool hasLines = false;
    for (const SubMesh& subMesh : result.SubMeshes)
    {
        hasTriangles |= subMesh.MeshDrawMode == DrawMode::TRIANGLE_LIST && subMesh.IndexCount == 6;
        hasLines |= subMesh.MeshDrawMode == DrawMode::LINE_LIST && subMesh.IndexCount == 2;
    }
    CHECK(hasTriangles);
    CHECK(hasLines);

    const Vector<glm::vec3> positions = result.Data->GetPositions();
    float maximumX = 0.0f;
    for (const glm::vec3& position : positions)
        maximumX = std::max(maximumX, position.x);
    CHECK_THAT(maximumX, Catch::Matchers::WithinAbs(4.0f, 0.001f));

    const Vector<glm::vec2> uvs = result.Data->GetUVs(0);
    bool foundFlippedCoordinate = false;
    for (const glm::vec2& uv : uvs)
        foundFlippedCoordinate |= glm::epsilonEqual(uv.y, 0.75f, 0.001f);
    CHECK(foundFlippedCoordinate);
}

TEST_CASE("Upstream RapidJSON and Assimp parse transformed scene instances", "[Assets][Importer][Mesh]")
{
    constexpr StringView gltf = R"GLTF({
  "asset": { "version": "2.0" },
  "buffers": [ {
    "byteLength": 126,
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AAABAAIA"
  } ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 36, "target": 34962 },
    { "buffer": 0, "byteOffset": 72, "byteLength": 48, "target": 34962 },
    { "buffer": 0, "byteOffset": 120, "byteLength": 6, "target": 34963 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [ 0, 0, 0 ], "max": [ 1, 1, 0 ] },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3" },
    { "bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4" },
    { "bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "materials": [ { "name": "Shared" } ],
  "meshes": [ {
    "name": "Triangle",
    "primitives": [ {
      "attributes": { "POSITION": 0, "NORMAL": 1, "TANGENT": 2 },
      "indices": 3,
      "material": 0,
      "mode": 4
    } ]
  } ],
  "nodes": [
    { "name": "ScaledParent", "translation": [ 10, 0, 0 ], "children": [ 1 ] },
    { "name": "Scaled", "mesh": 0, "scale": [ 2, 3, 1 ] },
    { "name": "Mirrored", "mesh": 0, "translation": [ -4, 0, 0 ], "scale": [ -1, 2, 1 ] }
  ],
  "scenes": [ { "nodes": [ 0, 2 ] } ],
  "scene": 0
})GLTF";

    rapidjson::Document upstreamDocument;
    upstreamDocument.Parse(gltf.data(), gltf.size());
    REQUIRE_FALSE(upstreamDocument.HasParseError());
    REQUIRE(upstreamDocument.HasMember("meshes"));

    const TemporaryMeshFile source(gltf, "gltf");

    MeshImportOptions options;
    options.ImportMaterials = false;
    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);

    REQUIRE(result);
    REQUIRE(result.Data != nullptr);
    CHECK(result.Data->GetVertexCount() == 6);
    CHECK(result.Data->GetIndexCount() == 6);
    REQUIRE(result.SubMeshes.size() == 2);
    REQUIRE(result.MaterialIndices.size() == result.SubMeshes.size());

    const Vector<glm::vec3> positions = result.Data->GetPositions();
    const Vector<glm::vec3> normals = result.Data->GetNormals();
    const Vector<glm::vec3> tangents = result.Data->GetTangents();
    const Vector<glm::vec3> bitangents = result.Data->GetBitangents();
    const Vector<uint32_t> indices = result.Data->GetIndices();
    REQUIRE(normals.size() == positions.size());
    REQUIRE(tangents.size() == positions.size());
    REQUIRE(bitangents.size() == positions.size());

    bool foundScaled = false;
    bool foundMirrored = false;
    for (uint32_t materialSlot = 0; materialSlot < result.SubMeshes.size(); materialSlot++)
    {
        const SubMesh& subMesh = result.SubMeshes[materialSlot];
        REQUIRE(subMesh.MeshDrawMode == DrawMode::TRIANGLE_LIST);
        REQUIRE(subMesh.IndexCount == 3);
        REQUIRE(subMesh.IndexOffset <= indices.size());
        REQUIRE(subMesh.IndexCount <= indices.size() - subMesh.IndexOffset);
        CHECK(result.MaterialIndices[materialSlot] == 0);

        const uint32_t first = indices[subMesh.IndexOffset];
        const uint32_t second = indices[subMesh.IndexOffset + 1];
        const uint32_t third = indices[subMesh.IndexOffset + 2];
        REQUIRE(first < positions.size());
        REQUIRE(second < positions.size());
        REQUIRE(third < positions.size());

        const glm::vec3 centroid = (positions[first] + positions[second] + positions[third]) / 3.0f;
        glm::vec3 geometricNormal = glm::cross(positions[second] - positions[first], positions[third] - positions[first]);
        geometricNormal = glm::normalize(geometricNormal);
        CHECK_THAT(geometricNormal.z, Catch::Matchers::WithinAbs(1.0f, 0.001f));

        const bool mirrored = centroid.x < 0.0f;
        foundMirrored |= mirrored;
        foundScaled |= !mirrored;
        for (uint32_t vertex : { first, second, third })
        {
            CHECK_THAT(normals[vertex].z, Catch::Matchers::WithinAbs(1.0f, 0.001f));
            CHECK_THAT(tangents[vertex].x, Catch::Matchers::WithinAbs(mirrored ? -1.0f : 1.0f, 0.001f));
            CHECK_THAT(bitangents[vertex].y, Catch::Matchers::WithinAbs(1.0f, 0.001f));
        }
    }
    CHECK(foundScaled);
    CHECK(foundMirrored);

    AABox bounds;
    SphereBounds sphereBounds;
    result.Data->CalculateBounds(bounds, sphereBounds);
    CHECK_THAT(bounds.GetMin().x, Catch::Matchers::WithinAbs(-5.0f, 0.001f));
    CHECK_THAT(bounds.GetMin().y, Catch::Matchers::WithinAbs(0.0f, 0.001f));
    CHECK_THAT(bounds.GetMax().x, Catch::Matchers::WithinAbs(12.0f, 0.001f));
    CHECK_THAT(bounds.GetMax().y, Catch::Matchers::WithinAbs(3.0f, 0.001f));
    CHECK(sphereBounds.GetRadius() > 0.0f);
}

TEST_CASE("Mesh parser rejects only degenerate transformed instances", "[Assets][Importer][Mesh]")
{
    const String gltf =
      BuildTriangleGltf(R"GLTF([{"name":"Triangle","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TANGENT":2},"indices":3,"mode":4}]}])GLTF",
                        R"GLTF([{"name":"Collapsed","mesh":0,"scale":[0,1,1]},{"name":"Valid","mesh":0,"translation":[4,0,0]}])GLTF", "[0,1]");
    const TemporaryMeshFile source(gltf, "gltf");

    MeshImportOptions options;
    options.ImportMaterials = false;
    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);

    REQUIRE(result);
    REQUIRE(result.Data != nullptr);
    CHECK(result.Data->GetVertexCount() == 3);
    CHECK(result.Data->GetIndexCount() == 3);
    REQUIRE(result.SubMeshes.size() == 1);
    const Vector<glm::vec3> positions = result.Data->GetPositions();
    REQUIRE(positions.size() == 3);
    CHECK_THAT(positions[0].x, Catch::Matchers::WithinAbs(4.0f, 0.001f));
}

TEST_CASE("Negative mesh scale preserves mirrored winding and tangent basis", "[Assets][Importer][Mesh]")
{
    const String gltf =
      BuildTriangleGltf(R"GLTF([{"name":"Triangle","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TANGENT":2},"indices":3,"mode":4}]}])GLTF",
                        R"GLTF([{"name":"Triangle","mesh":0}])GLTF", "[0]");
    const TemporaryMeshFile source(gltf, "gltf");

    MeshImportOptions options;
    options.ImportMaterials = false;
    options.ScaleFactor = -2.0f;
    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);

    REQUIRE(result);
    REQUIRE(result.Data != nullptr);
    const Vector<glm::vec3> positions = result.Data->GetPositions();
    const Vector<glm::vec3> normals = result.Data->GetNormals();
    const Vector<glm::vec3> tangents = result.Data->GetTangents();
    const Vector<glm::vec3> bitangents = result.Data->GetBitangents();
    const Vector<uint32_t> indices = result.Data->GetIndices();
    REQUIRE(positions.size() == 3);
    REQUIRE(normals.size() == positions.size());
    REQUIRE(tangents.size() == positions.size());
    REQUIRE(bitangents.size() == positions.size());
    REQUIRE(indices.size() == 3);

    const glm::vec3 geometricNormal =
      glm::normalize(glm::cross(positions[indices[1]] - positions[indices[0]], positions[indices[2]] - positions[indices[0]]));
    CHECK_THAT(geometricNormal.z, Catch::Matchers::WithinAbs(-1.0f, 0.001f));
    for (uint32_t vertex = 0; vertex < positions.size(); vertex++)
    {
        CHECK_THAT(normals[vertex].z, Catch::Matchers::WithinAbs(-1.0f, 0.001f));
        CHECK_THAT(tangents[vertex].x, Catch::Matchers::WithinAbs(-1.0f, 0.001f));
        CHECK_THAT(bitangents[vertex].y, Catch::Matchers::WithinAbs(-1.0f, 0.001f));
    }
}

TEST_CASE("Mesh parser duplicates transformed morph deltas for scene instances", "[Assets][Importer][Mesh][Morph]")
{
    constexpr StringView morphBuffer = "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAIA";
    const String gltf = BuildGltfDocument(
      78, morphBuffer,
      R"GLTF([{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":72,"byteLength":6,"target":34963}])GLTF",
      R"GLTF([{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}])GLTF",
      R"GLTF([{"name":"Triangle","weights":[0],"extras":{"targetNames":["Offset"]},"primitives":[{"attributes":{"POSITION":0},"indices":2,"targets":[{"POSITION":1}],"mode":4}]}])GLTF",
      R"GLTF([{"name":"Wide","mesh":0,"scale":[2,1,1]},{"name":"Mirrored","mesh":0,"translation":[10,0,0],"scale":[-3,1,1]}])GLTF", "[0,1]");
    const TemporaryMeshFile source(gltf, "gltf");

    MeshImportOptions options;
    options.ImportMaterials = false;
    options.ImportMorphMeshes = true;
    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);

    REQUIRE(result);
    REQUIRE(result.Data != nullptr);
    REQUIRE(result.Morph != nullptr);
    CHECK(result.Data->GetVertexCount() == 6);
    CHECK(result.Morph->GetVertexCount() == 6);
    REQUIRE(result.Morph->GetChannelCount() == 1);
    const Ref<MorphChannel>& channel = result.Morph->GetChannel(0);
    REQUIRE(channel != nullptr);
    REQUIRE(channel->GetShapeCount() == 1);
    const Vector<MorphData>& vertices = channel->GetShape(0)->GetVertices();
    REQUIRE(vertices.size() == 2);

    const auto first = std::find_if(vertices.begin(), vertices.end(), [](const MorphData& morph) { return morph.VertexIndex == 0; });
    const auto second = std::find_if(vertices.begin(), vertices.end(), [](const MorphData& morph) { return morph.VertexIndex == 3; });
    REQUIRE(first != vertices.end());
    REQUIRE(second != vertices.end());
    CHECK_THAT(first->VertexTranslation.x, Catch::Matchers::WithinAbs(2.0f, 0.001f));
    CHECK_THAT(second->VertexTranslation.x, Catch::Matchers::WithinAbs(-3.0f, 0.001f));
}

TEST_CASE("Mesh parser rejects transformed skinned instances", "[Assets][Importer][Mesh][Skin]")
{
    constexpr StringView skinBuffer =
      "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/"
      "AAAAAAAAAAAAAAAAAAABAAIAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/";
    const String gltf = BuildGltfDocument(
      168, skinBuffer,
      R"GLTF([{"buffer":0,"byteOffset":0,"byteLength":36,"target":34962},{"buffer":0,"byteOffset":36,"byteLength":12,"target":34962},{"buffer":0,"byteOffset":48,"byteLength":48,"target":34962},{"buffer":0,"byteOffset":96,"byteLength":6,"target":34963},{"buffer":0,"byteOffset":104,"byteLength":64}])GLTF",
      R"GLTF([{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5121,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":1,"type":"MAT4"}])GLTF",
      R"GLTF([{"name":"SkinnedTriangle","primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3,"mode":4}]}])GLTF",
      R"GLTF([{"name":"MeshNode","mesh":0,"skin":0,"translation":[2,0,0]},{"name":"Joint"}])GLTF", "[0,1]",
      R"GLTF(,"skins":[{"inverseBindMatrices":4,"joints":[1],"skeleton":1}])GLTF");
    const TemporaryMeshFile source(gltf, "gltf");

    MeshImportOptions options;
    options.ImportMaterials = false;
    options.ImportBones = true;
    const MeshImportResult result = MeshImporter::Parse(source.GetPath(), options);

    CHECK_FALSE(result);
    CHECK(result.Data == nullptr);
    CHECK(result.SubMeshes.empty());
    CHECK(result.MaterialIndices.empty());
    CHECK_FALSE(result.Bones.empty());
    CHECK(result.MeshSkeleton != nullptr);
}

TEST_CASE("Mesh import options survive metadata round trip", "[Assets][Importer][Mesh]")
{
    Ref<MeshImportOptions> source = CreateRef<MeshImportOptions>();
    source->ScaleFactor = 0.01f;
    source->CpuCached = true;
    source->ImportAnimations = true;
    source->ImportMorphMeshes = true;
    source->ImportBones = true;
    source->ImportMaterials = false;
    source->ImportVertexColors = false;
    source->FlipUVs = true;
    source->FlipWindingOrder = true;
    source->AnimationInfo.push_back({ "Walk", 10, 24 });

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    ImportOptionsSerializer::Serialize(emitter, source);
    emitter << YAML::EndMap;
    const Ref<MeshImportOptions> restored = StaticRefCast<MeshImportOptions>(ImportOptionsSerializer::Deserialize(YAML::Load(emitter.c_str())));

    REQUIRE(restored != nullptr);
    CHECK_THAT(restored->ScaleFactor, Catch::Matchers::WithinAbs(0.01f, 0.0001f));
    CHECK(restored->CpuCached);
    CHECK(restored->ImportAnimations);
    CHECK(restored->ImportMorphMeshes);
    CHECK(restored->ImportBones);
    CHECK_FALSE(restored->ImportMaterials);
    CHECK_FALSE(restored->ImportVertexColors);
    CHECK(restored->FlipUVs);
    CHECK(restored->FlipWindingOrder);
    REQUIRE(restored->AnimationInfo.size() == 1);
    CHECK(restored->AnimationInfo[0].Name == "Walk");
    CHECK(restored->AnimationInfo[0].StartFrame == 10);
    CHECK(restored->AnimationInfo[0].EndFrame == 24);
}

TEST_CASE("Texture import options survive metadata round trip", "[Assets][Importer][Texture]")
{
    Ref<TextureImportOptions> source = CreateRef<TextureImportOptions>();
    source->AutomaticFormat = false;
    source->Format = TextureFormat::RG8;
    source->Shape = TextureShape::TEXTURE_2D;
    source->GenerateMips = true;
    source->MaxMip = 5;
    source->CpuCached = true;
    source->SRGB = true;
    source->DiskFormat = TextureDiskFormat::ETC1S;

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    ImportOptionsSerializer::Serialize(emitter, source);
    emitter << YAML::EndMap;
    const Ref<TextureImportOptions> restored = StaticRefCast<TextureImportOptions>(ImportOptionsSerializer::Deserialize(YAML::Load(emitter.c_str())));

    REQUIRE(restored != nullptr);
    CHECK_FALSE(restored->AutomaticFormat);
    CHECK(restored->Format == TextureFormat::RG8);
    CHECK(restored->Shape == TextureShape::TEXTURE_2D);
    CHECK(restored->GenerateMips);
    CHECK(restored->MaxMip == 5);
    CHECK(restored->CpuCached);
    CHECK(restored->SRGB);
    CHECK(restored->DiskFormat == TextureDiskFormat::ETC1S);
}

TEST_CASE("Font import options survive metadata round trip", "[Assets][Importer][Font]")
{
    const UUID firstFallback(1, 2, 3, 4);
    const UUID secondFallback(5, 6, 7, 8);
    Ref<FontImportOptions> source = CreateRef<FontImportOptions>();
    source->GetKerningData = false;
    source->AutomaticFontSampling = false;
    source->SamplingFontSize = 48;
    source->AutoSizeAtlas = true;
    source->AtlasDimensionsConstraint = Font::AtlasDimensionsConstraint::POWER_OF_TWO_RECTANGLE;
    source->Range = CharsetRange::SymbolRange;
    source->CustomCharset = "\xE4\xB8\x96\xF0\x9F\x8C\x8D";
    source->Padding = 3;
    source->DynamicFontAtlas = true;
    source->TabMultiple = 6;
    source->FallbackFonts = { UUID::EMPTY, firstFallback, firstFallback, secondFallback };

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    ImportOptionsSerializer::Serialize(emitter, source);
    emitter << YAML::EndMap;
    const Ref<FontImportOptions> restored = StaticRefCast<FontImportOptions>(ImportOptionsSerializer::Deserialize(YAML::Load(emitter.c_str())));

    REQUIRE(restored != nullptr);
    CHECK_FALSE(restored->GetKerningData);
    CHECK_FALSE(restored->AutomaticFontSampling);
    CHECK(restored->SamplingFontSize == 48);
    CHECK(restored->AutoSizeAtlas);
    CHECK(restored->AtlasDimensionsConstraint == Font::AtlasDimensionsConstraint::POWER_OF_TWO_RECTANGLE);
    CHECK(restored->Range == CharsetRange::SymbolRange);
    CHECK(restored->CustomCharset == source->CustomCharset);
    CHECK(restored->Padding == 3);
    CHECK(restored->DynamicFontAtlas);
    CHECK(restored->TabMultiple == 6);
    REQUIRE(restored->FallbackFonts.size() == 2);
    CHECK(restored->FallbackFonts[0] == firstFallback);
    CHECK(restored->FallbackFonts[1] == secondFallback);
}
