#include "cwpch.h"

#include "Crowny/Renderer/EnvironmentMap.h"

#include "Crowny/Common/Timer.h"
#include "Crowny/Import/ImageLoader.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Material.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    EnvironmentMap::EnvironmentMap(const Path& hdrPath) : EnvironmentMap(hdrPath, Settings{}) {}

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

        ImageLoadOptions loadOptions;
        loadOptions.FlipVertically = true;
        ImageLoadResult image = ImageLoader::Decode(hdrPath, loadOptions);
        if (!image || !image.Info.IsHDR || !image.Pixels)
        {
            CW_ENGINE_ERROR("Failed to load HDR image '{}': {}", hdrPath.string(), image.Error);
            return;
        }

        Ref<PixelData> rgbaPixels = image.Pixels;
        if (rgbaPixels->GetFormat() != TextureFormat::RGBA32F)
        {
            rgbaPixels = PixelData::Create(image.Info.Width, image.Info.Height, 1, TextureFormat::RGBA32F);
            if (!rgbaPixels || !PixelUtils::ConvertPixels(*image.Pixels, *rgbaPixels))
            {
                CW_ENGINE_ERROR("Failed to convert HDR image '{}' to RGBA32F.", hdrPath.string());
                return;
            }
        }
        ComputeDiffuseSh(reinterpret_cast<const float*>(rgbaPixels->GetData()), image.Info.Width, image.Info.Height);

        TextureDesc equirectProps;
        equirectProps.Width = image.Info.Width;
        equirectProps.Height = image.Info.Height;
        equirectProps.Usage = TextureUsage::TEXTURE_STATIC;
        equirectProps.Format = TextureFormat::RGBA32F;
        equirectProps.DebugName = "EnvMap/Equirect";
        const Ref<Texture> equirectTexture = Texture::Create(equirectProps);
        equirectTexture->WriteData(*rgbaPixels);

        // Create environment cubemap
        RenderAPI& rapi = (*RenderAPI::TryGet());
        TextureDesc cubeProps;
        cubeProps.Width = m_Settings.CubemapResolution;
        cubeProps.Height = m_Settings.CubemapResolution;
        cubeProps.Faces = 6;
        cubeProps.Shape = TextureShape::TEXTURE_CUBE;
        cubeProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        cubeProps.Format = TextureFormat::RGBA16F;
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

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>(EQUIRECTTOCUBE_SHADER_PATH);
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
        auto& rapi = (*RenderAPI::TryGet());
        TextureDesc tProps;
        tProps.Width = m_Settings.IrradianceResolution;
        tProps.Height = m_Settings.IrradianceResolution;
        tProps.Format = TextureFormat::RGBA16F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = 0;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.DebugName = "EnvMap/Irradiance";
        m_IrradianceMap = Texture::Create(tProps);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>(PREFILTER_SHADER_PATH);
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
        auto& rapi = (*RenderAPI::TryGet());
        const uint32_t res = m_Settings.PrefilteredResolution;
        const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(res)));
        TextureDesc tProps;
        tProps.Width = res;
        tProps.Height = res;
        tProps.Format = TextureFormat::RGBA16F;
        tProps.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        tProps.MipLevels = numMips;
        tProps.Faces = 6;
        tProps.Shape = TextureShape::TEXTURE_CUBE;
        tProps.DebugName = "EnvMap/Prefiltered";
        m_PrefilteredMap = Texture::Create(tProps);

        const AssetHandle<Shader> shaderHandle = AssetManager::TryGet()->Load<Shader>(FILTER_SHADER_PATH);
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

    void EnvironmentMap::ComputeDiffuseSh(const float* pixels, uint32_t width, uint32_t height)
    {
        m_DiffuseSh = {};
        if (pixels == nullptr || width == 0 || height == 0)
            return;

        std::array<glm::vec3, 9> coefficients{};
        const uint32_t step = std::max(std::max(width, height) / 1024u, 1u);
        const float longitudeStep = glm::two_pi<float>() * static_cast<float>(step) / static_cast<float>(width);
        const float latitudeStep = glm::pi<float>() * static_cast<float>(step) / static_cast<float>(height);
        for (uint32_t y = 0; y < height; y += step)
        {
            const float v = (static_cast<float>(y) + 0.5f * static_cast<float>(step)) / static_cast<float>(height);
            const float latitude = (v - 0.5f) * glm::pi<float>();
            const float cosLatitude = std::cos(latitude);
            const float sinLatitude = std::sin(latitude);
            const float solidAngle = std::max(cosLatitude, 0.0f) * longitudeStep * latitudeStep;
            for (uint32_t x = 0; x < width; x += step)
            {
                const float u = (static_cast<float>(x) + 0.5f * static_cast<float>(step)) / static_cast<float>(width);
                const float longitude = (u - 0.5f) * glm::two_pi<float>();
                const glm::vec3 direction(std::cos(longitude) * cosLatitude, sinLatitude,
                                          std::sin(longitude) * cosLatitude);
                const uint64_t pixel = (static_cast<uint64_t>(std::min(y, height - 1u)) * width +
                                        std::min(x, width - 1u)) * 4u;
                const glm::vec3 radiance = glm::max(glm::vec3(pixels[pixel], pixels[pixel + 1u], pixels[pixel + 2u]),
                                                    glm::vec3(0.0f));
                const std::array<float, 9> basis = {
                    0.282095f,
                    0.488603f * direction.y,
                    0.488603f * direction.z,
                    0.488603f * direction.x,
                    1.092548f * direction.x * direction.y,
                    1.092548f * direction.y * direction.z,
                    0.315392f * (3.0f * direction.z * direction.z - 1.0f),
                    1.092548f * direction.x * direction.z,
                    0.546274f * (direction.x * direction.x - direction.y * direction.y),
                };
                for (uint32_t coefficient = 0; coefficient < coefficients.size(); coefficient++)
                    coefficients[coefficient] += radiance * basis[coefficient] * solidAngle;
            }
        }

        const std::array<float, 3> convolution = { glm::pi<float>(), 2.0f * glm::pi<float>() / 3.0f,
                                                    glm::pi<float>() / 4.0f };
        for (uint32_t coefficient = 0; coefficient < coefficients.size(); coefficient++)
        {
            const uint32_t band = coefficient == 0 ? 0u : coefficient < 4 ? 1u : 2u;
            m_DiffuseSh[coefficient] = glm::vec4(coefficients[coefficient] * convolution[band], 0.0f);
        }
    }

} // namespace Crowny
