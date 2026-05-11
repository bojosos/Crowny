#include "cwpch.h"

#include "Crowny/Renderer/EnvironmentMap.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Timer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Material.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

namespace Crowny
{

    static const float s_CubeVertices[] = { -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
                                            -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,
                                            1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,
                                            -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,
                                            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f };

    static const uint32_t s_CubeIndices[] = {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    static const Vector<glm::mat4> s_CubeFaceMatrices = {
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_X
        glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f),
                    glm::vec3(1.0f, 0.0f, 0.0f)),                                        // NEGATIVE_X
        glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),  // NEGATIVE_Y
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // POSITIVE_Z
        glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // NEGATIVE_Z
    };

    EnvironmentMap::EnvironmentMap(const Path& hdrPath, const Settings& settings) : m_Settings(settings)
    {
        CreateCubeMesh();
        GenerateFromHDR(hdrPath);
        GeneratePrefilteredCube();
        GenerateIrradianceCube();

        // Release temporary generation resources
        m_CubeVbo = nullptr;
        m_CubeIbo = nullptr;
    }

    void EnvironmentMap::CreateCubeMesh()
    {
        const Ref<BufferLayout> layout = CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } });
        m_CubeVbo = VertexBuffer::Create({sizeof(s_CubeVertices), BufferUsage::BU_STATIC_DRAW, s_CubeVertices});
        m_CubeIbo = IndexBuffer::Create({36, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, s_CubeIndices});
        m_CubeVbo->SetLayout(layout);
    }

    void EnvironmentMap::GenerateFromHDR(const Path& hdrPath)
    {
        Timer timer;

        // Load HDR equirectangular image
        stbi_set_flip_vertically_on_load(true);
        Ref<DataStream> stream = FileSystem::OpenFile(hdrPath);
        if (!stream)
        {
            CW_ENGINE_ERROR("Failed to open HDR file: {}", hdrPath.string());
            return;
        }
        std::vector<uint8_t> buf;
        buf.resize(stream->Size());
        stream->Read(buf.data(), stream->Size());
        stream->Close();

        int width, height, channels;
        // Force 4-channel (RGBA) output so the PixelData format matches the RGBA32F texture.
        // stbi will pad RGB HDR images to RGBA, setting the alpha channel to 1.0.
        float* data = stbi_loadf_from_memory(buf.data(), (int)buf.size(), &width, &height, &channels, STBI_rgb_alpha);
        if (!data)
        {
            CW_ENGINE_ERROR("Failed to load HDR image: {}", hdrPath.string());
            return;
        }

        TextureDesc equirectProps;
        equirectProps.Width = width;
        equirectProps.Height = height;
        equirectProps.Usage = TextureUsage::TEXTURE_STATIC;
        equirectProps.Format = TextureFormat::RGBA32F;
        equirectProps.DebugName = "EnvMap/Equirect";
        const Ref<Texture> equirectTexture = Texture::Create(equirectProps);
        PixelData pixelData(width, height, 1, TextureFormat::RGBA32F);
        pixelData.SetBuffer((uint8_t*)data);
        equirectTexture->WriteData(pixelData);
        pixelData.SetBuffer(nullptr);
        stbi_image_free(data);

        // Create environment cubemap
        RenderAPI& rapi = (*gRenderAPI);
        TextureDesc cubeProps;
        cubeProps.Width = m_Settings.CubemapResolution;
        cubeProps.Height = m_Settings.CubemapResolution;
        cubeProps.Faces = 6;
        cubeProps.Shape = TextureShape::TEXTURE_CUBE;
        cubeProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        cubeProps.Format = TextureFormat::RGBA32F;
        cubeProps.DebugName = "EnvMap/Cubemap";
        m_EnvironmentCubemap = Texture::Create(cubeProps);

        // Convert equirectangular to cubemap
        const glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        const glm::mat4 captureViews[] = { glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                                           glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                                           glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                                           glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) };

        const AssetHandle<Shader> shaderHandle = gAssetManager->Load<Shader>(EQUIRECTTOCUBE_SHADER_PATH);
        const Ref<Material> equirectMaterial = Material::Create(shaderHandle);
        equirectMaterial->SetTexture("cw_equirectangularMap", equirectTexture);
        equirectMaterial->SetMatrix("proj", captureProjection);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.SetGraphicsPipeline(equirectMaterial->GetGraphicsPipeline());

        for (uint32_t i = 0; i < 6; i++)
        {
            RenderTextureDesc rtProps;
            rtProps.ColorSurfaces[0].Texture = m_EnvironmentCubemap;
            rtProps.ColorSurfaces[0].Face = i;
            rtProps.ColorSurfaces[0].NumFaces = 1;
            rtProps.ColorSurfaces[0].MipLevel = 0;
            rtProps.Width = cubeProps.Width;
            rtProps.Height = cubeProps.Height;
            const Ref<RenderTexture> target = RenderTexture::Create(rtProps);
            rapi.SetRenderTarget(target);

            equirectMaterial->SetMatrix("view", captureViews[i]);
            rapi.SetVertexBuffers(0, &m_CubeVbo, 1);
            rapi.SetVertexLayout(m_CubeVbo->GetLayout());
            rapi.SetIndexBuffer(m_CubeIbo);
            rapi.SetUniforms(equirectMaterial->GetUniformParams());
            rapi.DrawIndexed(0, 36, 0, 72);
        }
    }

    void EnvironmentMap::GenerateIrradianceCube()
    {
        Timer timer;
        auto& rapi = (*gRenderAPI);
        TextureDesc tProps;
        tProps.Width = m_Settings.IrradianceResolution;
        tProps.Height = m_Settings.IrradianceResolution;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = 0;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.DebugName = "EnvMap/Irradiance";
        m_IrradianceMap = Texture::Create(tProps);

        const AssetHandle<Shader> shaderHandle = gAssetManager->Load<Shader>(PREFILTER_SHADER_PATH);
        const Ref<Material> irradianceMaterial = Material::Create(shaderHandle);

        irradianceMaterial->SetTexture("cw_samplerEnv", m_EnvironmentCubemap);
        for (uint32_t j = 0; j < 6; j++)
        {
            const float farPlane = (float)m_Settings.IrradianceResolution;
            const glm::mat4 viewProjection = glm::perspective((float)(M_PI * 0.5f), 1.0f, 0.1f, farPlane) * s_CubeFaceMatrices[j];
            irradianceMaterial->SetMatrix("mvp", viewProjection);

            RenderTextureDesc rtProps;
            rtProps.ColorSurfaces[0].Texture = m_IrradianceMap;
            rtProps.ColorSurfaces[0].Face = j;
            rtProps.ColorSurfaces[0].NumFaces = 1;
            rtProps.ColorSurfaces[0].MipLevel = 0;
            rtProps.Width = tProps.Width;
            rtProps.Height = tProps.Height;
            const Ref<RenderTexture> target = RenderTexture::Create(rtProps);
            rapi.SetRenderTarget(target);
            rapi.SetGraphicsPipeline(irradianceMaterial->GetGraphicsPipeline());
            rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            rapi.SetVertexLayout(m_CubeVbo->GetLayout());
            rapi.SetUniforms(irradianceMaterial->GetUniformParams());
            rapi.SetVertexBuffers(0, &m_CubeVbo, 1);
            rapi.SetIndexBuffer(m_CubeIbo);
            rapi.DrawIndexed(0, 36, 0, 72);
        }
    }

    void EnvironmentMap::GeneratePrefilteredCube()
    {
        Timer timer;
        auto& rapi = (*gRenderAPI);
        const uint32_t res = m_Settings.PrefilteredResolution;
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(res)));
        TextureDesc tProps;
        tProps.Width = res;
        tProps.Height = res;
        tProps.Format = TextureFormat::RGBA32F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.DebugName = "EnvMap/Prefiltered";
        m_PrefilteredMap = Texture::Create(tProps);

        const AssetHandle<Shader> shaderHandle = gAssetManager->Load<Shader>(FILTER_SHADER_PATH);
        const Ref<Material> prefilterMaterial = Material::Create(shaderHandle);

        prefilterMaterial->SetInt("samples", (int)m_Settings.PrefilterSamples);
        prefilterMaterial->SetTexture("cw_samplerEnv", m_EnvironmentCubemap);
        for (uint32_t j = 0; j < 6; j++)
        {
            const float farPlane = (float)res;
            const glm::mat4 viewProjection = glm::perspective((float)(M_PI * 0.5f), 1.0f, 0.1f, farPlane) * s_CubeFaceMatrices[j];
            prefilterMaterial->SetMatrix("mvp", viewProjection);
            for (uint32_t i = 0; i <= numMips; i++)
            {
                const float roughness = (float)i / (float)(numMips);
                prefilterMaterial->SetFloat("roughness", roughness);
                RenderTextureDesc rtProps;
                rtProps.ColorSurfaces[0].Texture = m_PrefilteredMap;
                rtProps.ColorSurfaces[0].Face = j;
                rtProps.ColorSurfaces[0].NumFaces = 1;
                rtProps.ColorSurfaces[0].MipLevel = i;
                rtProps.Width = res >> i;
                rtProps.Height = res >> i;
                const Ref<RenderTexture> target = RenderTexture::Create(rtProps);
                rapi.SetRenderTarget(target);
                rapi.SetGraphicsPipeline(prefilterMaterial->GetGraphicsPipeline());
                rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                rapi.SetUniforms(prefilterMaterial->GetUniformParams());
                rapi.SetVertexBuffers(0, &m_CubeVbo, 1);
                rapi.SetIndexBuffer(m_CubeIbo);
                rapi.DrawIndexed(0, 36, 0, 72);
            }
        }
    }

} // namespace Crowny
