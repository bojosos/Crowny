#include <catch2/catch_test_macros.hpp>

#include "cwpch.h"
#include "Crowny/Events/Event.h"

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
            struct DummyEvent : public Event {
                EVENT_CLASS_TYPE(KeyPressed)
                EVENT_CLASS_CATEGORY(EventCategoryKeyboard)
            };

            bool dispatched = dispatcher.Dispatch<DummyEvent>([](DummyEvent& ev) {
                return true;
            });

            CHECK_FALSE(dispatched);
            CHECK_FALSE(e.Handled);
        }
    }
}
