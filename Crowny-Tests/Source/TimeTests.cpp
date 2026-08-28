#include "Crowny/Common/Time.h"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace Crowny;

namespace
{
    struct ResetTimeOnExit
    {
        ~ResetTimeOnExit() { Time::Reset(); }
    };
}

TEST_CASE("Time tracks frame numbers as integers", "[Common][Time]")
{
    static_assert(std::is_same_v<decltype(Time::GetFrameCount()), uint64_t>);

    Time::Reset();
    [[maybe_unused]] const ResetTimeOnExit resetTime;
    REQUIRE(Time::GetFrameCount() == 0);

    Time::Update(1.0f / 60.0f, 1.0f / 50.0f);
    Time::Update(1.0f / 60.0f, 1.0f / 50.0f);
    CHECK(Time::GetFrameCount() == 2);

    Time::Reset();
    CHECK(Time::GetFrameCount() == 0);
}
