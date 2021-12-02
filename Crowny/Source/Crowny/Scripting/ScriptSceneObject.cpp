#include "cwpch.h"

#include "Crowny/Scripting/ScriptSceneObject.h"

namespace Crowny
{

    ScriptSceneObjectBase::ScriptSceneObjectBase(MonoObject* instance) : PersistentScriptObjectBase(instance) {}

    ScriptSceneObjectBase::~ScriptSceneObjectBase() { CW_ENGINE_ASSERT(m_GCHandle == 0); }

    MonoObject* ScriptSceneObjectBase::GetManagedInstance() const
    {
        return MonoUtils::GetObjectFromGCHandle(m_GCHandle);
    }

    void ScriptSceneObjectBase::SetManagedInstance(MonoObject* instance)
    {
        CW_ENGINE_ASSERT(m_GCHandle == 0);
        m_GCHandle = MonoUtils::NewGCHandle(instance, false);
    }

    void ScriptSceneObjectBase::FreeManagedInstance()
    {
        if (m_GCHandle != 0)
        {
            MonoUtils::FreeGCHandle(m_GCHandle);
            m_GCHandle = 0;
        }
    }

} // namespace Crowny