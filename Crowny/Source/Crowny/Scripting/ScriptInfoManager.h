#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

namespace Crowny
{
    class ScriptComponentBase;
    struct BuiltinScriptClasses
    {
        MonoClass* SystemArrayClass = nullptr;
        MonoClass* SystemGenericListClass = nullptr;
        MonoClass* SystemGenericDictionaryClass = nullptr;
        MonoClass* SystemTypeClass = nullptr;
        MonoClass* SystemSerializable = nullptr;

        MonoClass* Vector2 = nullptr;
        MonoClass* Vector3 = nullptr;
        MonoClass* Vector4 = nullptr;
        MonoClass* Matrix4 = nullptr;
        MonoClass* Color = nullptr;

        MonoClass* Component = nullptr;
        MonoClass* Entity = nullptr;
        MonoClass* EntityBehaviour = nullptr;
        MonoClass* MissingEntityBehaviour = nullptr;
        MonoClass* AssetClass = nullptr;

        MonoClass* SerializeFieldAttribute = nullptr;
        MonoClass* RangeAttribute = nullptr;
        MonoClass* StepAttribute = nullptr;
        MonoClass* ShowInInspectorAttribute = nullptr;
        MonoClass* HideInInspectorAttribute = nullptr;
        MonoClass* RunInEditorAttribute = nullptr;
        MonoClass* ButtonAttribute = nullptr;
        MonoClass* SerializeObjectAttribute = nullptr;
        MonoClass* NotNullAttribute = nullptr;
        MonoClass* DontSerializeFieldAttribute = nullptr;
        MonoClass* RequireComponentAttribute = nullptr;
        MonoClass* MultilineAttribute = nullptr;
        MonoClass* ColorPaletteAttribute = nullptr;
        MonoClass* ColorUsageAttribute = nullptr;
        MonoClass* EnumQuickTabsAttribute = nullptr;

        MonoClass* DropdownAttribute = nullptr;
        MonoClass* LabelAttribute = nullptr;
        MonoClass* FilepathAttribute = nullptr;
        MonoClass* FolderPathAttribute = nullptr;
        MonoClass* ReadOnlyAttribute = nullptr;
        MonoClass* HeaderAttribute = nullptr;
        MonoClass* TooltipAttribute = nullptr;

        MonoClass* ScriptUtils = nullptr;
        MonoClass* ScriptCompiler = nullptr;
    };

    struct ComponentInfo
    {
        std::function<ScriptComponentBase*(Entity)> AddCallback;
        std::function<bool(Entity)> HasCallback;
        std::function<void(Entity)> RemoveCallback;
        std::function<ScriptComponentBase*(Entity)> GetCallback;
        std::function<ScriptComponentBase*(Entity)> CreateCallback;

        const ScriptMeta* Metadata;
    };

    struct AssetInfo
    {
        std::function<ScriptAssetBase*(const AssetHandle<Asset>&, MonoObject*)> CreateCallback;
        MonoClass* AssetClass;
        AssetType Type;
    };

    struct ScriptTypeInfo
    {
        const ScriptMeta* Metadata;
        MonoClass* ScriptClass;
        uint32_t TypeId;
    };

    class ScriptInfoManager : public Module<ScriptInfoManager>
    {
    public:
        ScriptInfoManager();
        ~ScriptInfoManager() = default;
        void ClearAssemblyInfo();

        ComponentInfo* GetComponentInfo(MonoReflectionType* type);
        AssetInfo* GetAssetInfo(MonoReflectionType* type);
        AssetInfo* GetAssetInfo(AssetType id);
        AssetInfo* GetAssetInfo(uint32_t id);

        void InitializeTypes();

        bool IsBasicType(MonoClass* klass);
        const BuiltinScriptClasses& GetBuiltinClasses() { return m_Builtin; }

        void LoadAssemblyInfo(const String& assemblyName);
        const UnorderedMap<String, MonoClass*> GetEntityBehaviours() const { return m_EntityBehaviourClasses; }

        ScriptTypeInfo* GetSerializableTypeInfo(MonoReflectionType* reflType);

        bool GetSerializableObjectInfo(const String& ns, const String& name, Ref<SerializableObjectInfo>& outInfo);

    private:
        Ref<SerializableTypeInfo> GetTypeInfo(MonoClass* monoClass);
        void RegisterComponents();
        void RegisterAssets();

        template <typename Component, class ScriptType> void RegisterComponent()
        {
            MonoReflectionType* reflType = MonoUtils::GetType(ScriptType::GetMetaData()->ScriptClass->GetInternalPtr());
            ComponentInfo componentInfo;
            componentInfo.AddCallback = [reflType](Entity entity) {
                return ScriptSceneObjectManager::Get().GetScriptComponent(entity, entity.AddComponent<Component>(),
                                                                          reflType);
            };
            componentInfo.GetCallback = [reflType](Entity entity) {
                return ScriptSceneObjectManager::Get().GetScriptComponent(entity, entity.GetComponent<Component>(),
                                                                          reflType);
            };
            componentInfo.HasCallback = [](Entity entity) { return entity.HasComponent<Component>(); };
            componentInfo.RemoveCallback = [](Entity entity) { entity.RemoveComponent<Component>(); };
            componentInfo.CreateCallback = [](Entity entity) {
                MonoObject* managedInstance = ScriptType::GetMetaData()->ScriptClass->CreateInstance();
                return new ScriptType(managedInstance, entity);
            };
            componentInfo.Metadata = ScriptType::GetMetaData();
            m_ComponentInfos[reflType] = componentInfo;
        }

        template <typename AssetType, class ScriptType> void RegisterAsset()
        {
            MonoReflectionType* reflType = MonoUtils::GetType(ScriptType::GetMetaData()->ScriptClass->GetInternalPtr());
            AssetInfo assetInfo;
            assetInfo.CreateCallback = [reflType](const AssetHandle<Asset>& handle, MonoObject* instance) {
                if (instance == nullptr)
                    instance = ScriptType::GetMetaData()->ScriptClass->CreateInstance();
                AssetHandle<AssetType> cast = static_asset_cast<AssetType>(handle);
                ScriptType* scriptAsset = new ScriptType(instance, cast);
                return (ScriptAssetBase*)scriptAsset;
            };
            assetInfo.Type = AssetType::GetStaticType();
            assetInfo.AssetClass = ScriptType::GetMetaData()->ScriptClass;
            m_AssetInfos[reflType] = assetInfo;
            m_AssetInfosById[(uint32_t)AssetType::GetStaticType()] = assetInfo;
        }

        template <typename SerializableType, class ScriptType> void RegisterSerializableType()
        {
            MonoReflectionType* reflType = MonoUtils::GetType(ScriptType::GetMetaData()->ScriptClass->GetInternalPtr());
            ScriptTypeInfo scriptTypeInfo;
            scriptTypeInfo.Metadata = ScriptType::GetMetaData();
            scriptTypeInfo.ScriptClass = nullptr;
            scriptTypeInfo.TypeId = GetRuntimeId<SerializableType>();
            m_ScriptTypeInfos[reflType] = scriptTypeInfo;
        }

        void ClearScriptObjects();

    private:
        bool m_BaseTypesInitialized;
        BuiltinScriptClasses m_Builtin;
        UnorderedMap<MonoReflectionType*, ComponentInfo> m_ComponentInfos;
        UnorderedMap<MonoReflectionType*, AssetInfo> m_AssetInfos;
        UnorderedMap<uint32_t, AssetInfo> m_AssetInfosById;
        UnorderedMap<MonoReflectionType*, ScriptTypeInfo> m_ScriptTypeInfos;
        UnorderedMap<String, Ref<SerializableAssemblyInfo>> m_AssemblyInfos;
        UnorderedMap<String, MonoClass*> m_EntityBehaviourClasses;
    };
} // namespace Crowny