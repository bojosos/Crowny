#include <catch2/catch_test_macros.hpp>

#include "Editor/AssetSaveTracker.h"

using namespace Crowny;

TEST_CASE("Interactive asset edits produce one save after the interaction finishes", "[Editor][Assets][SaveTracker]")
{
    AssetSaveTracker tracker;
    const Path material = "Assets/Material.cwmat";
    const Ref<Asset> asset = CreateRef<Asset>();

    tracker.Observe(material, asset, true, true, false);
    tracker.Observe(material, asset, true, true, false);
    CHECK_FALSE(tracker.TakeReady().has_value());

    tracker.Observe(material, asset, false, false, true);
    const std::optional<AssetSaveRequest> ready = tracker.TakeReady();
    REQUIRE(ready.has_value());
    CHECK(ready->Filepath == material);
    CHECK(ready->Value == asset);
    CHECK_FALSE(tracker.TakeReady().has_value());

    tracker.Resolve(material, true);
    CHECK_FALSE(tracker.IsPending(material));
}

TEST_CASE("Discrete asset edits are ready immediately", "[Editor][Assets][SaveTracker]")
{
    AssetSaveTracker tracker;
    const Path material = "Assets/Material.cwmat";
    const Ref<Asset> asset = CreateRef<Asset>();

    tracker.Observe(material, asset, true, false, false);
    const std::optional<AssetSaveRequest> ready = tracker.TakeReady();
    REQUIRE(ready.has_value());
    CHECK(ready->Filepath == material);
    CHECK_FALSE(tracker.TakeReady().has_value());
    tracker.Resolve(material, true);
    CHECK_FALSE(tracker.IsPending(material));
}

TEST_CASE("Failed asset saves wait for a later retry and retain other assets", "[Editor][Assets][SaveTracker]")
{
    AssetSaveTracker tracker;
    const Path first = "Assets/First.cwmat";
    const Path second = "Assets/Second.cwmat";
    const Ref<Asset> firstAsset = CreateRef<Asset>();
    const Ref<Asset> secondAsset = CreateRef<Asset>();

    tracker.Observe(first, firstAsset, true, false, false);
    const std::optional<AssetSaveRequest> firstRequest = tracker.TakeReady();
    REQUIRE(firstRequest.has_value());
    CHECK(firstRequest->Filepath == first);
    CHECK(firstRequest->Value == firstAsset);
    tracker.Resolve(first, false);
    CHECK(tracker.IsPending(first));
    CHECK_FALSE(tracker.TakeReady().has_value());

    tracker.Observe(second, secondAsset, true, false, false);
    const std::optional<AssetSaveRequest> secondRequest = tracker.TakeReady();
    REQUIRE(secondRequest.has_value());
    CHECK(secondRequest->Filepath == second);
    CHECK(secondRequest->Value == secondAsset);
    tracker.Resolve(second, true);
    CHECK(tracker.IsPending(first));
    CHECK_FALSE(tracker.IsPending(second));

    tracker.Flush();
    const std::optional<AssetSaveRequest> retry = tracker.TakeReady();
    REQUIRE(retry.has_value());
    CHECK(retry->Filepath == first);
    CHECK(retry->Value == firstAsset);
    tracker.Resolve(first, true);
    CHECK(tracker.GetPendingCount() == 0);
}
