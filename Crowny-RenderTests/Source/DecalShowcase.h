#pragma once

#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    class Material;
    class Mesh;
    class Texture;
} // namespace Crowny

namespace Crowny::RenderTests
{
    // Exports source assets with matching metadata; never overwrites an existing project.
    class DecalShowcaseWriter
    {
    public:
        explicit DecalShowcaseWriter(const Path& project);
        bool WriteScene(const Ref<Scene>& scene, const String& name, String& error);

    private:
        void WriteMetadata(const Path& source, const UUID& id, AssetType type, const Ref<ImportOptions>& options = nullptr);
        void WriteMaterial(const AssetHandle<Material>& material);
        void WriteMesh(const AssetHandle<Mesh>& mesh);
        void WriteTexture(const AssetHandle<Texture>& texture);
        Path m_Project;
        UnorderedSet<UUID> m_Exported;
    };
} // namespace Crowny::RenderTests
