#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptLayerMask.h"

namespace Crowny
{

    ScriptLayerMask::ScriptLayerMask() : ScriptObject() {}

    void ScriptLayerMask::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_LayerToName", (void*)&Internal_LayerToName);
        MetaData.ScriptClass->AddInternalCall("Internal_NameToLayer", (void*)&Internal_NameToLayer);
    }

    MonoString* ScriptLayerMask::Internal_LayerToName(int layer) { return nullptr; }

    int ScriptLayerMask::Internal_NameToLayer(MonoString* name) { return 0; }

} // namespace Crowny