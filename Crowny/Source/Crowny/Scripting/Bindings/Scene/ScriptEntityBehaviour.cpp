#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/ScriptSerializer.h"

namespace Crowny
{
    ScriptEntityBehaviour::ScriptEntityBehaviour(MonoObject* instance, Entity entity, MonoScript& script)
      : ScriptObject(instance), m_TypeMissing(false)
    {
        m_Entity = entity;
        m_Assembly = script.GetAssemblyName();
        m_Namespace = script.GetNamespace();
        m_TypeName = script.GetTypeName();
        m_ScriptInstanceId = script.InstanceId;
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        script.OnInitialize(this);
    }

    ScriptObjectBackupData ScriptEntityBehaviour::BeginRefresh()
    {
        // Back up only the matching MonoScript for this behaviour (not the whole component)
        MonoScriptComponent& msc = m_Entity.GetComponent<MonoScriptComponent>();
        MonoScript* script = msc.FindScript(m_ScriptInstanceId);
        if (script == nullptr)
            return {};

        const PersistedScriptState state = script->CapturePersistedState();
        ScriptObjectBackupData data;
        if (state.Fields == nullptr)
            return data;
        Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>();
        BinaryDataStreamOutputArchive archive(stream);
        archive(state.Fields);
        data.Data.resize(stream->Size());
        if (!data.Data.empty())
            std::memcpy(data.Data.data(), stream->Data(), data.Data.size());
        return data;
    }

    void ScriptEntityBehaviour::EndRefresh(const ScriptObjectBackupData& data)
    {
        // Restore only the matching MonoScript for this behaviour
        MonoScriptComponent& msc = m_Entity.GetComponent<MonoScriptComponent>();
        MonoScript* script = msc.FindScript(m_ScriptInstanceId);
        if (script == nullptr)
            return;

        script->OnInitialize(this);
        Ref<SerializableObject> fields;
        if (!data.Data.empty())
        {
            ScriptSerializationSceneScope sceneScope(m_Entity.GetScene());
            Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>(const_cast<uint8_t*>(data.Data.data()), data.Data.size());
            BinaryDataStreamInputArchive archive(stream);
            archive(fields);
        }
        script->ApplyPersistedFields(std::move(fields));
    }

    MonoObject* ScriptEntityBehaviour::CreateManagedInstance(bool construct)
    {
        Ref<SerializableObjectInfo> currentObjInfo = nullptr;

        MonoObject* instance;
        if (!ScriptInfoManager::Get().GetSerializableObjectInfo(m_Assembly, m_Namespace, m_TypeName, currentObjInfo))
        {
            m_TypeMissing = true;
            instance = ScriptInfoManager::Get().GetBuiltinClasses().MissingEntityBehaviour->CreateInstance(true);
        }
        else
        {
            m_TypeMissing = false;
            instance = currentObjInfo->m_MonoClass->CreateInstance(construct);
        }

        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
        return instance;
    }

    void ScriptEntityBehaviour::ClearManagedInstance() { FreeManagedInstance(); }

    void ScriptEntityBehaviour::OnManagedInstanceDeleted(bool assemblyRefresh)
    {
        m_GCHandle = 0;

        if (!assemblyRefresh && GetNativeEntity())
            ScriptSceneObjectManager::Get().DestroyScriptComponent(this, m_ScriptInstanceId);
    }

    void ScriptEntityBehaviour::NotifyDestroyed()
    {
        MonoObject* instance = GetManagedInstance();
        if (instance != nullptr && MetaData.CachedPtrField != nullptr)
        {
            ScriptEntityBehaviour* cleared = nullptr;
            MetaData.CachedPtrField->Set(instance, &cleared);
        }
        FreeManagedInstance();
    }

    void ScriptEntityBehaviour::InitRuntimeData() {}
} // namespace Crowny
