#include "cwpch.h"

#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Renderer/Skybox.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Timer.h"
#include "Crowny/Common/VirtualFileSystem.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <glad/glad.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

namespace Crowny
{

    float skyboxVertices[] = { -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
                               -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,
                               1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,
                               -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                               -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f };

    uint32_t skyboxIndices[] = {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    Skybox::Skybox(const Path& filepath)
    {
        // Load equirectangular map
        // TODO: CW API
        stbi_set_flip_vertically_on_load(true);
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        std::vector<uint8_t> buf;
        buf.resize(stream->Size());
        stream->Read(buf.data(), stream->Size());
        stream->Close();

        int width, height, channels;
        float* data = stbi_loadf_from_memory(buf.data(), (int)buf.size(), &width, &height, &channels, 0);
        Ref<Texture> equirectangularTexture;
        if (data != nullptr)
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

        const glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        const glm::mat4 captureViews[] = { glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                                           glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                                           glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) };
        RenderAPI& rapi = RenderAPI::Get();
        TextureParameters tProps;
        tProps.Width = 1024;
        tProps.Height = 1024;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.Format = TextureFormat::RGBA32F;

        m_EnvironmentMap = Texture::Create(tProps);

        // Convert HDR equirectangular environment map to cubemap

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(EQUIRECTTOCUBE_SHADER_PATH);
        Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/EquirectToCube.glsl");
        const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        Ref<Material> equirectMaterial = Material::Create(shaderHandle);
        Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } });
        m_SkyboxVbo = VertexBuffer::Create(skyboxVertices, 72 * sizeof(float));
        m_SkyboxIbo = IndexBuffer::Create(skyboxIndices, 36);
        m_SkyboxVbo->SetLayout(layout);
        equirectMaterial->SetTexture("equirectangularMap", equirectangularTexture);
        equirectMaterial->SetMatrix("proj", captureProjection);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.SetGraphicsPipeline(equirectMaterial->GetGraphicsPipeline());

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

            equirectMaterial->SetMatrix("view", captureViews[i]);
            rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
            rapi.SetVertexLayout(layout);
            rapi.SetIndexBuffer(m_SkyboxIbo);
            rapi.SetUniforms(equirectMaterial->GetUniformParams());
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

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(BRDF_SHADER_PATH);
        Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Brdf.glsl");
        const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        Ref<Material> brdfLUTMaterial = Material::Create(shaderHandle);
        rapi.SetRenderTarget(target);
        rapi.SetGraphicsPipeline(brdfLUTMaterial->GetGraphicsPipeline());
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        // TODO: Samples uniform control, currently it's push constant in the shader.
        rapi.SetUniforms(brdfLUTMaterial->GetUniformParams());
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

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(FILTER_SHADER_PATH);
        Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Prefilter.glsl");
        const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        Ref<Material> filterMaterial = Material::Create(shaderHandle);

        Vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)),                                        // NEGATIVE_X
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),  // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };
        filterMaterial->SetTexture("samplerEnv", m_EnvironmentMap);
        for (uint32_t j = 0; j < 6; j++)
        {
            const glm::mat4 viewProjection = glm::perspective((float)(M_PI * 0.5f), 1.0f, 0.1f, 64.0f) * matrices[j];
            filterMaterial->SetMatrix("mvp", viewProjection);
            for (uint32_t i = 0; i < numMips; i++)
            {
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = m_IrradianceMap;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / (int)std::pow(2, i);
                rtProps.Height = tProps.Height / (int)std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(filterMaterial->GetGraphicsPipeline());
                // rapi.SetViewport(0.0f, 0.0f, (float)std::pow(0.5f, i), (float)std::pow(0.5f, i));
                rapi.SetVertexLayout(m_SkyboxVbo->GetLayout());
                rapi.SetUniforms(filterMaterial->GetUniformParams());
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
            float roughness = 0.1f;
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

        // AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>(FILTER_SHADER_PATH);
        Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Filter.glsl");
        const AssetHandle<Shader> shaderHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        Ref<Material> prefilterMaterial = Material::Create(shaderHandle);

        Vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                        glm::vec3(1.0f, 0.0f, 0.0f)),                                        // NEGATIVE_X
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),  // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
        };

        float roughness = 0.1f;
        prefilterMaterial->SetInt("samples", 32);
        prefilterMaterial->SetTexture("samplerEnv", m_EnvironmentMap);
        // TODO: Swap these fors around
        for (uint32_t j = 0; j < 6; j++)
        {
            const glm::mat4 viewProjection = glm::perspective((float)(M_PI * 0.5f), 1.0f, 0.1f, 512.0f) * matrices[j];
            prefilterMaterial->SetMatrix("mvp", viewProjection);
            for (uint32_t i = 0; i < numMips; i++)
            {
                roughness = (float)i / (float)(numMips);
                prefilterMaterial->SetFloat("roughness", roughness);
                RenderTextureProperties rtProps;
                rtProps.ColorSurfaces[0].Texture = m_PrefilteredMap;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = tProps.Width / (int)std::pow(2, i);
                rtProps.Height = tProps.Height / (int)std::pow(2, i);
                Ref<RenderTexture> cubemap = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(cubemap);
                rapi.SetGraphicsPipeline(prefilterMaterial->GetGraphicsPipeline());
                // rapi.SetViewport(0.0f, 0.0f, (float)std::pow(0.5f, i), (float)std::pow(0.5f, i));
                rapi.SetUniforms(prefilterMaterial->GetUniformParams());
                rapi.SetVertexBuffers(0, &m_SkyboxVbo, 1);
                rapi.SetIndexBuffer(m_SkyboxIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
    }

} // namespace Crowny
