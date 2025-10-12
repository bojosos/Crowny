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
        Ref<Material> m_Material; // TODO: Cache?
        AssetHandle<Material> m_MatHandle;
        Ref<RenderTexture> m_RenderTexture;
        SceneRenderer* m_SceneRenderer;
	};

	class PreviewMaterialRenderer
	{

	};

	class PreviewTextureRenderer
	{

	};

	class PreviewRendererQueue
	{

	};

}