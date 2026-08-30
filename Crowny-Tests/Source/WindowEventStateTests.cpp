#include "cwpch.h"

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Window/WindowEventState.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    struct RecordedWindowEvents
    {
        std::array<EventType, 8> Types{};
        uint32_t Count = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t FramebufferWidth = 0;
        uint32_t FramebufferHeight = 0;
        glm::vec2 ContentScale{ 0.0f };
        bool Minimized = false;

        void Record(Event& event)
        {
            REQUIRE(Count < Types.size());
            Types[Count++] = event.GetEventType();
            if (event.GetEventType() == EventType::WindowResize)
            {
                const auto& resize = static_cast<const WindowResizeEvent&>(event);
                Width = resize.GetWidth();
                Height = resize.GetHeight();
                FramebufferWidth = resize.GetFramebufferWidth();
                FramebufferHeight = resize.GetFramebufferHeight();
            }
            else if (event.GetEventType() == EventType::WindowContentScale)
            {
                const auto& scale = static_cast<const WindowContentScaleEvent&>(event);
                ContentScale = { scale.GetXScale(), scale.GetYScale() };
            }
            else if (event.GetEventType() == EventType::WindowMinimize)
            {
                Minimized = static_cast<const WindowMinimizeEvent&>(event).IsMinimized();
            }
        }
    };

    WindowStateSnapshot MakeInitialWindowState()
    {
        WindowStateSnapshot state;
        state.Width = 800;
        state.Height = 600;
        state.FramebufferWidth = 800;
        state.FramebufferHeight = 600;
        state.Left = 50;
        state.Top = 75;
        state.ContentScaleX = 1.0f;
        state.ContentScaleY = 1.0f;
        state.Focused = true;
        return state;
    }
} // namespace

TEST_CASE("Window state coalesces platform callback permutations", "[Events][Window][State]")
{
    constexpr std::array<std::array<uint32_t, 3>, 6> permutations = {
        std::array<uint32_t, 3>{ 0, 1, 2 }, std::array<uint32_t, 3>{ 0, 2, 1 }, std::array<uint32_t, 3>{ 1, 0, 2 },
        std::array<uint32_t, 3>{ 1, 2, 0 }, std::array<uint32_t, 3>{ 2, 0, 1 }, std::array<uint32_t, 3>{ 2, 1, 0 }
    };

    for (const auto& permutation : permutations)
    {
        WindowEventState state(MakeInitialWindowState());
        for (const uint32_t operation : permutation)
        {
            if (operation == 0)
                state.SetWindowSize(1200, 700);
            else if (operation == 1)
                state.SetFramebufferSize(1800, 1050);
            else
                state.SetContentScale(1.5f, 1.5f);
        }

        RecordedWindowEvents events;
        state.Flush([&](Event& event) { events.Record(event); });

        REQUIRE(events.Count == 2);
        CHECK(events.Types[0] == EventType::WindowContentScale);
        CHECK(events.Types[1] == EventType::WindowResize);
        CHECK(events.ContentScale == glm::vec2(1.5f));
        CHECK(events.Width == 1200);
        CHECK(events.Height == 700);
        CHECK(events.FramebufferWidth == 1800);
        CHECK(events.FramebufferHeight == 1050);

        state.Flush([&](Event& event) { events.Record(event); });
        CHECK(events.Count == 2);
    }
}

TEST_CASE("Window state publishes minimize and restore before their final extent", "[Events][Window][State]")
{
    WindowEventState state(MakeInitialWindowState());
    state.SetFramebufferSize(-1, -1);
    state.SetMinimized(true);
    state.SetWindowSize(0, 0);

    RecordedWindowEvents minimized;
    state.Flush([&](Event& event) { minimized.Record(event); });
    REQUIRE(minimized.Count == 2);
    CHECK(minimized.Types[0] == EventType::WindowMinimize);
    CHECK(minimized.Types[1] == EventType::WindowResize);
    CHECK(minimized.Minimized);
    CHECK(minimized.Width == 0);
    CHECK(minimized.Height == 0);
    CHECK(minimized.FramebufferWidth == 0);
    CHECK(minimized.FramebufferHeight == 0);

    state.SetFramebufferSize(1600, 1200);
    state.SetWindowSize(800, 600);
    state.SetMinimized(false);

    RecordedWindowEvents restored;
    state.Flush([&](Event& event) { restored.Record(event); });
    REQUIRE(restored.Count == 2);
    CHECK(restored.Types[0] == EventType::WindowMinimize);
    CHECK(restored.Types[1] == EventType::WindowResize);
    CHECK_FALSE(restored.Minimized);
    CHECK(restored.FramebufferWidth == 1600);
    CHECK(restored.FramebufferHeight == 1200);
}

TEST_CASE("Window focus events are edge triggered", "[Events][Window][State]")
{
    WindowEventState state(MakeInitialWindowState());
    state.SetFocused(false);
    state.SetFocused(false);

    RecordedWindowEvents events;
    state.Flush([&](Event& event) { events.Record(event); });
    REQUIRE(events.Count == 1);
    CHECK(events.Types[0] == EventType::WindowLostFocus);

    state.SetFocused(true);
    state.Flush([&](Event& event) { events.Record(event); });
    REQUIRE(events.Count == 2);
    CHECK(events.Types[1] == EventType::WindowFocus);
}

TEST_CASE("Window state storms publish once without allocations", "[Events][Window][State][Memory]")
{
    WindowEventState state(MakeInitialWindowState());
    uint32_t eventCount = 0;
    EventCallbackFn callback = [&](Event&) { eventCount++; };
    state.Flush(callback);

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (int32_t index = 0; index < 1000; index++)
    {
        state.SetPosition(index, -index);
        state.SetWindowSize(800 + index, 600 + index);
        state.SetFramebufferSize(1600 + index * 2, 1200 + index * 2);
    }
    state.Flush(callback);
    const Memory::ThreadAllocationSnapshot delta =
      Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(eventCount == 2);
    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
    CHECK(state.GetCurrent().Left == 999);
    CHECK(state.GetCurrent().Top == -999);
    CHECK(state.GetCurrent().Width == 1799);
    CHECK(state.GetCurrent().FramebufferWidth == 3598);
}
