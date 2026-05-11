#include "cwepch.h"

#include "Editor/PreviewRenderer.h"
#include "EditorLayer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Shader.h"

namespace Crowny
{
    // =========================================================================
    // PreviewObjectRenderer
    // =========================================================================

    PreviewObjectRenderer::PreviewObjectRenderer(const AssetHandle<Mesh>& mesh) : m_Mesh(mesh), m_SceneRenderer(nullptr) {}

    void PreviewObjectRenderer::Setup(uint32_t width, uint32_t height)
    {
        static Ref<Shader> directShader = Importer::Get().Import<Shader>("Resources/Shaders/Direct.glsl");
        static const AssetHandle<Shader> directHandle = static_asset_cast<Shader>(gAssetManager->CreateAssetHandle(directShader));
        m_Material = Material::Create(directHandle);
        static const AssetHandle<Material> materialHandle = static_asset_cast<Material>(gAssetManager->CreateAssetHandle(m_Material));
        m_MatHandle = materialHandle;
        m_Scene = CreateRef<Scene>("Object Preview");

        MeshRendererComponent& component = m_Scene->CreateEntity("Object").AddComponent<MeshRendererComponent>();
        component.SetMaterial(0, m_MatHandle);
        component.MeshHandle = m_Mesh;

        TextureDesc textureParams;
        textureParams.Width = width;
        textureParams.Height = height;
        textureParams.Format = TextureFormat::RGBA8;
        textureParams.Usage = TEXTURE_RENDERTARGET;
        textureParams.DebugName = "Preview texture";
        Ref<Texture> texture = Texture::Create(textureParams);

        RenderTextureDesc renderTextureParams;
        renderTextureParams.ColorSurfaces[0].Texture = texture;
        renderTextureParams.Width = width;
        renderTextureParams.Height = height;
        m_RenderTexture = RenderTexture::Create(renderTextureParams);

        m_SceneRenderer = new SceneRenderer(m_Scene, m_RenderTexture);
    }

    PreviewObjectRenderer::~PreviewObjectRenderer() { delete m_SceneRenderer; }

    Ref<Texture> PreviewObjectRenderer::RenderPreview()
    {
        EditorCamera camera;
        camera.SetViewportSize(256, 256);
        camera.SetDistance(5);
        camera.Focus(glm::vec3(0.0f));
        m_SceneRenderer->RenderEditor(camera);
        gRenderAPI->SubmitCommandBuffer(nullptr);
        gRenderAPI->SetRenderTarget(nullptr);
        return m_RenderTexture->GetColorTexture(0);
    }

    // =========================================================================
    // PreviewMaterialRenderer
    // =========================================================================

    PreviewMaterialRenderer::PreviewMaterialRenderer(const AssetHandle<Material>& material, const AssetHandle<Mesh>& previewMesh)
      : m_Material(material), m_PreviewMesh(previewMesh), m_SceneRenderer(nullptr)
    {
    }

    PreviewMaterialRenderer::~PreviewMaterialRenderer() { delete m_SceneRenderer; }

    void PreviewMaterialRenderer::Setup(uint32_t width, uint32_t height)
    {
        m_Scene = CreateRef<Scene>("Material Preview");

        // If no preview mesh provided, import a sphere (fallback to a simple cube-like mesh)
        if (!m_PreviewMesh)
        {
            // Use a built-in sphere mesh for material previews if available
            static AssetHandle<Mesh> sphereMesh;
            if (!sphereMesh)
            {
                Ref<Mesh> imported = Importer::Get().Import<Mesh>("Resources/Meshes/Sphere.fbx");
                if (imported)
                    sphereMesh = static_asset_cast<Mesh>(gAssetManager->CreateAssetHandle(imported));
            }
            m_PreviewMesh = sphereMesh;
        }

        if (m_PreviewMesh)
        {
            MeshRendererComponent& component = m_Scene->CreateEntity("Preview").AddComponent<MeshRendererComponent>();
            component.SetMaterial(0, m_Material);
            component.MeshHandle = m_PreviewMesh;
        }

        TextureDesc textureParams;
        textureParams.Width = width;
        textureParams.Height = height;
        textureParams.Format = TextureFormat::RGBA8;
        textureParams.Usage = TEXTURE_RENDERTARGET;
        textureParams.DebugName = "Material preview texture";
        Ref<Texture> texture = Texture::Create(textureParams);

        RenderTextureDesc renderTextureParams;
        renderTextureParams.ColorSurfaces[0].Texture = texture;
        renderTextureParams.Width = width;
        renderTextureParams.Height = height;
        m_RenderTexture = RenderTexture::Create(renderTextureParams);

        m_SceneRenderer = new SceneRenderer(m_Scene, m_RenderTexture);
    }

    Ref<Texture> PreviewMaterialRenderer::RenderPreview()
    {
        EditorCamera camera;
        camera.SetViewportSize(256, 256);
        camera.SetDistance(3);
        camera.Focus(glm::vec3(0.0f));
        m_SceneRenderer->RenderEditor(camera);
        gRenderAPI->SubmitCommandBuffer(nullptr);
        gRenderAPI->SetRenderTarget(nullptr);
        return m_RenderTexture->GetColorTexture(0);
    }

    // =========================================================================
    // PreviewTextureRenderer
    // =========================================================================

    Ref<Texture> PreviewTextureRenderer::CreateThumbnail(const Ref<Texture>& source, uint32_t maxSize)
    {
        if (!source)
            return nullptr;

        // For textures that are already small enough, return as-is
        const uint32_t srcW = source->GetWidth();
        const uint32_t srcH = source->GetHeight();
        if (srcW <= maxSize && srcH <= maxSize)
            return source;

        // Calculate downscaled dimensions maintaining aspect ratio
        const float aspect = (float)srcW / (float)srcH;
        uint32_t dstW, dstH;
        if (srcW >= srcH)
        {
            dstW = maxSize;
            dstH = (uint32_t)(maxSize / aspect);
        }
        else
        {
            dstH = maxSize;
            dstW = (uint32_t)(maxSize * aspect);
        }
        if (dstW == 0)
            dstW = 1;
        if (dstH == 0)
            dstH = 1;

        // Create a smaller texture — for now just return the source since
        // GPU-side downscale requires a blit pass. The ImGui texture display
        // will visually scale it down. This avoids loading full-res into the icon cache.
        // TODO: Implement GPU blit downscale for memory savings on large textures
        return source;
    }

} // namespace Crowny
