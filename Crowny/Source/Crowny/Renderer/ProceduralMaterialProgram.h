#pragma once

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    // A cooked opaque PBR variant. The procedure can originate in GLSL, OSL, or
    // another compiler implementing CrownyProceduralTexture.glslinc. Instances
    // share the shader/pipeline and own independent reflected material parameters.
    class ProceduralMaterialProgram
    {
    public:
        // The cook preserves the original GLSL module for reflection because
        // SPIR-V linking can merge block types and erase distinct member names.
        // Only vertexSpirv and fragmentSpirv execute on the GPU.
        bool Initialize(const Vector<uint8_t>& vertexSpirv, const Vector<uint8_t>& fragmentSpirv, const Vector<uint8_t>& fragmentInterfaceSpirv,
                        String& error);
        bool Initialize(const Path& package, String& error);
        // CPU-only package loading for asset import workers. GPU pipelines are created on use.
        static Ref<Shader> LoadShader(const Path& package, String& error);
        Ref<Material> CreateMaterial() const;

    private:
        static Ref<Shader> CreateShader(const Vector<uint8_t>& vertexSpirv, const Vector<uint8_t>& fragmentSpirv,
                                        const Vector<uint8_t>& fragmentInterfaceSpirv, const String& parameters, String& error);
        AssetHandle<Shader> m_Shader;
    };
} // namespace Crowny
