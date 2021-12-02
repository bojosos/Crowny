#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Ecs/Entity.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/ScriptComponent.h"
#include "Crowny/Scripting/ScriptSceneObjectManager.h"

namespace Crowny
{

    struct BuiltinScriptClasses
    {
        MonoClass* SystemArrayClass = nullptr;
        MonoClass* SystemGenericListClass = nullptr;
        MonoClass* SystemGenericDictionaryClass = nullptr;
        MonoClass* SystemTypeClass = nullptr;

        MonoClass* ComponentClass = nullptr;
        MonoClass* EntityClass = nullptr;
        MonoClass* EntityBehaviour = nullptr;

        MonoClass* SerializeFieldAttribute = nullptr;
        MonoClass* RangeAttribute = nullptr;
        MonoClass* ShowInInspector = nullptr;
        MonoClass* HideInInspector = nullptr;
        MonoClass* ScriptUtils = nullptr;
    };

    struct ComponentInfo
    {
        std::function<ScriptComponentBase*(Entity)> AddCallback;
        std::function<bool(Entity)> HasCallback;
        std::function<ScriptComponentBase*(Entity)> GetCallback;
        std::function<ScriptComponentBase*(Entity)> CreateCallback;
    };

    struct BuiltinTypeMappings
    {
        Vector<ComponentInfo> Components;
    };

    class ScriptInfoManager : public Module<ScriptInfoManager>
    {
    public:
        ScriptInfoManager();
        ~ScriptInfoManager() = default;
        void LoadAssemblyInfo(const String& assemblyName, const BuiltinTypeMappings& typeMappings);
        void ClearAssemblyInfo();
        ComponentInfo* GetComponentInfo(MonoReflectionType* type);
        void InitializeTypes();

        const BuiltinScriptClasses& GetBuiltinClasses() { return m_Builtin; }

    private:
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
            componentInfo.HasCallback = [reflType](Entity entity) { return entity.HasComponent<Component>(); };
            componentInfo.CreateCallback = [](Entity entity) {
                MonoObject* managedInstance = ScriptType::GetMetaData()->ScriptClass->CreateInstance();
                return new ScriptType(managedInstance, entity);
            };
            m_ComponentInfos[reflType] = componentInfo;
        }

    private:
        BuiltinScriptClasses m_Builtin;
        UnorderedMap<MonoReflectionType*, ComponentInfo> m_ComponentInfos;
    };
} // namespace Crowny