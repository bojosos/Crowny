#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Crowny/Common/Timer.h"
#include "cwpch.h"

#include <thread>

using namespace Crowny;

TEST_CASE("Timer Utility", "[Common][Timer]")
{
    Timer timer;

    SECTION("Initial State")
    {
        // Should be very close to 0 immediately after creation
        CHECK(timer.ElapsedMicros() < 10000);
    }

    SECTION("Elapsed Time")
    {
        timer.Reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        const uint64_t micros = timer.ElapsedMicros();
        const float millis = timer.ElapsedMillis();
        const float seconds = timer.ElapsedSeconds();

        // Sleeping may overshoot substantially on instrumented or loaded CI hosts. Verify the
        // lower bound, reject unit-scale mistakes, and compare all three representations.
        CHECK(micros >= 45000);
        CHECK(micros < 500000);
        CHECK_THAT(millis, Catch::Matchers::WithinRel(static_cast<float>(micros) / 1000.0f, 0.05f));
        CHECK_THAT(seconds, Catch::Matchers::WithinRel(static_cast<float>(micros) / 1000000.0f, 0.05f));
    }

    SECTION("Reset")
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        timer.Reset();
        CHECK(timer.ElapsedMicros() < 10000);
    }
}
