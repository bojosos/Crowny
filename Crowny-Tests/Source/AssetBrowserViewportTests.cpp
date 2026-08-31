#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/AssetBrowserViewport.h"

using namespace Crowny;

TEST_CASE("Asset browser toolbar keeps its table shape stable for the rendered frame", "[Editor][AssetBrowser][Viewport]")
{
    const AssetBrowserToolbarLayout wideGrid = GetAssetBrowserToolbarLayout(900.0f, true);
    CHECK(wideGrid.Mode == AssetBrowserToolbarMode::Wide);
    CHECK(wideGrid.ColumnCount == 5u);
    CHECK(wideGrid.SearchSharesControlRow);
    CHECK(wideGrid.ShowsThumbnailSize);
    CHECK_FALSE(wideGrid.NavigationSharesControlRow);

    const AssetBrowserToolbarLayout singleRowGrid = GetAssetBrowserToolbarLayout(1180.0f, true, 146.0f);
    CHECK(singleRowGrid.NavigationSharesControlRow);
    CHECK_FALSE(GetAssetBrowserToolbarLayout(978.9f, true, 146.0f).NavigationSharesControlRow);
    CHECK(GetAssetBrowserToolbarLayout(979.0f, true, 146.0f).NavigationSharesControlRow);

    const AssetBrowserToolbarLayout wideList = GetAssetBrowserToolbarLayout(900.0f, false);
    CHECK(wideList.Mode == AssetBrowserToolbarMode::Wide);
    CHECK(wideList.ColumnCount == 4u);
    CHECK_FALSE(wideList.ShowsThumbnailSize);

    // A view change takes effect next frame. The current table keeps the columns it declared.
    const AssetBrowserToolbarLayout renderedFrame = GetAssetBrowserToolbarLayout(900.0f, false);
    const AssetBrowserToolbarLayout nextFrame = GetAssetBrowserToolbarLayout(900.0f, true);
    CHECK(renderedFrame.ColumnCount == 4u);
    CHECK_FALSE(renderedFrame.ShowsThumbnailSize);
    CHECK(nextFrame.ColumnCount == 5u);
    CHECK(nextFrame.ShowsThumbnailSize);
}

TEST_CASE("Asset browser toolbar uses full-width search before controls collapse", "[Editor][AssetBrowser][Viewport]")
{
    const AssetBrowserToolbarLayout compactGrid = GetAssetBrowserToolbarLayout(600.0f, true);
    CHECK(compactGrid.Mode == AssetBrowserToolbarMode::Compact);
    CHECK(compactGrid.ColumnCount == 4u);
    CHECK(compactGrid.ControlRowCount == 1u);
    CHECK_FALSE(compactGrid.SearchSharesControlRow);

    const AssetBrowserToolbarLayout compactList = GetAssetBrowserToolbarLayout(500.0f, false);
    CHECK(compactList.Mode == AssetBrowserToolbarMode::Compact);
    CHECK(compactList.ColumnCount == 3u);
    CHECK(GetAssetBrowserToolbarLayout(697.0f, true).Mode == AssetBrowserToolbarMode::Wide);
    CHECK(GetAssetBrowserToolbarLayout(696.9f, true).Mode == AssetBrowserToolbarMode::Compact);
    CHECK(GetAssetBrowserToolbarLayout(564.0f, false).Mode == AssetBrowserToolbarMode::Wide);
    CHECK(GetAssetBrowserToolbarLayout(563.9f, false).Mode == AssetBrowserToolbarMode::Compact);

    const AssetBrowserToolbarLayout narrowGrid = GetAssetBrowserToolbarLayout(320.0f, true);
    CHECK(narrowGrid.Mode == AssetBrowserToolbarMode::Narrow);
    CHECK(narrowGrid.ColumnCount == 2u);
    CHECK(narrowGrid.ControlRowCount == 2u);

    const AssetBrowserToolbarLayout narrowList = GetAssetBrowserToolbarLayout(320.0f, false);
    CHECK(narrowList.Mode == AssetBrowserToolbarMode::Narrow);
    CHECK(narrowList.ColumnCount == 2u);
}

TEST_CASE("Asset browser header dimensions follow font and breadcrumb content", "[Editor][AssetBrowser][Viewport]")
{
    CHECK(GetAssetBrowserNavigationWidth(24.0f, 48.0f, 6.0f, 4.0f) == 146.0f);
    CHECK(GetAssetBrowserNavigationWidth(32.0f, 64.0f, 8.0f, 6.0f) == 196.0f);

    CHECK_FALSE(NeedsAssetBrowserBreadcrumbScrollbar(300.0f, 300.0f));
    CHECK(NeedsAssetBrowserBreadcrumbScrollbar(300.1f, 300.0f));
    CHECK(NeedsAssetBrowserBreadcrumbScrollbar(1.0f, -20.0f));
}

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
        const AssetBrowserToolbarLayout toolbar = GetAssetBrowserToolbarLayout(320.0f + static_cast<float>(frame), frame % 2u == 0u);
        checksum += range.Begin + range.End + GetAssetBrowserRowCount(10000u, 6u) + GetAssetBrowserItemRow(9123u, 6u) + toolbar.ColumnCount;
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(checksum != 0u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
