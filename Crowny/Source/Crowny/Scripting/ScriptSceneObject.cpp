#include "cwpch.h"

#include "Crowny/Scripting/ScriptSceneObject.h"

namespace Crowny
{

    ScriptSceneObjectBase::ScriptSceneObjectBase(MonoObject* instance) : PersistentScriptObjectBase(instance) {}

    ScriptSceneObjectBase::~ScriptSceneObjectBase() { CW_ENGINE_ASSERT(m_GCHandle == 0); }

    MonoObject* ScriptSceneObjectBase::GetManagedInstance() const { return m_GCHandle != 0 ? MonoUtils::GetObjectFromGCHandle(m_GCHandle) : nullptr; }

    void ScriptSceneObjectBase::SetManagedInstance(MonoObject* instance)
    {
        CW_ENGINE_ASSERT(m_GCHandle == 0);
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
    }

    void ScriptSceneObjectBase::FreeManagedInstance()
    {
        if (m_GCHandle != 0)
        {
            MonoObject* instance = GetManagedInstance();
            MonoField* cachedPtr = ScriptObjectWrapper::GetMetaData()->CachedPtrField;
            if (instance != nullptr && cachedPtr != nullptr)
            {
                ScriptSceneObjectBase* cleared = nullptr;
                cachedPtr->Set(instance, &cleared);
            }
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

} // namespace Crowny
