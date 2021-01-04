#include "cwpch.h"

#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"
#include "Crowny/Scripting/CWMonoRuntime.h"

namespace Crowny
{

    void ScriptRuntime::Init()
    {
		Ref<Scene> scene = SceneManager::GetActiveScene();
        CWMonoAssembly* engine = CWMonoRuntime::GetCrownyAssembly();

		// Create managed entities
		CWMonoClass* entityClass = engine->GetClass("Crowny", "Entity");
		CW_ENGINE_ASSERT(entityClass, "Entity class does not exist");
		CWMonoField* entityPtr = entityClass->GetField("m_InternalPtr");

		scene->m_Registry.each([&](auto entity) {
			MonoObject* entityInstance = entityClass->CreateInstance();
			uint32_t handle = mono_gchandle_new(entityInstance, false); // TODO: delete this
			ScriptComponent::s_EntityComponents[(uint32_t)entity] = entityInstance;
			size_t ent = (size_t)entity;
			entityPtr->Set(entityInstance, &ent);
		});

		// Create managed transforms
		CWMonoClass* transformClass = engine->GetClass("Crowny", "Transform");
		CW_ENGINE_ASSERT(transformClass, "Transform class does not exist");
		CWMonoField* transformPtr = transformClass->GetField("m_InternalPtr");

		scene->m_Registry.view<TransformComponent>().each([&](const auto& entity, auto& tc)
		{
			MonoObject* transformInstance = transformClass->CreateInstance();
			uint32_t handle = mono_gchandle_new(transformInstance, false); // TODO: delete this!
			tc.ManagedInstance = transformInstance;
			size_t tmp = (size_t)&tc;
			transformPtr->Set(transformInstance, &tmp);
		});
		
		// Create managed script components
		scene->m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) 
		{
			if (!sc.Class)
				return;

			CWMonoField* scriptPtr = sc.Class->GetField("m_InternalPtr");
			sc.OnUpdate = sc.Class->GetMethod("Update");
			MonoObject* scriptInstance = sc.Class->CreateInstance();
			sc.Instance = scriptInstance;
			uint32_t handle = mono_gchandle_new(scriptInstance, false); // TODO: delete this store these
			size_t tmp = (size_t)&sc;
			scriptPtr->Set(scriptInstance, &tmp);

			CWMonoMethod* ctor = sc.Class->GetMethod(".ctor", 0);
			if (ctor)
				ctor->Call(scriptInstance);
			sc.OnStart = sc.Class->GetMethod("Start", 0);
		});
    }

    void ScriptRuntime::OnStart()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) {
			if (sc.OnStart)
				sc.OnStart->Call(sc.Instance);
        });
    }

    void ScriptRuntime::OnUpdate()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) {
			if (sc.OnUpdate)
		    	sc.OnUpdate->Call(sc.Instance);
        });
    }

    void ScriptRuntime::OnShutdown()
    {
        Ref<Scene> activeScene = SceneManager::GetActiveScene();
        activeScene->m_Registry.view<MonoScriptComponent>().each([&](entt::entity entity, MonoScriptComponent &sc) {
			if (sc.OnDestroy)
		    	sc.OnDestroy->Call(sc.Instance);
        });
    }

}