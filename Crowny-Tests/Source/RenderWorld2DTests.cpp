#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/Renderer/RenderWorld2D.h"

using namespace Crowny;

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
