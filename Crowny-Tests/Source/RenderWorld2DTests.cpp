#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/GpuWorld2D.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/RenderWorld2D.h"

using namespace Crowny;

namespace
{
    class SnapshotTexture2D final : public Texture
    {
    public:
        SnapshotTexture2D() : Texture(TextureDesc{}, true) {}
        PixelData Lock(GpuLockOptions, uint32_t, uint32_t, uint32_t) override { return {}; }
        void Unlock() override {}
        void ReadData(PixelData&, uint32_t, uint32_t, uint32_t) override {}
        bool ReadPixel(uint32_t, uint32_t, void*, size_t, uint32_t, uint32_t, uint32_t) override { return false; }
        void WriteData(const PixelData&, uint32_t, uint32_t, uint32_t) override {}
    };
} // namespace

TEST_CASE("Moving 2D snapshots retain texture bindings without copying resource ownership", "[Renderer][2D][Memory]")
{
    const auto firstTexture = CreateRef<SnapshotTexture2D>();
    const auto secondTexture = CreateRef<SnapshotTexture2D>();
    RenderWorld2D world;
    GpuWorld2D mirror;
    RenderInstance2DDesc desc;
    desc.TextureResource = firstTexture;
    const auto handle = world.Create(desc);
    Vector<RenderChange2D> create;
    world.DrainChanges(create);
    REQUIRE(create.size() == 1);
    CHECK(create[0].TextureChanged);
    CHECK(create[0].TextureResource == firstTexture);
    mirror.Apply(create);
    const auto referenceCount = firstTexture->GetRefCount();
    FrameVector<RenderChange2D> updates;
    for (uint64_t frame = 1; frame < 4; ++frame)
    {
        world.BeginFrame(frame);
        desc.Transform[3].x = float(frame);
        desc.Color.a = float(frame) * 0.2f;
        REQUIRE(world.Update(handle, desc));
        world.DrainChanges(updates);
        REQUIRE(updates.Size() == 1);
        CHECK_FALSE(updates[0].TextureChanged);
        CHECK(updates[0].TextureResource == nullptr);
        mirror.Apply({ updates.begin(), updates.Size() });
        CHECK(firstTexture->GetRefCount() == referenceCount);
        RenderableSprite sprite;
        REQUIRE(mirror.GetSprite(handle, sprite));
        CHECK(sprite.Texture == firstTexture);
        CHECK(sprite.WorldMatrix[3].x == float(frame));
    }

    // A later transform/paint write must not erase a pending texture replacement.
    desc.TextureResource = secondTexture;
    REQUIRE(world.Update(handle, desc));
    desc.Color.r = 0.25f;
    REQUIRE(world.Update(handle, desc));
    world.DrainChanges(updates);
    REQUIRE(updates.Size() == 1);
    CHECK(updates[0].TextureChanged);
    CHECK(updates[0].TextureResource == secondTexture);
    RenderableSprite before;
    REQUIRE(mirror.GetSprite(handle, before));
    CHECK(before.Texture == firstTexture);
    mirror.Apply({ updates.begin(), updates.Size() });
    RenderableSprite after;
    REQUIRE(mirror.GetSprite(handle, after));
    CHECK(after.Texture == secondTexture);
    CHECK(create[0].TextureResource == firstTexture);

    // Null with the change flag explicitly clears the old binding.
    desc.TextureResource.Reset();
    REQUIRE(world.Update(handle, desc));
    world.DrainChanges(updates);
    REQUIRE(updates.Size() == 1);
    CHECK(updates[0].TextureChanged);
    CHECK(updates[0].TextureResource == nullptr);
    mirror.Apply({ updates.begin(), updates.Size() });
    REQUIRE(mirror.GetSprite(handle, after));
    CHECK(after.Texture == nullptr);

    desc.TextureResource = secondTexture;
    REQUIRE(world.Update(handle, desc));
    REQUIRE(world.Destroy(handle));
    world.DrainChanges(updates);
    REQUIRE(updates.Size() == 1);
    CHECK(updates[0].Type == RenderChange2DType::Destroy);
    CHECK_FALSE(updates[0].TextureChanged);
    CHECK(updates[0].TextureResource == nullptr);
    mirror.Apply({ updates.begin(), updates.Size() });
    CHECK_FALSE(mirror.GetSprite(handle, after));
}

TEST_CASE("Unpublished 2D creates publish their final texture and slot reuse starts a new binding", "[Renderer][2D]")
{
    RenderWorld2D world;
    GpuWorld2D mirror;
    Vector<RenderChange2D> changes;
    RenderInstance2DDesc desc;
    desc.TextureResource = CreateRef<SnapshotTexture2D>();
    const auto handle = world.Create(desc);
    desc.TextureResource = CreateRef<SnapshotTexture2D>();
    REQUIRE(world.Update(handle, desc));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderChange2DType::Create);
    CHECK(changes[0].TextureChanged);
    CHECK(changes[0].TextureResource == desc.TextureResource);
    mirror.Apply(changes);
    REQUIRE(world.Destroy(handle));
    world.DrainChanges(changes);
    mirror.Apply(changes);
    const auto reused = world.Create({});
    REQUIRE(reused.GetIndex() == handle.GetIndex());
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].TextureChanged);
    CHECK(changes[0].TextureResource == nullptr);
    mirror.Apply(changes);
    RenderableSprite sprite;
    REQUIRE(mirror.GetSprite(reused, sprite));
    CHECK(sprite.Texture == nullptr);
}

TEST_CASE("2D world coalesces direct updates and rejects retired handles", "[Renderer][2D]")
{
    RenderWorld2D world(1);
    Vector<RenderChange2D> changes;
    RenderInstance2DDesc desc;
    const auto first = world.Create(desc);
    desc.Color.r = 0.25f;
    REQUIRE(world.Update(first, desc));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderChange2DType::Create);
    CHECK(changes[0].Data.Color.r == 0.25f);
    REQUIRE(world.Update(first, desc));
    world.DrainChanges(changes);
    CHECK(changes.empty());
    REQUIRE(world.Destroy(first));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Type == RenderChange2DType::Destroy);
    const auto second = world.Create(desc);
    CHECK(second.GetIndex() == first.GetIndex());
    CHECK(second.GetGeneration() != first.GetGeneration());
    CHECK_FALSE(world.Update(first, desc));
    CHECK_FALSE(world.Destroy(first));
}

TEST_CASE("2D motion settles once per simulation frame instead of per camera", "[Renderer][2D]")
{
    RenderWorld2D world;
    Vector<RenderChange2D> changes;
    RenderInstance2DDesc desc;
    world.BeginFrame(1);
    const auto handle = world.Create(desc);
    world.DrainChanges(changes);
    world.BeginFrame(2);
    desc.Transform[3].x = 4;
    REQUIRE(world.Update(handle, desc));
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Data.PreviousTransform.Row0.w == 0);
    CHECK(changes[0].Data.Transform.Row0.w == 4);
    world.BeginFrame(2);
    REQUIRE(world.Update(handle, desc));
    world.DrainChanges(changes);
    CHECK(changes.empty());
    world.BeginFrame(3);
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Data.PreviousTransform.Row0.w == 4);
    world.BeginFrame(4);
    world.DrainChanges(changes);
    CHECK(changes.empty());
}

TEST_CASE("2D world cancels unpublished creates without reusing pending slots", "[Renderer][2D]")
{
    RenderWorld2D world;
    Vector<RenderChange2D> changes;
    const auto first = world.Create({});
    REQUIRE(world.Destroy(first));
    const auto second = world.Create({});
    CHECK(first.GetIndex() != second.GetIndex());
    world.DrainChanges(changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].Handle == second);
    CHECK(world.GetActiveCount() == 1);
}

TEST_CASE("Published 2D changes survive later drains and slot reuse", "[Renderer][2D]")
{
    RenderWorld2D world(2);
    Vector<RenderChange2D> firstCamera, laterCamera;
    RenderInstance2DDesc desc;
    desc.Transform[3].x = 3;
    const auto first = world.Create(desc);
    const auto cancelled = world.Create({});
    REQUIRE(world.Destroy(cancelled));
    world.DrainChanges(firstCamera);
    REQUIRE(firstCamera.size() == 1);
    CHECK(firstCamera[0].Handle == first);
    desc.Transform[3].x = 7;
    REQUIRE(world.Update(first, desc));
    world.DrainChanges(laterCamera);
    REQUIRE(laterCamera.size() == 1);
    CHECK(laterCamera[0].Data.Transform.Row0.w == 7);
    CHECK(firstCamera[0].Data.Transform.Row0.w == 3);
    REQUIRE(world.Destroy(first));
    world.DrainChanges(laterCamera);
    const auto replacement = world.Create(desc);
    CHECK(replacement.GetIndex() == first.GetIndex());
    CHECK(replacement.GetGeneration() != first.GetGeneration());
    world.DrainChanges(laterCamera);
    CHECK(firstCamera[0].Handle == first);
    CHECK(firstCamera[0].Type == RenderChange2DType::Create);
    CHECK(firstCamera[0].Data.Transform.Row0.w == 3);
}

TEST_CASE("Moving retained 2D instances reuse warmed change storage", "[Renderer][2D][Memory]")
{
    constexpr uint32_t count = 1024;
    RenderWorld2D world(count);
    Vector<RenderChange2D> changes;
    Vector<RenderHandle2D> handles;
    handles.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        handles.push_back(world.Create({}));
    world.DrainChanges(changes);
    auto moveFrame = [&](uint64_t frame) {
        world.BeginFrame(frame);
        for (uint32_t i = 0; i < count; ++i)
        {
            RenderInstance2DDesc desc;
            desc.Transform[3].x = static_cast<float>(frame + i);
            world.Update(handles[i], desc);
        }
        world.DrainChanges(changes);
    };
    moveFrame(1);
    moveFrame(2);
    const auto before = Memory::GetThreadAllocationSnapshot();
    for (uint64_t frame = 3; frame < 20; ++frame)
        moveFrame(frame);
    const auto delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());
    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
    REQUIRE(changes.size() == count);
    CHECK(changes.front().Data.Transform.Row0.w == 19.0f);
    CHECK(changes.front().Data.PreviousTransform.Row0.w == 18.0f);
}

TEST_CASE("2D changes publish directly into reusable snapshot storage", "[Renderer][2D][Memory]")
{
    RenderWorld2D world(1);
    FrameVector<RenderChange2D> changes;
    RenderInstance2DDesc desc;
    const auto handle = world.Create(desc);
    world.DrainChanges(changes);
    REQUIRE(changes.Size() == 1);
    CHECK(changes[0].Type == RenderChange2DType::Create);
    const auto before = Memory::GetThreadAllocationSnapshot();
    for (uint64_t frame = 1; frame < 10; ++frame)
    {
        world.BeginFrame(frame);
        desc.Transform[3].x = float(frame);
        world.Update(handle, desc);
        world.DrainChanges(changes);
    }
    const auto delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());
    CHECK(delta.AllocationCount == 0);
    REQUIRE(changes.Size() == 1);
    CHECK(changes[0].Data.Transform.Row0.w == 9);
    CHECK(changes[0].Data.PreviousTransform.Row0.w == 8);
    REQUIRE(world.Destroy(handle));
    world.DrainChanges(changes);
    REQUIRE(changes.Size() == 1);
    CHECK(changes[0].Type == RenderChange2DType::Destroy);
    world.DrainChanges(changes);
    CHECK(changes.Empty());
    const auto cancelled = world.Create({});
    REQUIRE(world.Destroy(cancelled));
    world.DrainChanges(changes);
    CHECK(changes.Empty());
}
