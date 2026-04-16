#include <catch2/catch_test_macros.hpp>

#include "cwpch.h"
#include "Crowny/Assets/AssetManager.h"

using namespace Crowny;

class MockAsset : public Asset
{
public:
    MockAsset() = default;
    virtual AssetType GetAssetType() const override { return AssetType::Mesh; } // Just use a dummy type
    static AssetType GetStaticType() { return AssetType::Mesh; }
};

TEST_CASE("Asset Handling", "[Assets]")
{
    AssetManager manager;
    
    SECTION("Manual Handle Creation")
    {
        Ref<MockAsset> asset = CreateRef<MockAsset>();
        UUID uuid = UuidGenerator::Generate();
        
        AssetHandle<MockAsset> handle = static_asset_cast<MockAsset>(manager.CreateAssetHandle(asset, uuid));
        
        CHECK(handle.IsLoaded());
        CHECK(handle.GetUUID() == uuid);
        CHECK(handle.Get() == asset.get());
    }

    SECTION("Reference Counting")
    {
        Ref<MockAsset> asset = CreateRef<MockAsset>();
        AssetHandle<MockAsset> handle1 = static_asset_cast<MockAsset>(manager.CreateAssetHandle(asset));
        
        {
            AssetHandle<MockAsset> handle2 = handle1;
            // GetRefCount not available on handle itself, it's in handle data
            CHECK(handle1.GetHandleData()->m_RefCount.load() == 2);
        }
        
        CHECK(handle1.GetHandleData()->m_RefCount.load() == 1);
    }

    SECTION("Weak Handles")
    {
        AssetHandle<MockAsset> handle = static_asset_cast<MockAsset>(manager.CreateAssetHandle(CreateRef<MockAsset>()));
        WeakAssetHandle<MockAsset> weak = handle.GetWeak();
        
        // CHECK(!weak.IsExpired()); // IsExpired not in API
        CHECK(weak.IsLoaded());
        
        handle = nullptr; // Release strong reference
    }
}
