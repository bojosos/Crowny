#pragma once

#include "Crowny/Animation/Skeleton.h"
#include "Crowny/Import/SpecificImporter.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{

    class Material;

    struct MeshImportedBone
    {
        String Name;
        uint32_t ParentIndex = INVALID_BONE_INDEX;
        Transform LocalBindPose;
        glm::mat4 InverseBindPose{ 1.0f };
    };

    struct MeshImportResult
    {
        Ref<MeshData> Data;
        Vector<SubMesh> SubMeshes;
        Vector<uint32_t> MaterialIndices; // Parallel to SubMeshes, including repeated scene-node instances.
        Vector<MeshImportedBone> Bones;
        Ref<Skeleton> MeshSkeleton;
        Ref<MeshMorph> Morph;

        explicit operator bool() const { return Data != nullptr; }
    };

    class MeshImporter : public SpecificImporter
    {
    public:
        virtual ~MeshImporter() = default;

        virtual bool IsExtensionSupported(const String& ext) const override;
        virtual bool IsMagicNumSupported(uint8_t* num, uint32_t numSize) const override;

        virtual Ref<Asset> Import(const Path& path, Ref<const ImportOptions> importOptions) override;
        virtual Vector<Ref<Asset>> ImportAll(const Path& path, Ref<const ImportOptions> importOptions) override;

        static MeshImportResult Parse(const Path& path, const MeshImportOptions& importOptions);

        virtual Ref<ImportOptions> CreateImportOptions() const override;
    };

} // namespace Crowny
