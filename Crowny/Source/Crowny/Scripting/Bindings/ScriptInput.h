#pragma once

#include "Crowny/Scripting/ScriptObject.h"

namespace Crowny
{
    class ScriptInput : public ScriptObject<ScriptInput>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Input")
        ScriptInput();

    private:
        static void Internal_GetMousePosition(glm::vec2* out);
    };
} // namespace Crowny