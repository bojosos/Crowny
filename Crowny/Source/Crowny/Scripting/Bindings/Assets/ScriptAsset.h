#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Scripting/Backends/Mono/MonoObjectIdentity.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    enum class ScriptAssetOwnership : uint8_t
    {
        EngineOwned,
        ManagedOwned
    };

    class ScriptAssetManager;

    class ScriptAssetBase : public PersistentScriptObjectBase
    {
    public:
        virtual AssetHandle<Asset> GetGenericHandle() const = 0;
        virtual void SetAsset(const AssetHandle<Asset>& asset) = 0;
        MonoObject* GetManagedInstance() const;

    protected:
        friend class ScriptAssetManager;

        ScriptAssetBase(MonoObject* instance);
        virtual ~ScriptAssetBase();

        virtual void NotifyAssetDestroyed() {}
        ::MonoClass* GetManagedAssetClass(uint32_t id);
        void SetManagedInstance(MonoObject* instance);
        void SetOwnership(ScriptAssetOwnership ownership);
        void FreeManagedInstance();

        void Destroy();

    private:
        uint32_t m_GCHandle = 0;
        ScriptAssetOwnership m_Ownership = ScriptAssetOwnership::EngineOwned;
    };

    template <class ScriptClass, class AssetType, class BaseType = ScriptAssetBase> class TScriptAsset : public ScriptObject<ScriptClass, BaseType>
    {
    public:
        AssetHandle<Asset> GetGenericHandle() const override { return m_Asset; }
        void SetAsset(const AssetHandle<Asset>& asset) override { m_Asset = static_asset_cast<AssetType>(asset); }
        const AssetHandle<AssetType>& GetHandle() const { return m_Asset; }

    protected:
        TScriptAsset(MonoObject* instance, const AssetHandle<AssetType>& asset) : ScriptObject<ScriptClass, BaseType>(instance), m_Asset(asset)
        {
            this->SetManagedInstance(instance);
            if (!MonoObjectIdentity::SetAsset(instance, asset.GetUUID()))
                CW_ENGINE_ERROR("Could not bind the managed asset identity.");
        }

        virtual ~TScriptAsset() = default;

        MonoObject* CreateManagedInstance(bool construct) override
        {
            MonoObject* managedInstance = ScriptClass::MetaData.ScriptClass->CreateInstance(construct);
            this->SetManagedInstance(managedInstance);
            if (!MonoObjectIdentity::SetAsset(managedInstance, m_Asset.GetUUID()))
                CW_ENGINE_ERROR("Could not restore the managed asset identity.");
            return managedInstance;
        }

        virtual void ClearManagedInstance() override { this->FreeManagedInstance(); }

        virtual void NotifyAssetDestroyed() override { this->FreeManagedInstance(); }

        virtual void OnManagedInstanceDeleted(bool refresh) override
        {
            this->FreeManagedInstance();
            if (!refresh)
                this->Destroy();
        }

        AssetHandle<AssetType> m_Asset;
    };

    // TODO: Implement handles for async loading assets in C#
    class ScriptAsset : public ScriptObject<ScriptAsset, ScriptAssetBase>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Asset");

    private:
        ScriptAsset(MonoObject* instance);

    private:
    };

} // namespace Crowny
