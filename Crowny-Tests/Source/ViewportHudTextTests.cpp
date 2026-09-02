#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/ViewportHudText.h"

#include <limits>

using namespace Crowny;

namespace
{
    StringView TextOf(const ViewportHudStatus& status) { return status.Text.data(); }
} // namespace

TEST_CASE("Viewport HUD status describes the current selection", "[Editor][Viewport][HUD]")
{
    CHECK(TextOf(FormatViewportHudStatus({}, false, 0u, 1280, 720, 5.0f)) == "No selection  |  1280 x 720  |  View 5.0 m");
    CHECK(TextOf(FormatViewportHudStatus("Camera", true, 1u, 1920, 1080, 6.4f)) == "Camera  |  1920 x 1080  |  View 6.4 m");
    CHECK(TextOf(FormatViewportHudStatus("Ignored", true, 12u, 800, 600, 3.0f)) == "12 entities  |  800 x 600  |  View 3.0 m");
}

TEST_CASE("Viewport HUD status preserves the entity-name truncation boundary", "[Editor][Viewport][HUD]")
{
    CHECK(TextOf(FormatViewportHudStatus("abcdefghijklmnopqrstuvwxyz12", true, 1u, 1280, 720, 5.0f)) ==
          "abcdefghijklmnopqrstuvwxyz12  |  1280 x 720  |  View 5.0 m");
    CHECK(TextOf(FormatViewportHudStatus("abcdefghijklmnopqrstuvwxyz123", true, 1u, 1280, 720, 5.0f)) ==
          "abcdefghijklmnopqrstuvwxy...  |  1280 x 720  |  View 5.0 m");
}

TEST_CASE("Viewport HUD status remains bounded for extreme Counts", "[Editor][Viewport][HUD]")
{
    const String longName(4096u, 'x');
    const ViewportHudStatus status = FormatViewportHudStatus(longName, true, 1u, std::numeric_limits<int32_t>::min(),
                                                             std::numeric_limits<int32_t>::max(), std::numeric_limits<float>::max());
    const ViewportHudStatus multiStatus =
      FormatViewportHudStatus("Ignored", true, std::numeric_limits<size_t>::max(), std::numeric_limits<int32_t>::min(),
                              std::numeric_limits<int32_t>::max(), std::numeric_limits<float>::max());

    CHECK(status.Text.back() == '\0');
    CHECK(TextOf(status).size() < ViewportHudStatus::Capacity);
    CHECK(TextOf(status).starts_with("xxxxxxxxxxxxxxxxxxxxxxxxx..."));
    CHECK(TextOf(status).ends_with(" m"));
    CHECK(multiStatus.Text.back() == '\0');
    CHECK(TextOf(multiStatus).size() < ViewportHudStatus::Capacity);
    CHECK(TextOf(multiStatus).find(" entities  |  ") != StringView::npos);
    CHECK(TextOf(multiStatus).ends_with(" m"));
}

TEST_CASE("Viewport HUD status formatting allocates nothing per frame", "[Editor][Viewport][HUD][Memory][Frame]")
{
    const String longName(4096u, 'x');
    ViewportHudStatus status;
    uint64_t checksum = 0u;

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0u; frame < 120u; frame++)
    {
        const size_t selectionCount = frame % 3u == 0u ? 0u : (frame % 3u == 1u ? 1u : 12u);
        status = FormatViewportHudStatus(longName, selectionCount != 0u, selectionCount, 1920, 1080, 6.4f);
        checksum += status.Text[0];
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(checksum != 0u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
