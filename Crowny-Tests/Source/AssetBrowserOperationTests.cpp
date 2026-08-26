#include <catch2/catch_test_macros.hpp>

#include "Panels/AssetBrowserOperations.h"

using namespace Crowny;

TEST_CASE("Asset browser operations own deferred paths in submission order", "[Editor][AssetBrowser]")
{
    AssetBrowserOperationQueue operations;
    Path firstSource = "Assets/First.png";
    Path firstDestination = "Assets/Folder/First.png";

    REQUIRE(operations.EnqueueMove(firstSource, firstDestination));
    REQUIRE(operations.EnqueueMove("Assets/Second.png", "Assets/Folder/Second.png"));
    firstSource = "Assets/Changed.png";
    firstDestination = "Assets/Changed/First.png";

    CHECK(operations.Size() == 2u);
    const Vector<AssetBrowserMoveOperation> pending = operations.TakePending();
    REQUIRE(pending.size() == 2u);
    const AssetBrowserMoveOperation expectedFirst{ "Assets/First.png", "Assets/Folder/First.png" };
    const AssetBrowserMoveOperation expectedSecond{ "Assets/Second.png", "Assets/Folder/Second.png" };
    CHECK(pending[0] == expectedFirst);
    CHECK(pending[1] == expectedSecond);
    CHECK(operations.Empty());
}

TEST_CASE("Asset browser operations reject moves that cannot change the library", "[Editor][AssetBrowser]")
{
    AssetBrowserOperationQueue operations;

    CHECK_FALSE(operations.EnqueueMove({}, "Assets/Folder/Asset.png"));
    CHECK_FALSE(operations.EnqueueMove("Assets/Asset.png", {}));
    CHECK_FALSE(operations.EnqueueMove("Assets/Asset.png", "Assets/Asset.png"));
    CHECK(operations.Empty());
}

TEST_CASE("Successful deferred moves preserve exact-path selection", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png", "Assets/B/Foo.png" };
    const AssetBrowserMoveOperation operation{ "Assets/A/Foo.png", "Assets/Renamed/Foo.png" };

    const std::optional<Path> preferredSelection = RemapAssetBrowserSelectionAfterMove(selection, operation, true);

    REQUIRE(preferredSelection);
    CHECK(*preferredSelection == Path("Assets/Renamed/Foo.png"));
    CHECK_FALSE(selection.contains("Assets/A/Foo.png"));
    CHECK(selection.contains("Assets/Renamed/Foo.png"));
    CHECK(selection.contains("Assets/B/Foo.png"));
}

TEST_CASE("Directory moves remap selected descendants by path components", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/Foo", "Assets/Foo/One.png", "Assets/Foo/Nested/Two.png",
                                            "Assets/Foobar/Three.png", "Assets/Other/Foo/One.png" };
    const AssetBrowserMoveOperation operation{ "Assets/Foo", "Assets/Renamed" };

    const std::optional<Path> preferredSelection = RemapAssetBrowserSelectionAfterMove(selection, operation, true);

    REQUIRE(preferredSelection);
    CHECK(*preferredSelection == Path("Assets/Renamed"));
    CHECK(selection.contains("Assets/Renamed"));
    CHECK(selection.contains("Assets/Renamed/One.png"));
    CHECK(selection.contains("Assets/Renamed/Nested/Two.png"));
    CHECK(selection.contains("Assets/Foobar/Three.png"));
    CHECK(selection.contains("Assets/Other/Foo/One.png"));
    CHECK_FALSE(selection.contains("Assets/Foo"));
    CHECK_FALSE(selection.contains("Assets/Foo/One.png"));
    CHECK_FALSE(selection.contains("Assets/Foo/Nested/Two.png"));
    CHECK(selection.size() == 5u);
}

TEST_CASE("Failed deferred moves leave selection unchanged", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/A/Foo.png" };
    const AssetBrowserMoveOperation operation{ "Assets/A/Foo.png", "Assets/Renamed/Foo.png" };
    const UnorderedSet<Path, HashPath> expectedSelection{ "Assets/A/Foo.png" };

    CHECK_FALSE(RemapAssetBrowserSelectionAfterMove(selection, operation, false));
    CHECK(selection == expectedSelection);
}

TEST_CASE("Moving an unselected asset does not select its destination", "[Editor][AssetBrowser]")
{
    UnorderedSet<Path, HashPath> selection{ "Assets/Selected.png" };
    const AssetBrowserMoveOperation operation{ "Assets/Other.png", "Assets/Folder/Other.png" };
    const UnorderedSet<Path, HashPath> expectedSelection{ "Assets/Selected.png" };

    CHECK_FALSE(RemapAssetBrowserSelectionAfterMove(selection, operation, true));
    CHECK(selection == expectedSelection);
}
