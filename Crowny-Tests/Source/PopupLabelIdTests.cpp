#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "UI/PopupLabelId.h"

#include <limits>

using namespace Crowny;

TEST_CASE("Popup label IDs preserve current editor identifiers", "[Editor][UI][Popup]")
{
    CHECK(UI::PopupLabelId::Create("AssetSearch", 0u).View() == "AssetSearch##0");
    CHECK(UI::PopupLabelId::Create("EntitySearch", 10u).View() == "EntitySearch##10");
    CHECK(UI::PopupLabelId::Create("ScriptSearch", std::numeric_limits<uint32_t>::max()).View() == "ScriptSearch##4294967295");
}

TEST_CASE("Popup label IDs own independent fixed storage", "[Editor][UI][Popup]")
{
    const UI::PopupLabelId first = UI::PopupLabelId::Create("EntitySearch", 4u);
    const UI::PopupLabelId second = UI::PopupLabelId::Create("AssetSearch", 6u);
    const UI::PopupLabelId copied = first;

    CHECK(first.View() == "EntitySearch##4");
    CHECK(second.View() == "AssetSearch##6");
    CHECK(copied.View() == first.View());
    CHECK(copied.CStr() != first.CStr());
}

TEST_CASE("Popup label IDs fit the maximum stem and counter", "[Editor][UI][Popup]")
{
    const UI::PopupLabelId id = UI::PopupLabelId::Create("1234567890123456789", std::numeric_limits<uint32_t>::max());

    CHECK(id.View() == "1234567890123456789##4294967295");
    CHECK(id.View().size() == UI::PopupLabelId::Capacity - 1u);
    CHECK(id.CStr()[UI::PopupLabelId::Capacity - 1u] == '\0');
}

TEST_CASE("Popup label ID construction allocates nothing per frame", "[Editor][UI][Popup][Memory][Frame]")
{
    uint64_t checksum = 0u;

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0u; frame < 120u; frame++)
    {
        const UI::PopupLabelId id = frame % 3u == 0u   ? UI::PopupLabelId::Create("AssetSearch", frame)
                                    : frame % 3u == 1u ? UI::PopupLabelId::Create("EntitySearch", frame)
                                                       : UI::PopupLabelId::Create("ScriptSearch", frame);
        checksum += id.CStr()[0];
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(checksum != 0u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
