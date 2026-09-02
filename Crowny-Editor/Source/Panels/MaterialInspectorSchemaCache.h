#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Renderer/Material.h"

#include <cstdint>

namespace Crowny
{
    /** Device-free reflected-layout source used by the editor cache and its tests. */
    class MaterialInspectorSchemaSource
    {
    public:
        virtual ~MaterialInspectorSchemaSource() = default;

        /** Must uniquely identify this exact layout across source instances. */
        virtual uint64_t GetLayoutVersion() const = 0;
        virtual const Material::BindingMap& GetBindings() const = 0;
        virtual const UniformDesc::TextureMap& GetTextureDescriptors() const = 0;
        virtual const UnorderedMap<String, AnnotationSet>& GetAnnotations(ShaderType shaderType) const = 0;
        virtual uint32_t GetBlockBindingSlot(const String& blockName) const = 0;
    };

    /** One selectable shader in the material inspector's shader picker. */
    struct MaterialShaderOption
    {
        String Name;
        UUID Uuid;
        Path AssetPath;              // Built-in shaders only: engine-relative asset path
        bool BuiltIn = false;
        bool MaterialCapable = true; // False for engine-internal shaders (compute, post-process, ...)
    };

    /**
     * Returns the options matching `search` (case-insensitive substring), built-in shaders first.
     * Internal built-in shaders are hidden unless `includeInternal` is set; project shaders always pass.
     */
    Vector<const MaterialShaderOption*> FilterMaterialShaderOptions(const Vector<MaterialShaderOption>& options, StringView search,
                                                                    bool includeInternal);

    class MaterialInspectorSchemaCache
    {
    public:
        const Vector<ShaderParameterDesc>& Resolve(const Material& material);
        const Vector<ShaderParameterDesc>& Resolve(const MaterialInspectorSchemaSource& source);
        void Reset();

    private:
        static Vector<ShaderParameterDesc> Build(const MaterialInspectorSchemaSource& source);

        uint64_t m_LayoutVersion = 0;
        bool m_HasLayout = false;
        Vector<ShaderParameterDesc> m_Parameters;
    };
} // namespace Crowny
