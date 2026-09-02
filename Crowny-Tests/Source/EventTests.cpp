#include <catch2/catch_test_macros.hpp>

#include "Crowny/Events/ApplicationEvent.h"
#include "Crowny/Events/Event.h"
#include "Crowny/Events/KeyEvent.h"
#include "Crowny/Events/MouseEvent.h"
#include "Crowny/Window/RenderWindow.h"
#include "Crowny/Window/Window.h"
#include "cwpch.h"

using namespace Crowny;

class TestEvent : public Event
{
public:
    TestEvent(int value) : m_Value(value) {}

    EVENT_CLASS_TYPE(AppUpdate)
    EVENT_CLASS_CATEGORY(EventCategoryApplication)

    int GetValue() const { return m_Value; }

private:
    int m_Value;
};

TEST_CASE("Event System", "[Events]")
{
    SECTION("Event Properties")
    {
        TestEvent e(42);
        CHECK(e.GetEventType() == EventType::AppUpdate);
        CHECK(e.GetName() == String("AppUpdate"));
        CHECK(e.GetCategoryFlags() == EventCategoryApplication);
        CHECK(e.IsInCategory(EventCategoryApplication));
        CHECK_FALSE(e.IsInCategory(EventCategoryInput));
        CHECK(e.GetValue() == 42);
        CHECK_FALSE(e.Handled);
    }

    SECTION("Event Dispatcher")
    {
        TestEvent e(100);
        EventDispatcher dispatcher(e);

        SECTION("Correct Dispatch")
        {
            bool dispatched = dispatcher.Dispatch<TestEvent>([](TestEvent& ev) {
                CHECK(ev.GetValue() == 100);
                return true; // Mark as handled
            });

            CHECK(dispatched);
            CHECK(e.Handled);
        }

        SECTION("Incorrect Dispatch Type")
        {
            // KeyPressed is different from AppUpdate
            struct DummyEvent : public Event
            {
                EVENT_CLASS_TYPE(KeyPressed)
                EVENT_CLASS_CATEGORY(EventCategoryKeyboard)
            };

            bool dispatched = dispatcher.Dispatch<DummyEvent>([](DummyEvent& ev) { return true; });

            CHECK_FALSE(dispatched);
            CHECK_FALSE(e.Handled);
        }
    }
}

TEST_CASE("Window descriptions and properties preserve window state", "[Events][Window]")
{
    WindowDesc windowDesc;
    CHECK(windowDesc.Width == 1280);
    CHECK(windowDesc.Height == 720);
    CHECK(windowDesc.Left == -1);
    CHECK(windowDesc.Top == -1);
    CHECK(windowDesc.Mode == WindowMode::Windowed);

    RenderWindowDesc renderDesc;
    renderDesc.Width = 1920;
    renderDesc.Height = 1080;
    renderDesc.Mode = WindowMode::BorderlessFullscreen;
    renderDesc.StartMaximized = true;
    renderDesc.Samples = 4;
    RenderWindowProperties properties(renderDesc);
    CHECK(properties.Width == 1920);
    CHECK(properties.Height == 1080);
    CHECK(properties.Samples == 4);
    CHECK(properties.Mode == WindowMode::BorderlessFullscreen);
    CHECK(properties.Fullscreen);
    CHECK(properties.IsMaximized);
}

TEST_CASE("Window events retain platform state", "[Events][Window]")
{
    WindowResizeEvent resize(800, 600, 1600, 1200);
    CHECK(resize.GetWidth() == 800);
    CHECK(resize.GetHeight() == 600);
    CHECK(resize.GetFramebufferWidth() == 1600);
    CHECK(resize.GetFramebufferHeight() == 1200);

    WindowResizeEvent legacyResize(640, 480);
    CHECK(legacyResize.GetFramebufferWidth() == 640);
    CHECK(legacyResize.GetFramebufferHeight() == 480);

    WindowResizeEvent minimizedResize(640, 480, 0, 0);
    CHECK(minimizedResize.GetFramebufferWidth() == 0);
    CHECK(minimizedResize.GetFramebufferHeight() == 0);

    WindowMinimizeEvent minimized(true);
    WindowMinimizeEvent restored(false);
    CHECK(minimized.IsMinimized());
    CHECK_FALSE(restored.IsMinimized());

    WindowMoveEvent moved(-1920, 32);
    CHECK(moved.GetLeft() == -1920);
    CHECK(moved.GetTop() == 32);

    WindowCloseEvent close;
    CHECK_FALSE(close.IsCancelled());
    close.Cancel();
    CHECK(close.IsCancelled());

    WindowFileDropEvent drop({ Path("one.txt"), Path("two.png") }, glm::vec2(120.0f, 48.0f));
    REQUIRE(drop.GetPaths().size() == 2);
    CHECK(drop.GetPaths()[1] == Path("two.png"));
    CHECK(drop.GetMousePosition() == glm::vec2(120.0f, 48.0f));
    CHECK(drop.GetEventType() == EventType::WindowFileDrop);
    CHECK(drop.IsInCategory(EventCategoryApplication));

    WindowFileDropEvent legacyDrop({ Path("one.txt") });
    CHECK(legacyDrop.GetMousePosition() == glm::vec2(0.0f));

    WindowContentScaleEvent scale(1.5f, 2.0f);
    CHECK(scale.GetXScale() == 1.5f);
    CHECK(scale.GetYScale() == 2.0f);

    KeyTypedEvent typed(0x1F642);
    CHECK(typed.GetCodepoint() == 0x1F642);

    MouseButtonPressedEvent mouse(Mouse::ButtonLeft, glm::vec2(12.0f, 34.0f));
    CHECK(mouse.GetPosition() == glm::vec2(12.0f, 34.0f));
}
