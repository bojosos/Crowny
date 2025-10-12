#include "cwpch.h"

#include "Crowny/ImGui/ImGuiBackend.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/Shader.h"

#include <imgui.h>

namespace Crowny
{
    static Ref<Material> mat;

    struct RenderableImGuiWindow
    {
        Ref<RenderWindow> Window;
        Ref<CommandBuffer> CmdBuffer;
        Ref<VertexBuffer> VertexBuffer;
        Ref<IndexBuffer> IndexBuffer;
        Ref<Material> ImGuiMaterial;
    };

    static void SetupRenderState(RenderableImGuiWindow* renderData, ImDrawData* drawData, int fbWidth, int fbHeigh);
    static void CreateVulkanWindow(ImGuiViewport* viewport);
    static void DrawVulkanWindow(ImGuiViewport* viewport, void* renderArg);
    static void RenderDrawData(RenderableImGuiWindow* renderData, ImDrawData* drawData, const Ref<CommandBuffer>& commandBuffer);
    static void SwapVulkanBuffers(ImGuiViewport* viewport, void* renderArg);
    static void DestroyVulkanWindow(ImGuiViewport* viewport);

    void ImGuiBackend::Init()
    {
        // TODO: Alpha blend
        Ref<Shader> textShader = Importer::Get().Import<Shader>("Resources/Shaders/ImGuiBackend.glsl");
        const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(textShader));
        mat = Material::Create(shaderHandle);

        ImGuiIO& io = ImGui::GetIO();
        IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

        io.BackendRendererUserData = this;
        io.BackendRendererName = "Crowny Vulkan";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // We can honor ImGuiPlatformIO::Textures[] requests during render.
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)

        ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        platformIo.Renderer_CreateWindow = CreateVulkanWindow;
        platformIo.Renderer_DestroyWindow = DestroyVulkanWindow;
        // platformIo.Renderer_SetWindowSize = SetVulkanWindowSize;
        platformIo.Renderer_RenderWindow = DrawVulkanWindow;
        platformIo.Renderer_SwapBuffers = SwapVulkanBuffers;
    }

    static UnorderedMap<uint32_t, Ref<Texture>> texs;
    static uint32_t idx = 1;
    static UnorderedMap<Ref<Texture>, uint32_t> texs2;

    static void CreateVulkanWindow(ImGuiViewport* viewport) {
        RenderableImGuiWindow *renderData = new RenderableImGuiWindow();
        const RenderWindowDesc& renderWindowDesc = Application::Get().GetApplicationDesc().Window;
        renderData->Window = RenderWindow::Create(renderWindowDesc);
        renderData->CmdBuffer = CommandBuffer::Create(GpuQueueType::GRAPHICS_QUEUE);
        viewport->RendererUserData = renderData;
    }

    static void DestroyVulkanWindow(ImGuiViewport* viewport) {
        RenderableImGuiWindow* renderData = static_cast<RenderableImGuiWindow*>(viewport->RendererUserData);
        delete renderData;
        viewport->RendererUserData = nullptr;
    }

    static void SwapVulkanBuffers(ImGuiViewport* viewport, void* renderArg) {
        RenderableImGuiWindow* renderData = static_cast<RenderableImGuiWindow*>(viewport->RendererUserData);
        renderData->Window->GetWindow()->OnUpdate();
        renderData->Window->SwapBuffers();
    }

    static void DrawVulkanWindow(ImGuiViewport* viewport, void* renderArg) {
        RenderableImGuiWindow* renderData = static_cast<RenderableImGuiWindow*>(viewport->RendererUserData);
        RenderDrawData(renderData, viewport->DrawData, renderData->CmdBuffer);
    }

    static void RenderDrawData(RenderableImGuiWindow* renderData, ImDrawData* drawData, const Ref<CommandBuffer>& commandBuffer)
    {
        const int fbWidth = (int)(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        const int fbHeight = (int)(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (fbWidth <= 0 || fbHeight <= 0)
            return;

        if (drawData->Textures)
        {
            for (auto* tex : *drawData->Textures)
            {
                if (tex->Status == ImTextureStatus_WantCreate)
                {
                    TextureParameters params;
                    params.Format = TextureFormat::RGBA8;
                    params.Width = tex->Width;
                    params.Height = tex->Height;
                    params.DebugName = "ImGuiFont";
                    Ref<Texture> cwTex = Texture::Create(params);
                    texs[idx] = cwTex;
                    texs2[cwTex] = idx;
                    tex->SetTexID(idx++);
                    tex->SetStatus(ImTextureStatus_OK);
                    CW_ENGINE_INFO("W: {}, H: {}, BPP: {}", tex->Width, tex->Height, tex->BytesPerPixel);
                    PixelData pd = PixelData(tex->Width, tex->Height, 1, TextureFormat::RGBA8);
                    pd.SetBuffer(tex->Pixels);
                    cwTex->WriteData(pd);
                    tex->BackendUserData = cwTex.get();
                }
                if (tex->Status == ImTextureStatus_WantUpdates)
                {
                    CW_ENGINE_INFO("W: {}, H: {}, BPP bad: {}", tex->Width, tex->Height, tex->BytesPerPixel);
                    CW_ENGINE_INFO("X: {}, Y: {}, W: {}, H: {}", tex->UpdateRect.x, tex->UpdateRect.y, tex->UpdateRect.w, tex->UpdateRect.h);
                    tex->SetStatus(ImTextureStatus_OK);
                }
                if (tex->Status == ImTextureStatus_WantDestroy)
                {
                    CW_ENGINE_INFO("Destroy");
                    texs.erase(tex->GetTexID());
                    tex->SetTexID(ImTextureID_Invalid);
                    tex->SetStatus(ImTextureStatus_Destroyed);
                    tex->BackendUserData = nullptr;
                }
            }
        }
        CW_ENGINE_ASSERT(drawData->TotalVtxCount && drawData->TotalIdxCount);
        if (drawData->TotalVtxCount > 0 &&
            (renderData->VertexBuffer == nullptr || drawData->TotalVtxCount * sizeof(ImDrawVert) >= renderData->VertexBuffer->GetBufferSize()))
        {
            renderData->VertexBuffer =
              Crowny::VertexBuffer::Create((uint32_t)drawData->TotalVtxCount * sizeof(ImDrawVert), Crowny::BufferUsage::DYNAMIC_DRAW);
            Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ BufferElement(ShaderDataType::Float2, "cw_Position"),
                                                                             BufferElement(ShaderDataType::Float2, "cw_TexCoords"),
                                                                             BufferElement(ShaderDataType::Color, "cw_Color") });
            renderData->VertexBuffer->SetLayout(layout);
        }
        if (drawData->TotalIdxCount > 0 &&
            (renderData->IndexBuffer == nullptr || drawData->TotalIdxCount >= (int)renderData->IndexBuffer->GetCount()))
            renderData->IndexBuffer = Crowny::IndexBuffer::Create((uint32_t)drawData->TotalIdxCount * 2,
                                                        sizeof(ImDrawIdx) == 2 ? Crowny::IndexType::Index_16 : Crowny::IndexType::Index_32,
                                                        Crowny::BufferUsage::DYNAMIC_DRAW);
        ImDrawVert* vtxDst = (ImDrawVert*)renderData->VertexBuffer->Map(0, renderData->VertexBuffer->GetBufferSize(), GpuLockOptions::WRITE_DISCARD);
        for (uint32_t n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* drawList = drawData->CmdLists[n];
            std::memcpy(vtxDst, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
            vtxDst += drawList->VtxBuffer.Size;
        }
        renderData->VertexBuffer->Unmap();

        ImDrawIdx* idxDst = (ImDrawIdx*)renderData->IndexBuffer->Map(0, renderData->IndexBuffer->GetBufferSize(), GpuLockOptions::WRITE_DISCARD);
        for (uint32_t n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* drawList = drawData->CmdLists[n];
            std::memcpy(idxDst, drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));
            idxDst += drawList->IdxBuffer.Size;
        }
        renderData->IndexBuffer->Unmap();

        SetupRenderState(renderData, drawData, fbWidth, fbHeight);

        ImVec2 clipOff = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        int globalVtxOffset = 0;
        int globalIdxOffset = 0;
        for (int n = 0; n < drawData->CmdListsCount; n++)
        {
            const ImDrawList* drawList = drawData->CmdLists[n];
            for (int i = 0; i < drawList->CmdBuffer.Size; i++)
            {
                const ImDrawCmd* pcmd = &drawList->CmdBuffer[i];
                if (pcmd->UserCallback != nullptr)
                {
                    if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                        SetupRenderState(renderData, drawData, fbWidth, fbHeight);
                    else
                        pcmd->UserCallback(drawList, pcmd);
                }
                else
                {
                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec2 clipMin((pcmd->ClipRect.x - clipOff.x) * clipScale.x, (pcmd->ClipRect.y - clipOff.y) * clipScale.y);
                    ImVec2 clipMax((pcmd->ClipRect.z - clipOff.x) * clipScale.x, (pcmd->ClipRect.w - clipOff.y) * clipScale.y);

                    if (clipMin.x < 0.0f)
                        clipMin.x = 0.0f;
                    if (clipMin.y < 0.0f)
                        clipMin.y = 0.0f;
                    if (clipMax.x > fbWidth)
                        clipMax.x = (float)fbWidth;
                    if (clipMax.y > fbHeight)
                        clipMax.y = (float)fbHeight;
                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    // Apply scissor/clipping rectangle
                    Rect2I scissor;
                    scissor.X = (int32_t)(clipMin.x);
                    scissor.Y = (int32_t)(clipMin.y);
                    scissor.Width = (uint32_t)(clipMax.x - clipMin.x);
                    scissor.Height = (uint32_t)(clipMax.y - clipMin.y);
                    RenderAPI::Get().SetScissorRect(scissor, commandBuffer);

                    // Draw
                    mat->SetTexture("sTexture", texs[pcmd->GetTexID()]);
                    RenderAPI::Get().SetUniforms(mat->GetUniformParams(), commandBuffer);
                    RenderAPI::Get().DrawIndexed(pcmd->IdxOffset + globalIdxOffset, pcmd->ElemCount, pcmd->VtxOffset + globalVtxOffset,
                                                 pcmd->ElemCount, 1, commandBuffer);
                }
            }
            globalVtxOffset += drawList->VtxBuffer.Size;
            globalIdxOffset += drawList->IdxBuffer.Size;
        }
        Rect2I scissor = { 0, 0, (int32_t)fbWidth, (int32_t)fbHeight };
        RenderAPI::Get().SetScissorRect(scissor, commandBuffer);
    }

    static void SetupRenderState(RenderableImGuiWindow* renderData, ImDrawData* drawData, int fbWidth, int fbHeight)
    {
        RenderAPI::Get().SetRenderTarget(Application::Get().GetRenderWindow());
        RenderAPI::Get().ClearViewport(FBT_COLOR | FBT_DEPTH);
        RenderAPI::Get().SetVertexLayout(renderData->VertexBuffer->GetLayout(), renderData->CmdBuffer);
        RenderAPI::Get().SetGraphicsPipeline(mat->GetGraphicsPipeline(), renderData->CmdBuffer);
        RenderAPI::Get().SetVertexBuffers(0, &renderData->VertexBuffer, 1, renderData->CmdBuffer);
        RenderAPI::Get().SetIndexBuffer(renderData->IndexBuffer, renderData->CmdBuffer);

        RenderAPI::Get().SetViewport(0, 0, 1, 1, renderData->CmdBuffer);

        const glm::vec2 scale = { 2.0f / drawData->DisplaySize.x, 2.0f / drawData->DisplaySize.y };
        const glm::vec2 translate = { -1.0f - drawData->DisplayPos.x * scale[0], -1.0f - drawData->DisplayPos.y * scale[1] };
        mat->SetFloat2("uScale", scale);
        mat->SetFloat2("uTranslate", translate);
    }

    ImTextureRef ImGuiBackend::RegisterTexture(const Ref<Texture>& texture)
    {
        if (texs2.count(texture))
            return ImTextureRef(texs2[texture]);
        texs[idx] = texture;
        texs2[texture] = idx;
        return ImTextureRef(idx++);
    }

    void ImGuiBackend::RenderVulkanWindow(ImGuiViewport* viewport, void*) {

    }

} // namespace Crowny