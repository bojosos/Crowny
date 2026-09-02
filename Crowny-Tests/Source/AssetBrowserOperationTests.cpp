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

TEST_CASE("Asset browser plans external imports into the browsed folder", "[Editor][AssetBrowser]")
{
    const Vector<Path> dropped{ "C:/Downloads/model.glb", "C:/Downloads/textures", "C:/Downloads/model.glb", "C:/Downloads/nested/model.glb",
                                "C:/Project/Assets/demo/already.png", "C:/Project/Assets", "" };
    const auto isDirectory = [](const Path& path) { return path.filename() == "textures" || path == Path("C:/Project/Assets"); };
    const auto makeUnique = [](const Path& path) { return path.filename() == "textures" ? path.parent_path() / "textures (1)" : path; };

    const Vector<AssetBrowserImportOperation> operations = PlanAssetBrowserImports(dropped, "C:/Project/Assets/demo", isDirectory, makeUnique);

    REQUIRE(operations.size() == 3u);
    CHECK(operations[0] == AssetBrowserImportOperation{ "C:/Downloads/model.glb", "C:/Project/Assets/demo/model.glb", false });
    CHECK(operations[1] == AssetBrowserImportOperation{ "C:/Downloads/textures", "C:/Project/Assets/demo/textures (1)", true });
    // A second source with the same name may not overwrite the first planned copy.
    CHECK(operations[2].Source == Path("C:/Downloads/nested/model.glb"));
    CHECK(operations[2].Destination == Path("C:/Project/Assets/demo/model (1).glb"));
    CHECK_FALSE(operations[2].IsDirectory);
}

TEST_CASE("Asset browser folder fingerprints track listing changes", "[Editor][AssetBrowser]")
{
    AssetBrowserFolderFingerprint baseline;
    baseline.AddEntry(0x1000, 100, 1, 512, true);
    baseline.AddEntry(0x2000, 100, 0, 0, false);

    AssetBrowserFolderFingerprint same;
    same.AddEntry(0x1000, 100, 1, 512, true);
    same.AddEntry(0x2000, 100, 0, 0, false);
    CHECK(same == baseline);

    AssetBrowserFolderFingerprint added = same;
    added.AddEntry(0x3000, 100, 0, 16, true);
    CHECK(added != baseline);
    CHECK(added.Count == 3u);

    AssetBrowserFolderFingerprint reimported;
    reimported.AddEntry(0x1000, 100, 2, 512, true);
    reimported.AddEntry(0x2000, 100, 0, 0, false);
    CHECK(reimported != baseline);

    AssetBrowserFolderFingerprint modified;
    modified.AddEntry(0x1000, 101, 1, 512, true);
    modified.AddEntry(0x2000, 100, 0, 0, false);
    CHECK(modified != baseline);

    AssetBrowserFolderFingerprint replaced;
    replaced.AddEntry(0x1001, 100, 1, 512, true);
    replaced.AddEntry(0x2000, 100, 0, 0, false);
    CHECK(replaced != baseline);
}

TEST_CASE("Asset browser drop hit test uses the content rectangle", "[Editor][AssetBrowser]")
{
    STATIC_CHECK(IsAssetBrowserPointInside(10.0f, 10.0f, 0.0f, 0.0f, 100.0f, 50.0f));
    STATIC_CHECK(IsAssetBrowserPointInside(0.0f, 0.0f, 0.0f, 0.0f, 100.0f, 50.0f));
    STATIC_CHECK_FALSE(IsAssetBrowserPointInside(100.0f, 10.0f, 0.0f, 0.0f, 100.0f, 50.0f));
    STATIC_CHECK_FALSE(IsAssetBrowserPointInside(10.0f, -1.0f, 0.0f, 0.0f, 100.0f, 50.0f));
    STATIC_CHECK_FALSE(IsAssetBrowserPointInside(10.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f));
}
