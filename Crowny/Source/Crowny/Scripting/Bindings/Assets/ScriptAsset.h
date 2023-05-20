#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{

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
        void FreeManagedInstance();
        ;
        void Destroy();

    private:
        uint32_t m_GCHandle = 0;
    };

    template <class ScriptClass, class AssetType, class BaseType = ScriptAssetBase>
    class TScriptAsset : public ScriptObject<ScriptClass, BaseType>
    {
    public:
        AssetHandle<Asset> GetGenericHandle() const override { return m_Asset; }
        void SetAsset(const AssetHandle<Asset>& asset) override { m_Asset = static_asset_cast<AssetType>(asset); }
        const AssetHandle<AssetType>& GetHandle() const { return m_Asset; }

    protected:
        TScriptAsset(MonoObject* instance, const AssetHandle<AssetType>& asset)
          : ScriptObject<ScriptClass, BaseType>(instance), m_Asset(asset)
        {
            this->SetManagedInstance(instance);
        }

        virtual ~TScriptAsset() = default;

        MonoObject* CreateManagedInstance(bool construct) override
        {
            MonoObject* managedInstance = ScriptClass::MetaData.ScriptClass->CreateInstance(construct);
            this->SetManagedInstance(managedInstance);
            return managedInstance;
        }

        virtual void ClearManagedInstance() override { this->FreeManagedInstance(); }

        virtual void NotifyAssetDestroyed() override { this->FreeManagedInstance(); }

        virtual void OnManagedInstanceDeleted(bool refresh) override
        {
            this->FreeManagedInstance();
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
        static MonoString* Internal_GetName(ScriptAssetBase* nativeInstance);
        static void Internal_GetUUID(ScriptAssetBase* nativeInstance, UUID42* uuid);
    };

} // namespace Crowny