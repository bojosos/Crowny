#pragma once

#include "Crowny/Renderer/DecalRenderer.h"

#include <span>

namespace Crowny
{
    // Render-thread mirror of one DecalWorld. Physical buffer/descriptor lifetime
    // follows the backend's in-flight resource retention; logical slots use generations.
    class GpuDecalWorld
    {
    public:
        void Apply(std::span<const RenderableDecalChange> changes);
        bool Prepare(GpuScene* scene, DecalRenderStats& stats);
        bool TryGet(DecalHandle handle, RenderableDecal& record) const;
        bool GetReference(DecalHandle handle, glm::uvec2& reference) const;
        glm::uvec2 GetMaterialReference(DecalHandle handle) const;
        uint32_t GetActiveCount() const { return m_ActiveCount; }
        uint32_t GetMaterialCount() const { return static_cast<uint32_t>(m_MaterialIndices.size()); }
        const Ref<GenericGpuBuffer>& GetDecalBuffer() const { return m_Decals; }
        const Ref<GenericGpuBuffer>& GetMaterialBuffer() const { return m_Materials; }
        const Vector<Ref<Texture>>& GetTextures() const { return m_Textures; }

    private:
        struct Slot
        {
            RenderableDecal Source;
            glm::uvec2 Material{ 0 };
            uint32_t Generation = 0;
            bool Alive = false;
        };
        struct MaterialSlot
        {
            UUID Id;
            GpuDecalMaterial Source;
            std::array<Ref<Texture>, 5> Textures;
            uint32_t Generation = 1;
            uint32_t References = 0;
            bool Resident = true;
        };
        glm::uvec2 AcquireMaterial(const RenderableDecal& source);
        void ReleaseMaterial(glm::uvec2 handle);
        bool IsAlive(DecalHandle handle) const;
        bool Upload(Ref<GenericGpuBuffer>& buffer, Vector<uint8_t>& previous, const void* data, uint32_t count, uint32_t stride,
                    DecalRenderStats& stats);

        Vector<Slot> m_Slots;
        Vector<MaterialSlot> m_MaterialSlots;
        Vector<uint32_t> m_FreeMaterials;
        UnorderedMap<UUID, uint32_t> m_MaterialIndices;
        Vector<GpuDecalSlot> m_DecalData;
        Vector<GpuDecalMaterialSlot> m_MaterialData;
        Vector<Ref<Texture>> m_Textures;
        Ref<GenericGpuBuffer> m_Decals, m_Materials;
        Vector<uint8_t> m_PreviousDecals, m_PreviousMaterials;
        uint32_t m_ActiveCount = 0;
    };
} // namespace Crowny
