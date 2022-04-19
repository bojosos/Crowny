#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"

namespace Crowny
{

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

        MonoClass* ComponentClass = nullptr;
        MonoClass* EntityClass = nullptr;
        MonoClass* EntityBehaviour = nullptr;
        MonoClass* AssetClass = nullptr;

        MonoClass* SerializeFieldAttribute = nullptr;
        MonoClass* RangeAttribute = nullptr;
        MonoClass* StepAttribute = nullptr;
        MonoClass* ShowInInspectorAttribute = nullptr;
        MonoClass* HideInInspectorAttribute = nullptr;
        MonoClass* SerializableObjectAtrribute = nullptr;
        MonoClass* NotNullAttribute = nullptr;
        MonoClass* DontSerializeFieldAttribute = nullptr;
		MonoClass* RequireComponent = nullptr;

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
        uint32_t m_UniqueTypeId = 1;
        BuiltinScriptClasses m_Builtin;
        UnorderedMap<MonoReflectionType*, ComponentInfo>
          m_ComponentInfos; // TODO: Have to replace the reflection type with script meta or update the map after reload
        UnorderedMap<MonoReflectionType*, ScriptTypeInfo> m_ScriptTypeInfos;
        UnorderedMap<String, Ref<SerializableAssemblyInfo>> m_AssemblyInfos;
        UnorderedMap<String, MonoClass*> m_EntityBehaviourClasses;
    };
} // namespace Crowny