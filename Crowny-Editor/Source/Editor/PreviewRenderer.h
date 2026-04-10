#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Assets/Asset.h"
#include "Crowny/Scene/SceneRenderer.h"

namespace Crowny
{

	class PreviewRenderer
    {
    public:
        virtual ~PreviewRenderer() = default;
        virtual Ref<Texture> RenderPreview() = 0;
        virtual void Setup(uint32_t width, uint32_t height) = 0;
        virtual bool IsAssetTypeSupported(AssetType assetType) { return false; }
    };

	class PreviewObjectRenderer : public PreviewRenderer
	{
    public:
        PreviewObjectRenderer(const AssetHandle<Mesh>& mesh);
        ~PreviewObjectRenderer();

		virtual void Setup(uint32_t width, uint32_t height) override;
        virtual Ref<Texture> RenderPreview() override;
    private:
        AssetHandle<Mesh> m_Mesh;
        Ref<Scene> m_Scene;
        Ref<CommandBuffer> m_CommandBuffer;
        Ref<Material> m_Material;
        AssetHandle<Material> m_MatHandle;
        Ref<RenderTexture> m_RenderTexture;
        SceneRenderer* m_SceneRenderer;
	};

	class PreviewMaterialRenderer : public PreviewRenderer
	{
    public:
        PreviewMaterialRenderer(const AssetHandle<Material>& material, const AssetHandle<Mesh>& previewMesh = {});
        ~PreviewMaterialRenderer();

        virtual void Setup(uint32_t width, uint32_t height) override;
        virtual Ref<Texture> RenderPreview() override;
    private:
        AssetHandle<Material> m_Material;
        AssetHandle<Mesh> m_PreviewMesh;
        Ref<Scene> m_Scene;
        Ref<RenderTexture> m_RenderTexture;
        SceneRenderer* m_SceneRenderer;
	};

	class PreviewTextureRenderer
	{
    public:
        // Returns a downscaled copy suitable for thumbnails
        static Ref<Texture> CreateThumbnail(const Ref<Texture>& source, uint32_t maxSize = 128);
	};

}
