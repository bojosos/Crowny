#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Animation/AnimationPlayer.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/Mesh.h"

using namespace Crowny;
using Catch::Approx;

namespace
{
    bool ApproxVec3(const glm::vec3& value, const glm::vec3& expected, float epsilon = 0.0001f)
    {
        return glm::all(glm::lessThanEqual(glm::abs(value - expected), glm::vec3(epsilon)));
    }
} // namespace

TEST_CASE("Animation curves handle boundaries and interpolation", "[Animation]")
{
    AnimationCurve<float> empty;
    CHECK(empty.Evaluate(1.0f) == 0.0f);

    AnimationCurve<float> curve({ { 1.0f, 10.0f }, { 0.0f, 0.0f }, { 2.0f, 20.0f } });
    CHECK(curve.GetStartTime() == 0.0f);
    CHECK(curve.GetEndTime() == 2.0f);
    CHECK(curve.Evaluate(0.5f) == Approx(5.0f));
    CHECK(curve.Evaluate(-0.5f, AnimationWrapMode::Loop) == Approx(15.0f));
    CHECK(curve.Evaluate(2.5f, AnimationWrapMode::PingPong) == Approx(15.0f));

    KeyFrame<float> first{ 0.0f, 0.0f };
    first.OutTangent = 1.0f;
    first.Interpolation = AnimationInterpolation::Cubic;
    KeyFrame<float> second{ 1.0f, 1.0f };
    second.InTangent = 1.0f;
    AnimationCurve<float> cubic({ first, second });
    CHECK(cubic.Evaluate(0.5f) == Approx(0.5f));
}

TEST_CASE("Skeleton pose maps named tracks to skinning matrices", "[Animation][Skeleton]")
{
    Vector<SkeletonBone> bones;
    bones.push_back({ "Root", INVALID_BONE_INDEX, Transform(), glm::mat4(1.0f) });
    bones.push_back({ "Child", 0, Transform(glm::vec3(0.0f, 1.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f)),
                      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) });
    Ref<Skeleton> skeleton = Skeleton::Create(std::move(bones));
    REQUIRE(skeleton->IsValid());
    CHECK(skeleton->FindBone("Child") == 1);

    AnimationTransformTrack child;
    child.Name = "Child";
    child.Position = AnimationCurve<glm::vec3>({ { 0.0f, glm::vec3(0.0f, 1.0f, 0.0f) }, { 1.0f, glm::vec3(0.0f, 2.0f, 0.0f) } });
    Ref<AnimationClip> clip = AnimationClip::Create({ child });

    SkeletonPose pose(skeleton);
    pose.Evaluate(*clip, 1.0f, AnimationWrapMode::Clamp);
    CHECK(ApproxVec3(glm::vec3(pose.GetGlobalTransforms()[1][3]), glm::vec3(0.0f, 2.0f, 0.0f)));
    CHECK(ApproxVec3(glm::vec3(pose.GetSkinningMatrices()[1][3]), glm::vec3(0.0f, 1.0f, 0.0f)));
}

TEST_CASE("Morph channels blend sparse targets without rebuilding storage", "[Animation][Morph]")
{
    Ref<MorphShape> half = MorphShape::Create("Half", 0.5f, { { glm::vec3(1.0f, 0.0f, 0.0f), {}, 0 } });
    Ref<MorphShape> full = MorphShape::Create("Full", 1.0f, { { glm::vec3(2.0f, 0.0f, 0.0f), {}, 0 } });
    Ref<MeshMorph> morph = MeshMorph::Create({ MorphChannel::Create("Smile", { half, full }) }, 1);

    Vector<glm::vec3> positions;
    Vector<glm::vec3> normals;
    morph->Apply({ 0.75f }, { glm::vec3(0.0f) }, { glm::vec3(0.0f, 1.0f, 0.0f) }, positions, normals);
    REQUIRE(positions.size() == 1);
    CHECK(positions[0].x == Approx(1.5f));
    const size_t capacity = positions.capacity();
    morph->Apply({ 0.25f }, { glm::vec3(0.0f) }, { glm::vec3(0.0f, 1.0f, 0.0f) }, positions, normals);
    CHECK(positions[0].x == Approx(0.5f));
    CHECK(positions.capacity() == capacity);
}

TEST_CASE("Animation player evaluates events, morph weights, and cross fades", "[Animation][Player]")
{
    Ref<AnimationClip> first = AnimationClip::Create({}, { { "Smile", AnimationCurve<float>({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }) } });
    first->SetEvents({ { "Midpoint", 0.5f, "payload" } });
    Ref<AnimationClip> second = AnimationClip::Create({}, { { "Smile", AnimationCurve<float>({ { 0.0f, 1.0f }, { 1.0f, 0.0f } }) } });
    Ref<MeshMorph> morph =
      MeshMorph::Create({ MorphChannel::Create("Smile", { MorphShape::Create("Smile", 1.0f, { { glm::vec3(1.0f), {}, 0 } }) }) }, 1);

    AnimationPlayer player;
    uint32_t eventCount = 0;
    player.SetEventCallback([&](const AnimationEvent& event) {
        CHECK(event.Name == "Midpoint");
        eventCount++;
    });
    player.Play(first);
    player.Update(0.5f, nullptr, morph);
    CHECK(eventCount == 1);
    REQUIRE(player.GetMorphWeights().size() == 1);
    CHECK(player.GetMorphWeights()[0] == Approx(0.5f));

    player.CrossFade(second, 1.0f);
    player.Update(0.5f, nullptr, morph);
    CHECK(player.GetMorphWeights()[0] == Approx(0.25f));
}

TEST_CASE("Looping root motion remains continuous across clip boundaries", "[Animation][Player]")
{
    RootMotionCurves rootMotion;
    rootMotion.Position = AnimationCurve<glm::vec3>({ { 0.0f, glm::vec3(0.0f) }, { 1.0f, glm::vec3(1.0f, 0.0f, 0.0f) } });
    Ref<AnimationClip> clip = AnimationClip::Create({}, {}, {}, std::move(rootMotion));
    AnimationPlayer player;
    player.Play(clip);
    player.Update(0.75f);
    CHECK(player.GetRootMotionDelta().GetPosition().x == Approx(0.75f));
    player.Update(0.5f);
    CHECK(player.GetRootMotionDelta().GetPosition().x == Approx(0.5f));
}

TEST_CASE("Animation clips preserve all track types during asset round trips", "[Animation][Assets]")
{
    const Path assetPath = fs::temp_directory_path() / "crowny-animation-roundtrip.asset";
    fs::remove(assetPath);
    AssetManager manager;

    AnimationTransformTrack transform;
    transform.Name = "Bone";
    transform.Position = AnimationCurve<glm::vec3>({ { 0.0f, glm::vec3(0.0f) }, { 1.0f, glm::vec3(2.0f) } });
    RootMotionCurves rootMotion;
    rootMotion.Rotation = AnimationCurve<glm::quat>(
      { { 0.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f) }, { 1.0f, glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) } });
    Ref<AnimationClip> source =
      AnimationClip::Create({ transform }, { { "Smile", AnimationCurve<float>({ { 0.0f, 0.0f }, { 1.0f, 1.0f } }) } },
                            { { "Opacity", AnimationCurve<float>({ { 0.0f, 1.0f }, { 1.0f, 0.0f } }) } }, std::move(rootMotion), 60.0f);
    source->SetEvents({ { "Event", 0.25f, "Payload" } });
    manager.Save(source, assetPath);
    source = nullptr;

    AssetHandle<AnimationClip> loaded = manager.Load<AnimationClip>(assetPath, false);
    REQUIRE(loaded);
    CHECK(loaded->GetSampleRate() == Approx(60.0f));
    CHECK(loaded->FindTransformTrack("Bone") == 0);
    CHECK(loaded->FindMorphTrack("Smile") == 0);
    CHECK(loaded->FindGenericTrack("Opacity") == 0);
    CHECK(loaded->SampleTransform(0, 0.5f, AnimationWrapMode::Clamp).GetPosition().x == Approx(1.0f));
    CHECK(loaded->SampleMorphWeight(0, 0.5f, AnimationWrapMode::Clamp) == Approx(0.5f));
    REQUIRE(loaded->GetEvents().size() == 1);
    CHECK(loaded->GetEvents()[0].Payload == "Payload");
    CHECK_FALSE(loaded->GetRootMotion().Rotation.IsEmpty());

    loaded = nullptr;
    fs::remove(assetPath);
}

TEST_CASE("Mesh deformer skins vertices using a reusable output mesh", "[Animation][Mesh]")
{
    BufferLayout layout = { { ShaderDataType::Float3, VertexAttribute::Position },
                            { ShaderDataType::Float3, VertexAttribute::Normal },
                            { ShaderDataType::Float2, VertexAttribute::TexCoord0 },
                            { ShaderDataType::Float4, VertexAttribute::BlendWeights },
                            { ShaderDataType::Int4, VertexAttribute::BlendIndices } };
    Ref<MeshData> mesh = MeshData::Create(1, 0, layout);
    mesh->SetPositions({ glm::vec3(0.0f) });
    mesh->SetNormals({ glm::vec3(0.0f, 1.0f, 0.0f) });
    mesh->SetUVs(0, { glm::vec2(0.25f, 0.75f) });
    const glm::vec4 weights(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::ivec4 indices(0);
    mesh->SetVertexData(VertexAttribute::BlendWeights, &weights, sizeof(weights));
    mesh->SetVertexData(VertexAttribute::BlendIndices, &indices, sizeof(indices));

    Ref<Skeleton> skeleton = Skeleton::Create({ { "Root", INVALID_BONE_INDEX, Transform(), glm::mat4(1.0f) } });
    AnimationTransformTrack root;
    root.Name = "Root";
    root.Position = AnimationCurve<glm::vec3>({ { 0.0f, glm::vec3(0.0f) }, { 1.0f, glm::vec3(2.0f, 0.0f, 0.0f) } });
    Ref<AnimationClip> clip = AnimationClip::Create({ root });
    SkeletonPose pose(skeleton);
    pose.Evaluate(*clip, 0.5f, AnimationWrapMode::Clamp);

    MeshDeformer deformer;
    REQUIRE(deformer.Initialize(mesh, skeleton));
    REQUIRE(deformer.Deform(&pose));
    REQUIRE(deformer.GetOutputMeshData()->GetBufferLayout().HasAttribute(VertexAttribute::PreviousPosition));
    CHECK(ApproxVec3(deformer.GetOutputMeshData()->GetPositions()[0], glm::vec3(1.0f, 0.0f, 0.0f)));
    CHECK(deformer.GetOutputMeshData()->GetUVs(0)[0] == glm::vec2(0.25f, 0.75f));
    glm::vec3 previousPosition;
    deformer.GetOutputMeshData()->GetVertexData(VertexAttribute::PreviousPosition, &previousPosition, sizeof(previousPosition));
    CHECK(ApproxVec3(previousPosition, glm::vec3(0.0f)));
    const Ref<MeshData> output = deformer.GetOutputMeshData();

    pose.Evaluate(*clip, 1.0f, AnimationWrapMode::Clamp);
    REQUIRE(deformer.Deform(&pose));
    CHECK(deformer.GetOutputMeshData() == output);
    CHECK(ApproxVec3(deformer.GetOutputMeshData()->GetPositions()[0], glm::vec3(2.0f, 0.0f, 0.0f)));
    deformer.GetOutputMeshData()->GetVertexData(VertexAttribute::PreviousPosition, &previousPosition, sizeof(previousPosition));
    CHECK(ApproxVec3(previousPosition, glm::vec3(1.0f, 0.0f, 0.0f)));
    CHECK(ApproxVec3(deformer.GetSphereBounds().GetCenter(), glm::vec3(2.0f, 0.0f, 0.0f)));
    CHECK(deformer.GetSphereBounds().GetRadius() == Approx(0.0f));
}

TEST_CASE("Mesh deformer applies morph targets before skeletal skinning", "[Animation][Mesh][Morph]")
{
    BufferLayout layout = { { ShaderDataType::Float3, VertexAttribute::Position },
                            { ShaderDataType::Float3, VertexAttribute::Normal },
                            { ShaderDataType::Float4, VertexAttribute::BlendWeights },
                            { ShaderDataType::Int4, VertexAttribute::BlendIndices } };
    Ref<MeshData> mesh = MeshData::Create(1, 0, layout);
    mesh->SetPositions({ glm::vec3(0.0f) });
    mesh->SetNormals({ glm::vec3(0.0f, 1.0f, 0.0f) });
    const glm::vec4 weights(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::ivec4 indices(0);
    mesh->SetVertexData(VertexAttribute::BlendWeights, &weights, sizeof(weights));
    mesh->SetVertexData(VertexAttribute::BlendIndices, &indices, sizeof(indices));

    Ref<MeshMorph> morph = MeshMorph::Create(
      { MorphChannel::Create("Offset", { MorphShape::Create("Offset", 1.0f, { { glm::vec3(1.0f, 0.0f, 0.0f), {}, 0 } }) }) }, 1);
    Ref<Skeleton> skeleton = Skeleton::Create({ { "Root", INVALID_BONE_INDEX, Transform(), glm::mat4(1.0f) } });
    AnimationTransformTrack root;
    root.Name = "Root";
    root.Position = AnimationCurve<glm::vec3>({ { 0.0f, glm::vec3(2.0f, 0.0f, 0.0f) } });
    Ref<AnimationClip> clip = AnimationClip::Create({ root });
    SkeletonPose pose(skeleton);
    pose.Evaluate(*clip, 0.0f, AnimationWrapMode::Clamp);

    MeshDeformer deformer;
    REQUIRE(deformer.Initialize(mesh, skeleton, morph));
    REQUIRE(deformer.Deform(&pose, { 1.0f }));
    CHECK(ApproxVec3(deformer.GetOutputMeshData()->GetPositions()[0], glm::vec3(3.0f, 0.0f, 0.0f)));
}
