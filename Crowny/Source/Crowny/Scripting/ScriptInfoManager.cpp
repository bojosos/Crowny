#include "cwpch.h"

#include "Crowny/Scripting/ScriptInfoManager.h"

#include "Crowny/Scripting/Mono/MonoManager.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"

namespace Crowny
{

    ScriptInfoManager::ScriptInfoManager()
    {
        RegisterComponent<TransformComponent, ScriptTransform>();
        RegisterComponent<CameraComponent, ScriptCamera>();
        RegisterComponent<MonoScriptComponent, ScriptEntityBehaviour>();
        RegisterComponent<AudioSourceComponent, ScriptAudioSource>();
        RegisterComponent<AudioListenerComponent, ScriptAudioListener>();
    }

    void ScriptInfoManager::InitializeTypes()
    {

        MonoAssembly* corlib = MonoManager::Get().GetAssembly("corlib");
        if (corlib == nullptr)
            CW_ENGINE_ERROR("Corlib assembly not loaded.");
        MonoAssembly* crownyAssembly = MonoManager::Get().GetAssembly(CROWNY_ASSEMBLY);
        if (crownyAssembly == nullptr)
            CW_ENGINE_ERROR("Crowny assembly not loaded.");

        m_Builtin.SystemTypeClass = corlib->GetClass("System", "Type");
        if (m_Builtin.SystemTypeClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Type class.");
        m_Builtin.SystemArrayClass = corlib->GetClass("System", "Array");
        if (m_Builtin.SystemArrayClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Array class.");
        m_Builtin.SystemGenericDictionaryClass = corlib->GetClass("System.Collections.Generic", "Dictionary`2");
        if (m_Builtin.SystemGenericDictionaryClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.Dictionary<TKey, TValue> class.");
        m_Builtin.SystemGenericListClass = corlib->GetClass("System.Collections.Generic", "List`1");
        if (m_Builtin.SystemGenericListClass == nullptr)
            CW_ENGINE_ERROR("Cannot find System.Collections.Generic.List<T> class.");

        m_Builtin.ComponentClass = crownyAssembly->GetClass(CROWNY_NS, "Component");
        if (m_Builtin.ComponentClass == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.Component class.", CROWNY_NS);

        m_Builtin.EntityClass = crownyAssembly->GetClass(CROWNY_NS, "Entity");
        if (m_Builtin.EntityClass == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.EntityClass class.", CROWNY_NS);

        m_Builtin.EntityBehaviour = crownyAssembly->GetClass(CROWNY_NS, "EntityBehaviour");
        if (m_Builtin.EntityBehaviour == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.EntityBehaviour class.", CROWNY_NS);

        m_Builtin.SerializeFieldAttribute = crownyAssembly->GetClass(CROWNY_NS, "SerializeField");
        if (m_Builtin.SerializeFieldAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.SerializeField class.", CROWNY_NS);

        m_Builtin.RangeAttribute = crownyAssembly->GetClass(CROWNY_NS, "Range");
        if (m_Builtin.RangeAttribute == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.Range class.", CROWNY_NS);

        m_Builtin.ShowInInspector = crownyAssembly->GetClass(CROWNY_NS, "ShowInInspector");
        if (m_Builtin.ShowInInspector == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.ShowInInspector class.", CROWNY_NS);

        m_Builtin.HideInInspector = crownyAssembly->GetClass(CROWNY_NS, "HideInInspector");
        if (m_Builtin.HideInInspector == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.HideInInspector class.", CROWNY_NS);

        m_Builtin.ScriptUtils = crownyAssembly->GetClass(CROWNY_NS, "ScriptUtils");
        if (m_Builtin.ScriptUtils == nullptr)
            CW_ENGINE_ERROR("Cannot find {0}.ScriptUtils class.", CROWNY_NS);
    }

    ComponentInfo* ScriptInfoManager::GetComponentInfo(MonoReflectionType* type)
    {
        auto findIter = m_ComponentInfos.find(type);
        if (findIter != m_ComponentInfos.end())
            return &findIter->second;
        return nullptr;
    }

} // namespace Crowny