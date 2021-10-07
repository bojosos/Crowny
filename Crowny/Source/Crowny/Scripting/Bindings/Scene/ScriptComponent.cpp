#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{
    UnorderedMap<uint32_t, MonoObject*> ScriptComponent::s_EntityComponents = {};
    UnorderedMap<MonoClass*, uint32_t> ScriptComponent::s_TypeMap = {};
    UnorderedMap<uint32_t, ScriptComponent::ComponentInfo> ScriptComponent::s_ComponentInfos;

    void ScriptComponent::InitRuntimeFunctions()
    {
        auto* assembly = CWMonoRuntime::GetCrownyAssembly();
        CWMonoClass* componentClass = assembly->GetClass("Crowny", "Component");

        componentClass->AddInternalCall("Internal_GetEntity", (void*)&Internal_GetEntity);
        componentClass->AddInternalCall("Internal_GetComponent", (void*)&Internal_GetComponent);
        componentClass->AddInternalCall("Internal_HasComponent", (void*)&Internal_HasComponent);
        componentClass->AddInternalCall("Internal_AddComponent", (void*)&Internal_AddComponent);

        s_TypeMap[assembly->GetClass("Crowny", "Transform")->GetInternalPtr()] =
          entt::type_info<TransformComponent>::id();
        s_TypeMap[assembly->GetClass("Crowny", "MeshRenderer")->GetInternalPtr()] =
          entt::type_info<MeshRendererComponent>::id();
        s_TypeMap[assembly->GetClass("Crowny", "SpriteRendererComponent")->GetInternalPtr()] =
          entt::type_info<SpriteRendererComponent>::id();
        s_TypeMap[assembly->GetClass("Crowny", "CameraComponent")->GetInternalPtr()] =
          entt::type_info<CameraComponent>::id();

        RegisterComponent<TransformComponent>();
        RegisterComponent<MeshRendererComponent>();
        RegisterComponent<SpriteRendererComponent>();
        RegisterComponent<CameraComponent>();
    }

    MonoObject* ScriptComponent::Internal_GetEntity(MonoScriptComponent* component)
    {
        return s_EntityComponents[(uint32_t)component->ComponentParent.GetHandle()];
    }

    MonoObject* ScriptComponent::Internal_GetComponent(MonoScriptComponent* component, MonoReflectionType* type)
    {
        MonoType* ctype = mono_reflection_type_get_type(type);
        MonoClass* cclass = mono_type_get_class(ctype);
        auto it = s_TypeMap.find(cclass);
        if (it == s_TypeMap.end())
        {
            const char* typeName = mono_type_get_name(ctype);
            CW_ENGINE_ERROR("{0} is not a valid component type!", typeName);
            return nullptr;
        }

        return s_ComponentInfos[it->second].GetCallback(component->ComponentParent);
    }

    bool ScriptComponent::Internal_HasComponent(MonoScriptComponent* component, MonoReflectionType* type)
    {
        MonoType* ctype = mono_reflection_type_get_type(type);
        MonoClass* cclass = mono_type_get_class(ctype);
        auto it = s_TypeMap.find(cclass);
        if (it == s_TypeMap.end())
        {
            const char* typeName = mono_type_get_name(ctype);
            CW_ENGINE_ERROR("{0} is not a valid component type!", typeName);
            return false;
        }

        return s_ComponentInfos[it->second].HasCallback(component->ComponentParent);
    }

    MonoObject* ScriptComponent::Internal_AddComponent(MonoScriptComponent* component, MonoReflectionType* type)
    {
        MonoType* ctype = mono_reflection_type_get_type(type);
        MonoClass* cclass = mono_type_get_class(ctype);
        auto it = s_TypeMap.find(cclass);
        if (it == s_TypeMap.end())
        {
            const char* typeName = mono_type_get_name(ctype);
            CW_ENGINE_ERROR("{0} is not a valid component type!", typeName);
            return nullptr;
        }

        return s_ComponentInfos[it->second].AddCallback(component->ComponentParent);
    }
} // namespace Crowny
