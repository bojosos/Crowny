#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"

namespace Crowny
{

    class EnvironmentMap : public Asset
    {
    public:
        struct Settings
        {
            uint32_t CubemapResolution = 1024;
            uint32_t IrradianceResolution = 64;
            uint32_t PrefilteredResolution = 512;
            uint32_t PrefilterSamples = 1024;
        };

        EnvironmentMap() = default;
        explicit EnvironmentMap(const Path& hdrPath);
        EnvironmentMap(const Path& hdrPath, const Settings& settings);
        ~EnvironmentMap() override = default;

        AssetType GetAssetType() const override { return AssetType::EnvironmentMap; }
        static AssetType GetStaticType() { return AssetType::EnvironmentMap; }

        const Ref<Texture>& GetEnvironmentCubemap() const { return m_EnvironmentCubemap; }
        const Ref<Texture>& GetIrradianceMap() const { return m_IrradianceMap; }
        const Ref<Texture>& GetPrefilteredMap() const { return m_PrefilteredMap; }
        const std::array<glm::vec4, 9>& GetDiffuseSh() const { return m_DiffuseSh; }
        const Settings& GetSettings() const { return m_Settings; }

        bool IsValid() const { return m_EnvironmentCubemap != nullptr; }

    private:
        CW_SERIALIZABLE(EnvironmentMap);
        void GenerateFromHDR(const Path& hdrPath);
        void GenerateIrradianceCube();
        void GeneratePrefilteredCube();
        void ComputeDiffuseSh(const float* pixels, uint32_t width, uint32_t height);

        // Shared cube mesh used during IBL generation
        void CreateCubeMesh();

        Ref<Texture> m_EnvironmentCubemap;
        Ref<Texture> m_IrradianceMap;
        Ref<Texture> m_PrefilteredMap;
        std::array<glm::vec4, 9> m_DiffuseSh{};
        Settings m_Settings;

        // Temporary — used during generation, not serialized
        Ref<VertexBuffer> m_CubeVbo;
        Ref<IndexBuffer> m_CubeIbo;
    };

} // namespace Crowny
