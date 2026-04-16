#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "cwpch.h"
#include "Crowny/Common/Timer.h"

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
        
        uint64_t micros = timer.ElapsedMicros();
        float millis = timer.ElapsedMillis();
        float seconds = timer.ElapsedSeconds();

        // Check within reasonable bounds (allowing for OS scheduling jitter)
        CHECK(micros >= 45000);
        CHECK(micros <= 75000);
        
        CHECK_THAT(millis, Catch::Matchers::WithinRel(50.0f, 0.5f)); // 50% tolerance for CI jitter
        CHECK_THAT(seconds, Catch::Matchers::WithinRel(0.05f, 0.5f));
    }

    SECTION("Reset")
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        timer.Reset();
        CHECK(timer.ElapsedMicros() < 5000);
    }
}
