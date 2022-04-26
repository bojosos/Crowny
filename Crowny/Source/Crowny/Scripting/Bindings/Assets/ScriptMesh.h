#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    class ScriptMesh : public TScriptAsset<ScriptMesh, Mesh>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Mesh");
        ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh);

    private:
    };

} // namespace Crowny