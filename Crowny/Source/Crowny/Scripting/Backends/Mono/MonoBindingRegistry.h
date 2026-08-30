#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny
{
    class ScriptComponentBase;

    struct MonoBuiltinTypes
    {
        MonoClass* Entity = nullptr;
        MonoClass* EntityBehaviour = nullptr;
        MonoClass* MissingEntityBehaviour = nullptr;
    };

    struct MonoComponentBinding
    {
        std::function<ScriptComponentBase*(Entity)> Create;
    };

    struct MonoAssetBinding
    {
        std::function<ScriptAssetBase*(const AssetHandle<Asset>&, MonoObject*)> Create;
        MonoClass* ManagedClass = nullptr;
    };

    class MonoBindingRegistry : public Module<MonoBindingRegistry>
    {
    public:
        void LoadAssembly(const String& assemblyName);
        void Clear();

        MonoComponentBinding* FindComponent(MonoReflectionType* type);
        MonoAssetBinding* FindAsset(MonoReflectionType* type);
        MonoAssetBinding* FindAsset(AssetType type);
        const MonoBuiltinTypes& GetBuiltinTypes() const { return m_BuiltinTypes; }

    private:
        void RegisterComponents();
        void RegisterAssets();

        template <typename Component, class ScriptType> void RegisterComponent()
        {
            MonoReflectionType* reflectionType = MonoUtils::GetType(ScriptType::GetMetaData()->ScriptClass->GetInternalPtr());
            MonoComponentBinding binding;
            binding.Create = [](Entity entity) {
                MonoObject* managedInstance = ScriptType::GetMetaData()->ScriptClass->CreateInstance();
                return new ScriptType(managedInstance, entity);
            };
            m_ComponentBindings[reflectionType] = std::move(binding);
        }

        template <typename NativeAsset, class ScriptType> void RegisterAsset()
        {
            MonoReflectionType* reflectionType = MonoUtils::GetType(ScriptType::GetMetaData()->ScriptClass->GetInternalPtr());
            MonoAssetBinding binding;
            binding.Create = [](const AssetHandle<Asset>& handle, MonoObject* instance) {
                if (instance == nullptr)
                    instance = ScriptType::GetMetaData()->ScriptClass->CreateInstance();
                return static_cast<ScriptAssetBase*>(new ScriptType(instance, static_asset_cast<NativeAsset>(handle)));
            };
            binding.ManagedClass = ScriptType::GetMetaData()->ScriptClass;
            m_AssetBindings[reflectionType] = binding;
            m_AssetBindingsByType[static_cast<uint32_t>(NativeAsset::GetStaticType())] = std::move(binding);
        }

        MonoBuiltinTypes m_BuiltinTypes;
        UnorderedMap<MonoReflectionType*, MonoComponentBinding> m_ComponentBindings;
        UnorderedMap<MonoReflectionType*, MonoAssetBinding> m_AssetBindings;
        UnorderedMap<uint32_t, MonoAssetBinding> m_AssetBindingsByType;
    };
} // namespace Crowny
