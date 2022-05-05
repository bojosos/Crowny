#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptLayerMask.h"

#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{

    ScriptLayerMask::ScriptLayerMask() : ScriptObject() {}

    void ScriptLayerMask::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_LayerToName", (void*)&Internal_LayerToName);
        MetaData.ScriptClass->AddInternalCall("Internal_NameToLayer", (void*)&Internal_NameToLayer);
    }

    MonoString* ScriptLayerMask::Internal_LayerToName(int layer)
    {
		if (layer < 0 || layer > 31)
			return nullptr;
		return MonoUtils::ToMonoString(Physics2D::Get().GetLayerName(layer));
    }

    int ScriptLayerMask::Internal_NameToLayer(MonoString* name)
    {
        String nativeName = MonoUtils::FromMonoString(name);
        for (int i = 0; i < 32; i++)
        {
            if (Physics2D::Get().GetLayerName(i) == nativeName)
				return i;
        }
        return -1;
    }

} // namespace Crowny