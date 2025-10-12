#include "cwepch.h"

#include "Editor/PreviewRenderer.h"
#include "EditorLayer.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{
    PreviewObjectRenderer::PreviewObjectRenderer(const AssetHandle<Mesh>& mesh) : m_Mesh(mesh), m_SceneRenderer(nullptr) {}

    void PreviewObjectRenderer::Setup(uint32_t width, uint32_t height) {
        static Ref<Shader> directShader = Importer::Get().Import<Shader>("Resources/Shaders/Direct.glsl");
        static const AssetHandle<Shader> directHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(directShader));
        m_Material = Material::Create(directHandle);
        static const AssetHandle<Material> materialHandle = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(m_Material));
        m_MatHandle = materialHandle;
        m_Scene = CreateRef<Scene>("Object Preview");

        MeshRendererComponent &component=m_Scene->CreateEntity("Object").AddComponent<MeshRendererComponent>();
        component.BaseMaterial = m_MatHandle;
        component.MeshHandle = m_Mesh;

        TextureParameters textureParams;
        textureParams.Width = width;
        textureParams.Height = height;
        textureParams.Format = TextureFormat::RGBA8;
        textureParams.Usage = TEXTURE_RENDERTARGET;
        textureParams.DebugName = "Preview texture";
        Ref<Texture> texture = Texture::Create(textureParams);

        RenderTextureProperties renderTextureParams;
        renderTextureParams.ColorSurfaces[0] = { texture };
        renderTextureParams.Width = width;
        renderTextureParams.Height = height;
        m_RenderTexture = RenderTexture::Create(renderTextureParams);
        
        m_SceneRenderer = new SceneRenderer(m_Scene, m_RenderTexture);
    }

    PreviewObjectRenderer::~PreviewObjectRenderer() { delete m_SceneRenderer; }

    Ref<Texture> PreviewObjectRenderer::RenderPreview() {
        EditorCamera camera;
        camera.SetViewportSize(256, 256);
        camera.SetDistance(5);
        camera.Focus(glm::vec3(0.0f));
        m_SceneRenderer->RenderEditor(camera);
        RenderAPI::Get().SubmitCommandBuffer(nullptr);
        RenderAPI::Get().SetRenderTarget(nullptr);
        return m_RenderTexture->GetColorTexture(0);

    }
}