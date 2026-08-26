#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/AssetBrowserSelection.h"

using namespace Crowny;

namespace
{
    template <size_t N> Array<const Path*, N> PathPointers(const Array<Path, N>& paths)
    {
        Array<const Path*, N> pointers{};
        for (size_t index = 0; index < N; index++)
            pointers[index] = &paths[index];
        return pointers;
    }

    template <size_t N> Array<const Path*, N> SortedPathPointers(const Array<Path, N>& paths)
    {
        Array<const Path*, N> pointers = PathPointers(paths);
        std::sort(pointers.begin(), pointers.end(), [](const Path* lhs, const Path* rhs) { return *lhs < *rhs; });
        return pointers;
    }
} // namespace

TEST_CASE("Asset browser item IDs distinguish duplicate basenames", "[Editor][AssetBrowser]")
{
    const AssetBrowserItemId first = MakeAssetBrowserItemId("Assets/A/Foo.png");
    const AssetBrowserItemId second = MakeAssetBrowserItemId("Assets/B/Foo.png");

    CHECK(first != second);
}

TEST_CASE("Asset browser selection removes entries hidden by filtering", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png", "Assets/B/Foo.png", "Assets/Hidden/Foo.png" };
    const Array<Path, 3> visible{ "Assets/Bar.png", "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const Array<const Path*, 3> visiblePointers = PathPointers(visible);
    const Array<const Path*, 3> sortedPointers = SortedPathPointers(visible);

    const AssetBrowserSelectionResult result =
      ReconcileAssetBrowserSelection(selection, visiblePointers, sortedPointers, Path("Assets/B/Foo.png"), Path("Assets/A/Foo.png"));
    const UnorderedSet<Path, HashPath> expectedSelection{ "Assets/A/Foo.png", "Assets/B/Foo.png" };

    CHECK(result.SelectionChanged);
    CHECK(selection == expectedSelection);
    CHECK(result.StartIndex == 2u);
    CHECK(result.EndIndex == 1u);
}

TEST_CASE("Asset browser selection chooses a visible fallback anchor", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const Array<Path, 3> visible{ "Assets/Bar.png", "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const Array<const Path*, 3> visiblePointers = PathPointers(visible);
    const Array<const Path*, 3> sortedPointers = SortedPathPointers(visible);

    const AssetBrowserSelectionResult result =
      ReconcileAssetBrowserSelection(selection, visiblePointers, sortedPointers, Path("Assets/Missing.png"), Path("Assets/Missing.png"));

    CHECK_FALSE(result.SelectionChanged);
    CHECK(result.StartIndex == 1u);
    CHECK(result.EndIndex == 2u);
}

TEST_CASE("Asset browser selection clears stale state for an empty result", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png", "Assets/B/Foo.png" };

    const AssetBrowserSelectionResult result =
      ReconcileAssetBrowserSelection(selection, std::span<const Path* const>{}, std::span<const Path* const>{});

    CHECK(result.SelectionChanged);
    CHECK(selection.empty());
    CHECK(result.StartIndex == AssetBrowserSelectionResult::InvalidIndex);
    CHECK(result.EndIndex == 0u);
}

TEST_CASE("Stable asset browser selection reconciliation allocates nothing", "[Editor][AssetBrowser][Memory][Frame]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const Array<Path, 3> visible{ "Assets/Bar.png", "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const Array<const Path*, 3> visiblePointers = PathPointers(visible);
    const Array<const Path*, 3> sortedPointers = SortedPathPointers(visible);
    const std::optional<Path> start = Path("Assets/A/Foo.png");
    const std::optional<Path> end = Path("Assets/B/Foo.png");
    ReconcileAssetBrowserSelection(selection, visiblePointers, sortedPointers, start, end);

    uint32_t observedIndices = 0u;
    bool selectionChanged = false;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 240u; frame++)
    {
        const AssetBrowserSelectionResult result = ReconcileAssetBrowserSelection(selection, visiblePointers, sortedPointers, start, end);
        selectionChanged |= result.SelectionChanged;
        observedIndices += result.StartIndex + result.EndIndex;
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK_FALSE(selectionChanged);
    CHECK(observedIndices == 240u * 3u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
