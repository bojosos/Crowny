#include "cwpch.h"

#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Renderer/Skybox.h"

#include "Crowny/Common/Timer.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace Crowny
{

    float vertices[] = { -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
                         -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,
                         -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f,
                         -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
                         1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                         -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f };

    uint32_t indices[] = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    Skybox::Skybox(const Path& filepath)
    {
        // Load equirectangular map
        stbi_set_flip_vertically_on_load(true);
        auto [dat, size] = VirtualFileSystem::Get()->ReadFile(filepath);
        int width, height, channels;
        float* data = stbi_loadf_from_memory(dat, size, &width, &height, &channels, 0);
        Ref<Texture> equirectangularTexture;
        if (data)
        {
            m_Width = width;
            m_Height = height;
            m_Channels = channels;
            TextureParameters tProps;
            tProps.Width = m_Width;
            tProps.Height = m_Height;
            tProps.Usage = TextureUsage::TEXTURE_STATIC;
            tProps.Format = TextureFormat::RGBA32F;
            equirectangularTexture = Texture::Create(tProps);
            PixelData dat(m_Width, m_Height, 1, TextureFormat::RGB32F);
            dat.SetBuffer((uint8_t*)data);
            equirectangularTexture->WriteData(dat);
            dat.SetBuffer(nullptr);
            stbi_image_free(data);
        }
        else
            CW_ENGINE_ERROR("Failed to load HDR image.");
            
        delete dat;

        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] = {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
        };
        auto& rapi = RenderAPI::Get();
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.Format = TextureFormat::RGBA32F;

        m_EnvironmentMap = Texture::Create(tProps);

        // Convert HDR equirectangular environment map to cubemap
        Ref<Shader> shader = Importer::Get().Import<Shader>(EQUIRECTTOCUBE_SHADER_PATH);
        Ref<ShaderStage> eqToCubeVert = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> eqToCubeFrag = shader->GetStage(FRAGMENT_SHADER);
        PipelineStateDesc desc;
        desc.VertexShader = eqToCubeVert;
        desc.FragmentShader = eqToCubeFrag;
        BufferLayout layout = { { ShaderDataType::Float3, "a_Position" } };
        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, layout);

        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);
        Ref<UniformBufferBlock> block = UniformBufferBlock::Create(
          eqToCubeVert->GetUniformDesc()->Uniforms.at("VP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "VP", block);
        uniforms->SetTexture(0, 1, equirectangularTexture);
        block->Write(0, &captureProjection, sizeof(glm::mat4));

        m_SkyboxVbo = VertexBuffer::Create(vertices, 72 * sizeof(float));
        m_SkyboxIbo = IndexBuffer::Create(indices, 36);
        m_SkyboxVbo->SetLayout(layout);

        rapi.SetViewport(0, 0, 512, 512);
        rapi.SetGraphicsPipeline(pipeline);

        for (uint32_t i = 0; i < 6; i++)
        {
            RenderTextureProperties rtProps;
            rtProps.ColorSurfaces[0].Texture = m_EnvironmentMap;
            rtProps.ColorSurfaces[0].Face = i;
            rtProps.ColorSurfaces[0].NumFaces = 1;
            rtProps.ColorSurfaces[0].MipLevel = 0;
            rtProps.Width = tProps.Width;
            rtProps.Height = tProps.Height;
            Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
            rapi.SetRenderTarget(cubemap);

            block->Write(sizeof(glm::mat4), &captureViews[i], sizeof(glm::mat4));
            rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
            rapi.SetIndexBuffer(m_SkyboxIbo);
            rapi.SetUniforms(uniforms);
            rapi.DrawIndexed(0, 36, 0, 72);
        }

        GenerateBRDFLUT();
        GeneratePrefilteredCube();
        GenerateIrradianceCube();
    }

    void Skybox::GenerateBRDFLUT()
    {
        Timer t;
        auto& rapi = RenderAPI::Get();
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Format = TextureFormat::RG32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        m_Brdf = Texture::Create(tProps);

        RenderTextureProperties rtProps;
        rtProps.Width = tProps.Width;
        rtProps.Height = tProps.Height;
        rtProps.ColorSurfaces[0] = { m_Brdf };
        Ref<RenderTexture> target = RenderTexture::Create(rtProps);

        Ref<Shader> shader = Importer::Get().Import<Shader>(BRDF_SHADER_PATH);
        Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, {});
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        rapi.SetRenderTarget(target);
        rapi.SetGraphicsPipeline(pipeline);
        rapi.SetViewport(0, 0, 512, 512);
        rapi.SetUniforms(uniforms);
        rapi.Draw(0, 3, 1);
    }

    void Skybox::GenerateIrradianceCube()
    {
        Timer timer;
        auto& rapi = RenderAPI::Get();
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(64)));
        TextureParameters tProps;
        tProps.Width = 64;
        tProps.Height = 64;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        m_IrradianceMap = Texture::Create(tProps);

        Ref<Shader> shader = Importer::Get().Import<Shader>(FILTER_SHADER_PATH);
        Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, m_SkyboxVbo->GetLayout());
        Rect2F viewport = { 0.0f, 0.0f, 64.0f, 64.0f };
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        Vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),                  // NEGATIVE_X
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),  // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };

        Ref<UniformBufferBlock> mvp =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms = UniformParams::Create(pipeline);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", mvp);
        uniforms->SetTexture(0, 1, m_EnvironmentMap);
        for (uint32_t j = 0; j < 6; j++)
        {
            auto persp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 64.0f) * matrices[j];
            mvp->Write(0, &persp, sizeof(glm::mat4));
            for (uint32_t i = 0; i < numMips; i++)
            {
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = m_IrradianceMap;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / std::pow(2, i);
                rtProps.Height = tProps.Height / std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(pipeline);
                viewport.Width = static_cast<float>(64 * std::pow(0.5f, i));
                viewport.Height = static_cast<float>(64 * std::pow(0.5f, i));
                rapi.SetViewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
                rapi.SetUniforms(uniforms);
                rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
                rapi.SetIndexBuffer(m_SkyboxIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
    }

    void Skybox::GeneratePrefilteredCube()
    {
        Timer timer;

        struct PrefilterParams
        {
            uint32_t samples = 32;
            float roughness;
        } params;

        auto& rapi = RenderAPI::Get();
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(64)));
        TextureParameters tProps;
        tProps.Width = 512;
        tProps.Height = 512;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        m_PrefilteredMap = Texture::Create(tProps);

        Ref<Shader> shader = Importer::Get().Import<Shader>(FILTER_SHADER_PATH);
        Ref<ShaderStage> vertex = shader->GetStage(VERTEX_SHADER);
        Ref<ShaderStage> fragment = shader->GetStage(FRAGMENT_SHADER);
        PipelineStateDesc desc;
        desc.FragmentShader = fragment;
        desc.VertexShader = vertex;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create(desc, m_SkyboxVbo->GetLayout());
        Rect2F viewport = { 0.0f, 0.0f, 512.0f, 512.0f };
        Ref<UniformParams> uniforms = UniformParams::Create(pipeline);

        Vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),                  // NEGATIVE_X
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),  // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };

        Ref<UniformBufferBlock> mvp =
          UniformBufferBlock::Create(vertex->GetUniformDesc()->Uniforms.at("MVP").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms = UniformParams::Create(pipeline);
        uniforms->SetUniformBlockBuffer(ShaderType::VERTEX_SHADER, "MVP", mvp);
        uniforms->SetTexture(0, 1, m_EnvironmentMap);
        Ref<UniformBufferBlock> fragBlock = UniformBufferBlock::Create(
          fragment->GetUniformDesc()->Uniforms.at("Params").BlockSize, BufferUsage::DYNAMIC_DRAW);
        uniforms->SetUniformBlockBuffer(ShaderType::FRAGMENT_SHADER, "Params", fragBlock);
        for (uint32_t j = 0; j < 6; j++)
        {
            auto persp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[j];
            mvp->Write(0, &persp, sizeof(glm::mat4));
            for (uint32_t i = 0; i < numMips; i++)
            {
                params.roughness = (float)i / (float)(numMips);
                fragBlock->Write(0, &params, sizeof(PrefilterParams));
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = m_PrefilteredMap;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / std::pow(2, i);
                rtProps.Height = tProps.Height / std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(pipeline);
                viewport.Width = static_cast<float>(512 * std::pow(0.5f, i));
                viewport.Height = static_cast<float>(512 * std::pow(0.5f, i));
                rapi.SetViewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
                rapi.SetUniforms(uniforms);
                rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
                rapi.SetIndexBuffer(m_SkyboxIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
    }

} // namespace Crowny
