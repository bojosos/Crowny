#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptLayerMask : public ScriptObject<ScriptLayerMask>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "LayerMask");
        ScriptLayerMask();

    private:
        static MonoString* Internal_LayerToName(int layer);
        static int Internal_NameToLayer(MonoString* name);
    };
} // namespace Crowny