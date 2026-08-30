#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{

    // Must match C# VertexAttributeFormat enum
    enum class ScriptVertexAttributeFormat : int32_t
    {
        Float32 = 1,
        Float16 = 2,
        UNorm8 = 3,
        SNorm8 = 4,
        UInt8 = 5,
        SInt8 = 6,
        UInt16 = 7,
        SInt16 = 8,
        UInt32 = 9,
        SInt32 = 10
    };

    // Must match C# VertexAttributeDescriptor struct layout
    struct ScriptVertexAttributeDescriptor
    {
        VertexAttribute Attribute;
        ScriptVertexAttributeFormat Format;
        int32_t Dimension;
        int32_t Stream;
    };

    class ScriptMesh : public TScriptAsset<ScriptMesh, Mesh>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "Mesh");
        ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh);

    private:
        static ShaderDataType DescriptorToShaderDataType(ScriptVertexAttributeFormat format, int32_t dimension);
    };

} // namespace Crowny
