#include "cwepch.h"

#include "Editor/PreviewRenderer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/MeshFactory.h"

namespace Crowny
{
    namespace
    {
        Ref<RenderTexture> CreatePreviewTarget(uint32_t width, uint32_t height, StringView debugName)
        {
            TextureDesc textureParams;
            textureParams.Width = width;
            textureParams.Height = height;
            textureParams.Format = TextureFormat::RGBA8;
            textureParams.Usage = TEXTURE_RENDERTARGET;
            textureParams.DebugName = String(debugName);
            Ref<Texture> texture = Texture::Create(textureParams);
            if (!texture)
                return nullptr;

            RenderTextureDesc renderTextureParams;
            renderTextureParams.ColorSurfaces[0].Texture = texture;
            renderTextureParams.Width = width;
            renderTextureParams.Height = height;
            return RenderTexture::Create(renderTextureParams);
        }
    } // namespace

    // =========================================================================
    // PreviewObjectRenderer
    // =========================================================================

    PreviewObjectRenderer::PreviewObjectRenderer(const AssetHandle<Mesh>& mesh) : m_Mesh(mesh) {}

    bool PreviewObjectRenderer::Setup(uint32_t width, uint32_t height)
    {
        Shutdown();
        AssetManager* assets = AssetManager::TryGet();
        if (width == 0 || height == 0 || !m_Mesh || assets == nullptr || RenderAPI::TryGet() == nullptr)
            return false;

        m_Width = std::min(width, MAX_PREVIEW_DIMENSION);
        m_Height = std::min(height, MAX_PREVIEW_DIMENSION);
        try
        {
            const AssetHandle<Shader> unlitShader = assets->Load<Shader>(UNLIT_SHADER_PATH, false);
            if (!unlitShader)
            {
                Shutdown();
                return false;
            }
            m_Material = Material::CreateUnlit(unlitShader);
            if (!m_Material)
            {
                Shutdown();
                return false;
            }
            m_MatHandle = static_asset_cast<Material>(assets->CreateAssetHandle(m_Material));
            if (!m_MatHandle)
            {
                Shutdown();
                return false;
            }

            m_Scene = CreateRef<Scene>("Object Preview");
            MeshRendererComponent& component = m_Scene->CreateEntity("Object").AddComponent<MeshRendererComponent>();
            component.SetMaterial(0, m_MatHandle);
            component.MeshHandle = m_Mesh;

            m_RenderTexture = CreatePreviewTarget(m_Width, m_Height, "Preview texture");
            if (!m_RenderTexture)
            {
                Shutdown();
                return false;
            }
            m_SceneRenderer = CreateScope<SceneRenderer>(m_Scene, m_RenderTexture);
            return true;
        }
        catch (...)
        {
            Shutdown();
            return false;
        }
    }

    PreviewObjectRenderer::~PreviewObjectRenderer() { Shutdown(); }

    void PreviewObjectRenderer::Shutdown()
    {
        m_SceneRenderer.reset();
        m_RenderTexture = nullptr;
        m_Scene = nullptr;
        m_MatHandle = {};
        m_Material = nullptr;
        m_Width = 0;
        m_Height = 0;
    }

    Ref<Texture> PreviewObjectRenderer::RenderPreview()
    {
        RenderAPI* renderAPI = RenderAPI::TryGet();
        if (!m_SceneRenderer || !m_RenderTexture || m_Width == 0 || m_Height == 0 || renderAPI == nullptr)
            return nullptr;

        EditorCamera camera;
        camera.SetViewportSize(m_Width, m_Height);
        camera.SetDistance(5);
        camera.Focus(glm::vec3(0.0f));
        m_SceneRenderer->RenderEditor(camera, false);
        renderAPI->SubmitCommandBuffer(nullptr);
        renderAPI->SetRenderTarget(nullptr);
        return m_RenderTexture->GetColorTexture(0);
    }

    // =========================================================================
    // PreviewMaterialRenderer
    // =========================================================================

    PreviewMaterialRenderer::PreviewMaterialRenderer(const AssetHandle<Material>& material, const AssetHandle<Mesh>& previewMesh)
      : m_Material(material), m_PreviewMesh(previewMesh)
    {
    }

    PreviewMaterialRenderer::~PreviewMaterialRenderer() { Shutdown(); }

    bool PreviewMaterialRenderer::Setup(uint32_t width, uint32_t height)
    {
        Shutdown();
        if (width == 0 || height == 0 || !m_Material || RenderAPI::TryGet() == nullptr)
            return false;

        m_Width = std::min(width, MAX_PREVIEW_DIMENSION);
        m_Height = std::min(height, MAX_PREVIEW_DIMENSION);
        try
        {
            m_RenderMesh = m_PreviewMesh;
            if (!m_RenderMesh)
            {
                AssetManager* assets = AssetManager::TryGet();
                if (assets == nullptr)
                {
                    Shutdown();
                    return false;
                }
                const Ref<Mesh> generated = MeshFactory::CreateSphere(0.5f, 32, 16);
                if (generated)
                    m_RenderMesh = static_asset_cast<Mesh>(assets->CreateAssetHandle(generated));
            }

            if (!m_RenderMesh)
            {
                Shutdown();
                return false;
            }

            m_Scene = CreateRef<Scene>("Material Preview");
            MeshRendererComponent& component = m_Scene->CreateEntity("Preview").AddComponent<MeshRendererComponent>();
            component.SetMaterial(0, m_Material);
            component.MeshHandle = m_RenderMesh;

            m_RenderTexture = CreatePreviewTarget(m_Width, m_Height, "Material preview texture");
            if (!m_RenderTexture)
            {
                Shutdown();
                return false;
            }
            m_SceneRenderer = CreateScope<SceneRenderer>(m_Scene, m_RenderTexture);
            return true;
        }
        catch (...)
        {
            Shutdown();
            return false;
        }
    }

    void PreviewMaterialRenderer::Shutdown()
    {
        m_SceneRenderer.reset();
        m_RenderTexture = nullptr;
        m_Scene = nullptr;
        m_RenderMesh = {};
        m_Width = 0;
        m_Height = 0;
    }

    Ref<Texture> PreviewMaterialRenderer::RenderPreview()
    {
        RenderAPI* renderAPI = RenderAPI::TryGet();
        if (!m_SceneRenderer || !m_RenderTexture || m_Width == 0 || m_Height == 0 || renderAPI == nullptr)
            return nullptr;

        EditorCamera camera;
        camera.SetViewportSize(m_Width, m_Height);
        camera.SetDistance(3);
        camera.Focus(glm::vec3(0.0f));
        m_SceneRenderer->RenderEditor(camera, false);
        renderAPI->SubmitCommandBuffer(nullptr);
        renderAPI->SetRenderTarget(nullptr);
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
