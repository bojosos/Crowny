#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Scene/SceneRenderer.h"

namespace Crowny
{

    class PreviewRenderer
    {
    public:
        virtual ~PreviewRenderer() = default;
        virtual Ref<Texture> RenderPreview() = 0;
        virtual bool Setup(uint32_t width, uint32_t height) = 0;
        virtual void Shutdown() = 0;
        virtual bool IsAssetTypeSupported(AssetType assetType) { return false; }

    protected:
        static constexpr uint32_t MAX_PREVIEW_DIMENSION = 256;
    };

    class PreviewObjectRenderer : public PreviewRenderer
    {
    public:
        PreviewObjectRenderer(const AssetHandle<Mesh>& mesh);
        ~PreviewObjectRenderer() override;

        bool Setup(uint32_t width, uint32_t height) override;
        void Shutdown() override;
        Ref<Texture> RenderPreview() override;

    private:
        AssetHandle<Mesh> m_Mesh;
        Ref<Scene> m_Scene;
        Ref<Material> m_Material;
        AssetHandle<Material> m_MatHandle;
        Ref<RenderTexture> m_RenderTexture;
        Scope<SceneRenderer> m_SceneRenderer;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    class PreviewMaterialRenderer : public PreviewRenderer
    {
    public:
        PreviewMaterialRenderer(const AssetHandle<Material>& material, const AssetHandle<Mesh>& previewMesh = {});
        ~PreviewMaterialRenderer() override;

        bool Setup(uint32_t width, uint32_t height) override;
        void Shutdown() override;
        Ref<Texture> RenderPreview() override;

    private:
        AssetHandle<Material> m_Material;
        AssetHandle<Mesh> m_PreviewMesh;
        AssetHandle<Mesh> m_RenderMesh;
        Ref<Scene> m_Scene;
        Ref<RenderTexture> m_RenderTexture;
        Scope<SceneRenderer> m_SceneRenderer;
        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
    };

    class PreviewTextureRenderer
    {
    public:
        // Returns a downscaled copy suitable for thumbnails
        static Ref<Texture> CreateThumbnail(const Ref<Texture>& source, uint32_t maxSize = 128);
    };

} // namespace Crowny
