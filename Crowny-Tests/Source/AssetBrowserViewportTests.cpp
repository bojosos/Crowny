#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/AssetBrowserViewport.h"

using namespace Crowny;

TEST_CASE("Asset browser viewport maps clipped grid rows to absolute items", "[Editor][AssetBrowser][Viewport]")
{
    CHECK(GetAssetBrowserRowCount(0u, 5u) == 0u);
    CHECK(GetAssetBrowserRowCount(11u, 5u) == 3u);
    CHECK(GetAssetBrowserRowCount(3u, 0u) == 3u);

    CHECK((GetAssetBrowserItemRange(0u, 1u, 5u, 11u) == AssetBrowserItemRange{ 0u, 5u }));
    CHECK((GetAssetBrowserItemRange(1u, 2u, 5u, 11u) == AssetBrowserItemRange{ 5u, 10u }));
    CHECK((GetAssetBrowserItemRange(2u, 3u, 5u, 11u) == AssetBrowserItemRange{ 10u, 11u }));
    CHECK((GetAssetBrowserItemRange(4u, 6u, 5u, 11u) == AssetBrowserItemRange{ 11u, 11u }));
}

TEST_CASE("Asset browser viewport keeps selection and rename rows in absolute coordinates", "[Editor][AssetBrowser][Viewport]")
{
    constexpr uint32_t columnCount = 7u;
    constexpr uint32_t itemCount = 10000u;
    constexpr uint32_t selectedItem = 9123u;
    constexpr uint32_t renamedItem = 34u;

    const uint32_t selectedRow = GetAssetBrowserItemRow(selectedItem, columnCount);
    const uint32_t renamedRow = GetAssetBrowserItemRow(renamedItem, columnCount);
    const AssetBrowserItemRange selectedRange = GetAssetBrowserItemRange(selectedRow, selectedRow + 1u, columnCount, itemCount);
    const AssetBrowserItemRange renamedRange = GetAssetBrowserItemRange(renamedRow, renamedRow + 1u, columnCount, itemCount);

    CHECK(selectedRange.Begin <= selectedItem);
    CHECK(selectedItem < selectedRange.End);
    CHECK(renamedRange.Begin <= renamedItem);
    CHECK(renamedItem < renamedRange.End);
    CHECK(selectedRange.End - selectedRange.Begin <= columnCount);
    CHECK(renamedRange.End - renamedRange.Begin <= columnCount);
}

TEST_CASE("Asset browser presentation fingerprints follow same-cardinality metadata changes", "[Editor][AssetBrowser][Viewport]")
{
    const AssetBrowserPresentationFingerprint original{ 42u, 100, 7u, 1024u, true };
    CHECK_FALSE(NeedsAssetBrowserPresentationRefresh(original, original));

    AssetBrowserPresentationFingerprint changed = original;
    changed.ModifiedTime++;
    CHECK(NeedsAssetBrowserPresentationRefresh(original, changed));
    changed = original;
    changed.Revision++;
    CHECK(NeedsAssetBrowserPresentationRefresh(original, changed));
    changed = original;
    changed.ByteSize++;
    CHECK(NeedsAssetBrowserPresentationRefresh(original, changed));
    changed = original;
    changed.Identity++;
    CHECK(NeedsAssetBrowserPresentationRefresh(original, changed));
    changed = original;
    changed.IsFile = false;
    CHECK(NeedsAssetBrowserPresentationRefresh(original, changed));
}

TEST_CASE("Stable asset browser viewport calculations allocate nothing", "[Editor][AssetBrowser][Viewport][Memory][Frame]")
{
    uint64_t checksum = 0u;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0u; frame < 240u; frame++)
    {
        const uint32_t firstRow = 1200u + frame % 4u;
        const AssetBrowserItemRange range = GetAssetBrowserItemRange(firstRow, firstRow + 8u, 6u, 10000u);
        checksum += range.Begin + range.End + GetAssetBrowserRowCount(10000u, 6u) + GetAssetBrowserItemRow(9123u, 6u);
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(checksum != 0u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
