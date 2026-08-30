#include "RenderTestRunner.h"

#include "RenderTestImage.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/MeshProcessing.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneCamera.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Utils/PixelUtils.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

namespace Crowny::RenderTests
{
    namespace
    {
        constexpr uint32_t TEST_WIDTH = 64u;
        constexpr uint32_t TEST_HEIGHT = 64u;

        struct TestCase
        {
            String Name;
            Tolerance AllowedDifference;
            std::function<bool(Image&, String&)> Render;
        };

        struct TestResult
        {
            String Name;
            bool Passed = false;
            bool Updated = false;
            Comparison Difference;
            String Message;
        };

        String BackendName(RenderAPI::API backend) { return backend == RenderAPI::API::OpenGL ? "opengl" : "vulkan"; }

        String EscapeJson(StringView value)
        {
            String result;
            result.reserve(value.size());
            for (const char character : value)
            {
                switch (character)
                {
                case '\\':
                    result += "\\\\";
                    break;
                case '"':
                    result += "\\\"";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += character;
                    break;
                }
            }
            return result;
        }

        Ref<Texture> CreateRenderTexture(uint32_t width, uint32_t height, TextureFormat format, StringView debugName,
                                         bool loadStore = false)
        {
            TextureDesc textureDesc;
            textureDesc.Width = width;
            textureDesc.Height = height;
            textureDesc.Format = format;
            textureDesc.Usage = PixelUtils::IsDepthFormat(format) ? TextureUsage::TEXTURE_DEPTHSTENCIL : TextureUsage::TEXTURE_RENDERTARGET;
            if (loadStore)
                textureDesc.Usage = static_cast<TextureUsage>(textureDesc.Usage | TextureUsage::TEXTURE_LOADSTORE);
            textureDesc.sRGB = false;
            textureDesc.ReadWrite = true;
            textureDesc.DebugName = String(debugName);
            return Texture::Create(textureDesc);
        }

        bool CompileGraphicsPass(StringView name, const String& source, Ref<ShaderRenderPass>& output, String& error)
        {
            const ShaderCompileResult result = ShaderCompiler::CompileWithDiagnostics(String(name), source, ShaderLanguage::VKSL);
            if (!result.Succeeded())
            {
                for (const ShaderDiagnostic& diagnostic : result.Diagnostics)
                {
                    if (!error.empty())
                        error += "; ";
                    error += diagnostic.Message;
                }
                if (error.empty())
                    error = String(name) + " failed without diagnostics";
                return false;
            }
            if (result.Description.Techniques.empty() || result.Description.Techniques.front()->GetRenderPasses().empty())
            {
                error = String(name) + " produced no graphics pass";
                return false;
            }
            const Ref<ShaderTechnique>& technique = result.Description.Techniques.front();
            technique->Compile();
            output = technique->GetRenderPasses().front();
            if (!output || !output->GetGraphicsPipeline())
            {
                error = String(name) + " produced no graphics pipeline";
                return false;
            }
            return true;
        }

        Ref<Texture> CreateColorTexture(uint32_t width, uint32_t height, StringView debugName)
        {
            return CreateRenderTexture(width, height, TextureFormat::RGBA8, debugName);
        }

        Ref<RenderTexture> CreateTarget(const Vector<Ref<Texture>>& colors, uint32_t width, uint32_t height,
                                        const Ref<Texture>& depth = nullptr)
        {
            RenderTextureDesc targetDesc;
            targetDesc.Width = width;
            targetDesc.Height = height;
            targetDesc.Samples = 1u;
            for (uint32_t index = 0; index < colors.size(); ++index)
                targetDesc.ColorSurfaces[index].Texture = colors[index];
            targetDesc.DepthSurface.Texture = depth;
            return RenderTexture::Create(targetDesc);
        }

        bool ReadTexture(const Ref<Texture>& texture, PixelData& pixels, String& error)
        {
            if (!texture)
            {
                error = "The render target texture is missing";
                return false;
            }
            pixels.AllocateInternalBuffer();
            texture->ReadData(pixels);
            if (!pixels.IsValid())
            {
                error = "Texture readback returned invalid pixel data";
                return false;
            }
            return true;
        }

        bool Capture(const Ref<Texture>& texture, Image& image, String& error)
        {
            if (!texture)
            {
                error = "The render target has no color texture";
                return false;
            }
            PixelData pixels(texture->GetWidth(), texture->GetHeight(), 1u, TextureFormat::RGBA8);
            if (!ReadTexture(texture, pixels, error))
                return false;

            Image captured(texture->GetWidth(), texture->GetHeight());
            const bool flipVertically = RenderAPI::GetAPI() == RenderAPI::API::OpenGL;
            for (uint32_t y = 0; y < captured.Height; ++y)
            {
                const uint32_t sourceY = flipVertically ? captured.Height - 1u - y : y;
                const uint8_t* source = pixels.GetData() + static_cast<size_t>(sourceY) * pixels.GetRowPitch();
                std::memcpy(captured.Pixel(0, y), source, static_cast<size_t>(captured.Width) * 4u);
            }
            image = std::move(captured);
            return true;
        }

        bool RenderSolidClear(Image& image, String& error)
        {
            const Ref<Texture> color = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/SolidClear");
            const Ref<RenderTexture> target = CreateTarget({ color }, TEST_WIDTH, TEST_HEIGHT);
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            return Capture(color, image, error);
        }

        bool RenderDepthOutputCase(bool writeVelocity, bool writeObjectID, Image& image, uint32_t destinationX, uint32_t destinationY,
                                   String& error)
        {
            static const String source = R"(#lang glsl
#pragma depth_read true
#pragma depth_write true
#pragma depth_compare greater_equal
#pragma cull none
#type vertex
#version 450

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    // The far clip endpoint maps to depth one in both Vulkan [0, 1] and OpenGL [-1, 1] NDC.
    gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
}

#type fragment
#version 450

#ifdef CROWNY_TEST_VELOCITY
layout(location = 0) out vec2 cwVelocity;
#endif
#ifdef CROWNY_TEST_OBJECT_ID
#ifdef CROWNY_TEST_VELOCITY
layout(location = 1) out int cwObjectId;
#else
layout(location = 0) out int cwObjectId;
#endif
#endif

void main()
{
#ifdef CROWNY_TEST_VELOCITY
    cwVelocity = vec2(0.25, -0.5);
#endif
#ifdef CROWNY_TEST_OBJECT_ID
    cwObjectId = 37;
#endif
}
)";

            UnorderedMap<String, String> defines;
            if (writeVelocity)
                defines["CROWNY_TEST_VELOCITY"] = "1";
            if (writeObjectID)
                defines["CROWNY_TEST_OBJECT_ID"] = "1";
            const String caseName = "Crowny-RenderTests/DepthOutput" + std::to_string(writeVelocity) + std::to_string(writeObjectID) + ".glsl";
            const ShaderCompileResult compileResult =
              ShaderCompiler::CompileWithDiagnostics(caseName, source, ShaderLanguage::VKSL, defines);
            if (!compileResult.Succeeded())
            {
                for (const ShaderDiagnostic& diagnostic : compileResult.Diagnostics)
                {
                    if (!error.empty())
                        error += "; ";
                    error += diagnostic.Message;
                }
                if (error.empty())
                    error = "Depth-output shader compilation failed without diagnostics";
                return false;
            }

            constexpr uint32_t quadrantSize = TEST_WIDTH / 2u;
            const Ref<Texture> depth = CreateRenderTexture(quadrantSize, quadrantSize, TextureFormat::DEPTH32F, "RenderTests/DepthOutputDepth");
            const Ref<Texture> velocity =
              writeVelocity ? CreateRenderTexture(quadrantSize, quadrantSize, TextureFormat::RG16F, "RenderTests/DepthOutputVelocity") : nullptr;
            const Ref<Texture> objectID =
              writeObjectID ? CreateRenderTexture(quadrantSize, quadrantSize, TextureFormat::R32I, "RenderTests/DepthOutputObjectID") : nullptr;
            Vector<Ref<Texture>> colors;
            if (velocity)
                colors.push_back(velocity);
            if (objectID)
                colors.push_back(objectID);
            const Ref<RenderTexture> target = CreateTarget(colors, quadrantSize, quadrantSize, depth);
            if (!depth || (writeVelocity && !velocity) || (writeObjectID && !objectID) || !target)
            {
                error = "Could not create the depth-output render target";
                return false;
            }

            if (compileResult.Description.Techniques.empty() || compileResult.Description.Techniques.front()->GetRenderPasses().empty())
            {
                error = "Depth-output shader compilation produced no graphics pass";
                return false;
            }
            const Ref<ShaderTechnique>& technique = compileResult.Description.Techniques.front();
            technique->Compile();
            const Ref<ShaderRenderPass>& pass = technique->GetRenderPasses().front();
            if (!pass || !pass->GetGraphicsPipeline())
            {
                error = "Depth-output shader compilation produced no graphics pipeline";
                return false;
            }
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().ClearRenderTarget(FBT_DEPTH, glm::vec4(0.0f), 0.0f);
            RenderAPI::Get().SetGraphicsPipeline(pass->GetGraphicsPipeline());
            RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
            RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
            RenderAPI::Get().Draw(0u, 3u, 1u);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);

            PixelData depthPixels(quadrantSize, quadrantSize, 1u, TextureFormat::DEPTH32F);
            if (!ReadTexture(depth, depthPixels, error))
                return false;
            const float depthValue = depthPixels.GetColorAt(quadrantSize / 2u, quadrantSize / 2u).r;
            if (depthValue < 0.5f)
            {
                error = "Depth-only attachment was not written";
                return false;
            }
            glm::vec4 velocityValue(0.0f);
            if (velocity)
            {
                PixelData velocityPixels(quadrantSize, quadrantSize, 1u, TextureFormat::RG16F);
                if (!ReadTexture(velocity, velocityPixels, error))
                    return false;
                velocityValue = velocityPixels.GetColorAt(quadrantSize / 2u, quadrantSize / 2u);
                if (std::abs(velocityValue.x - 0.25f) > 0.01f || std::abs(velocityValue.y + 0.5f) > 0.01f)
                {
                    error = "RG16F velocity attachment returned the wrong value";
                    return false;
                }
            }
            float objectIdValue = 0.0f;
            if (objectID)
            {
                PixelData objectIdPixels(quadrantSize, quadrantSize, 1u, TextureFormat::R32I);
                if (!ReadTexture(objectID, objectIdPixels, error))
                    return false;
                objectIdValue = objectIdPixels.GetColorAt(quadrantSize / 2u, quadrantSize / 2u).r;
                if (objectIdValue != 37.0f)
                {
                    error = "R32I object-ID attachment returned the wrong value";
                    return false;
                }
            }

            const auto encodeUnit = [](float value) {
                return static_cast<uint8_t>(std::round(glm::clamp(value, 0.0f, 1.0f) * 255.0f));
            };
            const auto encodeSigned = [&](float value) { return encodeUnit(value * 0.5f + 0.5f); };
            const std::array<uint8_t, 4> readbackColor = {
                encodeUnit(depthValue),
                writeVelocity ? encodeSigned(velocityValue.x) : uint8_t{ 0 },
                writeVelocity ? encodeSigned(velocityValue.y) : uint8_t{ 0 },
                writeObjectID ? static_cast<uint8_t>(glm::clamp(objectIdValue, 0.0f, 255.0f)) : uint8_t{ 255 },
            };
            for (uint32_t y = 0; y < quadrantSize; ++y)
            {
                for (uint32_t x = 0; x < quadrantSize; ++x)
                    std::memcpy(image.Pixel(destinationX + x, destinationY + y), readbackColor.data(), readbackColor.size());
            }
            return true;
        }

        bool RenderDepthOutputMatrix(Image& image, String& error)
        {
            Image matrix(TEST_WIDTH, TEST_HEIGHT);
            if (!RenderDepthOutputCase(false, false, matrix, 0u, 0u, error) ||
                !RenderDepthOutputCase(true, false, matrix, TEST_WIDTH / 2u, 0u, error) ||
                !RenderDepthOutputCase(false, true, matrix, 0u, TEST_HEIGHT / 2u, error) ||
                !RenderDepthOutputCase(true, true, matrix, TEST_WIDTH / 2u, TEST_HEIGHT / 2u, error))
                return false;
            image = std::move(matrix);
            return true;
        }

        bool RenderMrtClear(Image& image, String& error)
        {
            const Ref<Texture> first = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/Mrt0");
            const Ref<Texture> second = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/Mrt1");
            const Ref<RenderTexture> target = CreateTarget({ first, second }, TEST_WIDTH, TEST_HEIGHT);
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(1.0f, 0.125f, 0.25f, 1.0f), 1.0f, 0u,
                                               1u << 0u);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.125f, 0.875f, 0.5f, 1.0f), 1.0f, 0u,
                                               1u << 1u);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);

            Image firstImage;
            Image secondImage;
            if (!Capture(first, firstImage, error) || !Capture(second, secondImage, error))
                return false;
            Image combined(TEST_WIDTH * 2u, TEST_HEIGHT);
            for (uint32_t y = 0; y < TEST_HEIGHT; ++y)
            {
                std::memcpy(combined.Pixel(0, y), firstImage.Pixel(0, y), static_cast<size_t>(TEST_WIDTH) * 4u);
                std::memcpy(combined.Pixel(TEST_WIDTH, y), secondImage.Pixel(0, y), static_cast<size_t>(TEST_WIDTH) * 4u);
            }
            image = std::move(combined);
            return true;
        }

        bool RenderFullscreenPattern(Image& image, String& error)
        {
            static const String source = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) out vec4 outColor;

void main()
{
    float canonicalY = gl_FragCoord.y;
#ifdef CROWNY_TEST_OPENGL
    canonicalY = 64.0 - canonicalY;
#endif
    bool right = gl_FragCoord.x >= 32.0;
    bool bottom = canonicalY >= 32.0;
    if (!right && !bottom)
        outColor = vec4(0.9, 0.1, 0.2, 1.0);
    else if (right && !bottom)
        outColor = vec4(0.1, 0.8, 0.25, 1.0);
    else if (!right && bottom)
        outColor = vec4(0.15, 0.3, 0.95, 1.0);
    else
        outColor = vec4(0.95, 0.8, 0.1, 1.0);
}
)";

            UnorderedMap<String, String> defines;
            if (RenderAPI::GetAPI() == RenderAPI::API::OpenGL)
                defines["CROWNY_TEST_OPENGL"] = "1";
            const ShaderCompileResult compileResult =
              ShaderCompiler::CompileWithDiagnostics("Crowny-RenderTests/FullscreenPattern.glsl", source, ShaderLanguage::VKSL, defines);
            if (!compileResult.Succeeded())
            {
                for (const ShaderDiagnostic& diagnostic : compileResult.Diagnostics)
                {
                    if (!error.empty())
                        error += "; ";
                    error += diagnostic.Message;
                }
                if (error.empty())
                    error = "Fullscreen pattern shader compilation failed without diagnostics";
                return false;
            }

            const Ref<ShaderTechnique>& technique = compileResult.Description.Techniques.front();
            technique->Compile();
            const Ref<ShaderRenderPass>& pass = technique->GetRenderPasses().front();
            const Ref<Texture> color = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/FullscreenPattern");
            const Ref<RenderTexture> target = CreateTarget({ color }, TEST_WIDTH, TEST_HEIGHT);
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.0f));
            RenderAPI::Get().SetGraphicsPipeline(pass->GetGraphicsPipeline());
            RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
            RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
            RenderAPI::Get().Draw(0u, 3u, 1u);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            return Capture(color, image, error);
        }

        bool RenderMipSelection(Image& image, String& error)
        {
            static const String source = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450

layout(location = 0) out vec2 cwUv;

void main()
{
    cwUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(cwUv * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) in vec2 cwUv;
layout(set = 0, binding = 0) uniform sampler2D cwMipTexture;
layout(location = 0) out vec4 outColor;

void main()
{
    float mip = min(floor(cwUv.x * 4.0), 3.0);
    outColor = textureLod(cwMipTexture, vec2(0.5), mip);
}
)";

            const ShaderCompileResult compileResult =
              ShaderCompiler::CompileWithDiagnostics("Crowny-RenderTests/MipSelection.glsl", source, ShaderLanguage::VKSL);
            if (!compileResult.Succeeded())
            {
                for (const ShaderDiagnostic& diagnostic : compileResult.Diagnostics)
                {
                    if (!error.empty())
                        error += "; ";
                    error += diagnostic.Message;
                }
                if (error.empty())
                    error = "Mip-selection shader compilation failed without diagnostics";
                return false;
            }

            TextureDesc textureDesc;
            textureDesc.Width = 8u;
            textureDesc.Height = 8u;
            textureDesc.MipLevels = 4u;
            textureDesc.Format = TextureFormat::RGBA8;
            textureDesc.Usage = TextureUsage::TEXTURE_STATIC;
            textureDesc.sRGB = false;
            textureDesc.ReadWrite = true;
            textureDesc.DebugName = "RenderTests/MipSelectionSource";
            const Ref<Texture> texture = Texture::Create(textureDesc);
            if (!texture)
            {
                error = "Could not create the mip-selection source texture";
                return false;
            }

            const glm::vec4 colors[] = {
                glm::vec4(0.9f, 0.1f, 0.2f, 1.0f),
                glm::vec4(0.1f, 0.8f, 0.25f, 1.0f),
                glm::vec4(0.15f, 0.3f, 0.95f, 1.0f),
                glm::vec4(0.95f, 0.8f, 0.1f, 1.0f),
            };
            for (uint32_t mip = 0; mip < 4u; ++mip)
            {
                const uint32_t width = std::max(8u >> mip, 1u);
                const uint32_t height = std::max(8u >> mip, 1u);
                PixelData pixels(width, height, 1u, TextureFormat::RGBA8);
                pixels.AllocateInternalBuffer();
                for (uint32_t y = 0; y < height; ++y)
                {
                    for (uint32_t x = 0; x < width; ++x)
                        pixels.SetColorAt(x, y, colors[mip]);
                }
                texture->WriteData(pixels, mip);
            }

            SamplerStateDesc samplerDesc;
            samplerDesc.MinFilter = TextureFilter::NEAREST;
            samplerDesc.MagFilter = TextureFilter::NEAREST;
            samplerDesc.MipFilter = TextureFilter::NEAREST;
            samplerDesc.MaxAnsio = 1u;
            samplerDesc.MipMin = 0.0f;
            samplerDesc.MipMax = 3.0f;
            samplerDesc.AddressMode = { TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE,
                                        TextureWrap::CLAMP_TO_EDGE };
            const Ref<SamplerState> sampler = SamplerState::Create(samplerDesc);

            const Ref<ShaderTechnique>& technique = compileResult.Description.Techniques.front();
            technique->Compile();
            const Ref<ShaderRenderPass>& pass = technique->GetRenderPasses().front();
            const Ref<UniformParams> uniforms = UniformParams::Create(pass->GetGraphicsPipeline());
            uniforms->SetTexture(0u, 0u, texture);
            uniforms->SetSamplerState(0u, 0u, sampler);

            const Ref<Texture> color = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/MipSelection");
            const Ref<RenderTexture> target = CreateTarget({ color }, TEST_WIDTH, TEST_HEIGHT);
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.0f));
            RenderAPI::Get().SetGraphicsPipeline(pass->GetGraphicsPipeline());
            RenderAPI::Get().SetUniforms(uniforms);
            RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
            RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
            RenderAPI::Get().Draw(0u, 3u, 1u);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            return Capture(color, image, error);
        }

        bool RenderPostSharpening(Image& image, String& error)
        {
            AssetManager* assetManager = AssetManager::TryGet();
            if (assetManager == nullptr)
            {
                error = "Asset manager is unavailable for the tone-map sharpening shader";
                return false;
            }

            GraphicsMaterial toneMap;
            if (!toneMap.Initialize(assetManager->Load<Shader>("Resources/Shaders/ToneMap.asset")))
            {
                error = "Could not initialize the tone-map sharpening shader: " + toneMap.GetError();
                return false;
            }

            const Ref<Texture> hdrColor = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/SharpeningHdr");
            const Ref<Texture> objectIds = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::R32I, "RenderTests/SharpeningObjectIds");
            const Ref<Texture> sceneDepth = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::DEPTH32F, "RenderTests/SharpeningDepth");
            const Ref<Texture> bloom = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/SharpeningBloom");
            if (!hdrColor || !objectIds || !sceneDepth || !bloom)
            {
                error = "Could not create tone-map sharpening inputs";
                return false;
            }

            PixelData hdrPixels(TEST_WIDTH, TEST_HEIGHT, 1u, TextureFormat::RGBA16F);
            PixelData objectIdPixels(TEST_WIDTH, TEST_HEIGHT, 1u, TextureFormat::R32I);
            PixelData bloomPixels(TEST_WIDTH, TEST_HEIGHT, 1u, TextureFormat::RGBA16F);
            hdrPixels.AllocateInternalBuffer();
            objectIdPixels.AllocateInternalBuffer();
            bloomPixels.AllocateInternalBuffer();
            constexpr std::array<float, 8> levels = { 0.06f, 0.10f, 0.16f, 0.25f, 0.38f, 0.52f, 0.68f, 0.82f };
            for (uint32_t y = 0u; y < TEST_HEIGHT; ++y)
            {
                for (uint32_t x = 0u; x < TEST_WIDTH; ++x)
                {
                    const float level = levels[std::min(x / 8u, static_cast<uint32_t>(levels.size() - 1u))];
                    hdrPixels.SetColorAt(x, y, glm::vec4(level, level * 0.72f, level * 0.45f, 0.6f));
                    objectIdPixels.SetColorAt(x, y, glm::vec4(17.0f));
                    bloomPixels.SetColorAt(x, y, glm::vec4(0.0f));
                }
            }
            hdrColor->WriteData(hdrPixels);
            objectIds->WriteData(objectIdPixels);
            bloom->WriteData(bloomPixels);

            const Ref<RenderTexture> depthTarget = CreateTarget({}, TEST_WIDTH, TEST_HEIGHT, sceneDepth);
            if (!depthTarget)
            {
                error = "Could not create the tone-map sharpening input depth target";
                return false;
            }
            RenderAPI::Get().SetRenderTarget(depthTarget);
            RenderAPI::Get().ClearRenderTarget(FBT_DEPTH, glm::vec4(0.0f), 0.35f);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);

            struct alignas(16) ToneMapConstants
            {
                float Exposure = 1.0f;
                float BloomIntensity = 0.0f;
                float SharpeningStrength = 0.0f;
                float Padding = 0.0f;
            } constants;
            static_assert(sizeof(ToneMapConstants) == 16u);

            auto renderCapture = [&](float sharpeningStrength, StringView debugName, Image& capture) {
                const Ref<Texture> color = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, String(debugName) + "/Color");
                const Ref<Texture> outputObjectIds =
                  CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::R32I, String(debugName) + "/ObjectIds");
                const Ref<Texture> depth = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::DEPTH32F, String(debugName) + "/Depth");
                const Ref<RenderTexture> target = CreateTarget({ color, outputObjectIds }, TEST_WIDTH, TEST_HEIGHT, depth);
                if (!color || !outputObjectIds || !depth || !target)
                {
                    error = "Could not create tone-map sharpening output resources";
                    return false;
                }

                constants.SharpeningStrength = sharpeningStrength;
                if (!toneMap.WriteUniformBlock(0u, 1u, &constants, sizeof(constants)) || !toneMap.SetTexture(0u, 0u, hdrColor) ||
                    !toneMap.SetTexture(0u, 2u, objectIds) || !toneMap.SetTexture(0u, 3u, sceneDepth) || !toneMap.SetTexture(0u, 4u, bloom))
                {
                    error = "Could not bind tone-map sharpening inputs";
                    return false;
                }

                RenderAPI::Get().SetRenderTarget(target);
                RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                RenderAPI::Get().ClearRenderTarget(FBT_COLOR | FBT_DEPTH, glm::vec4(0.0f), 0.0f, 0u, 1u << 0u);
                if (!toneMap.Bind())
                {
                    error = "Could not bind the tone-map sharpening material";
                    return false;
                }
                RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
                RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
                RenderAPI::Get().Draw(0u, 3u, 1u);
                RenderAPI::Get().SubmitCommandBuffer(nullptr);
                return Capture(color, capture, error);
            };

            Image baseline;
            Image sharpened;
            if (!renderCapture(0.0f, "RenderTests/SharpeningOff", baseline) || !renderCapture(1.0f, "RenderTests/SharpeningOn", sharpened))
                return false;

            auto edgeEnergy = [](const Image& input) {
                uint64_t energy = 0u;
                for (uint32_t y = 0u; y < input.Height; ++y)
                {
                    for (uint32_t x = 1u; x < input.Width; ++x)
                    {
                        const uint8_t* left = input.Pixel(x - 1u, y);
                        const uint8_t* right = input.Pixel(x, y);
                        for (uint32_t channel = 0u; channel < 3u; ++channel)
                            energy += static_cast<uint64_t>(std::abs(static_cast<int32_t>(right[channel]) - left[channel]));
                    }
                }
                return energy;
            };
            if (edgeEnergy(sharpened) <= edgeEnergy(baseline))
            {
                error = "Sharpening did not increase deterministic edge contrast";
                return false;
            }
            constexpr uint8_t expectedAlpha = 255u;
            if (std::abs(static_cast<int32_t>(baseline.Pixel(TEST_WIDTH / 2u, TEST_HEIGHT / 2u)[3]) - expectedAlpha) > 2 ||
                std::abs(static_cast<int32_t>(sharpened.Pixel(TEST_WIDTH / 2u, TEST_HEIGHT / 2u)[3]) - expectedAlpha) > 2)
            {
                error = "Tone-map sharpening changed the established opaque output alpha";
                return false;
            }

            Image comparison(TEST_WIDTH * 2u, TEST_HEIGHT);
            for (uint32_t y = 0u; y < TEST_HEIGHT; ++y)
            {
                std::memcpy(comparison.Pixel(0u, y), baseline.Pixel(0u, y), static_cast<size_t>(TEST_WIDTH) * 4u);
                std::memcpy(comparison.Pixel(TEST_WIDTH, y), sharpened.Pixel(0u, y), static_cast<size_t>(TEST_WIDTH) * 4u);
            }
            image = std::move(comparison);
            return true;
        }

        bool RenderWeightedOit(Image& image, String& error)
        {
            static const String accumulationSource = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
blend_state { enabled = true; color = { one, one, add }; alpha = { one, one, add }; };
#type vertex
#version 450

layout(location = 0) flat out int cwLayer;

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    cwLayer = gl_InstanceIndex;
}

#type fragment
#version 450

layout(location = 0) flat in int cwLayer;
layout(location = 0) out vec4 cwAccumulation;

void main()
{
    bool reverseOrder = gl_FragCoord.y >= 32.0;
    bool redLayer = (cwLayer == 0) != reverseOrder;
    if ((redLayer && gl_FragCoord.x >= 48.0) || (!redLayer && gl_FragCoord.x < 16.0))
        discard;
    vec3 color = redLayer ? vec3(0.9, 0.1, 0.05) : vec3(0.05, 0.2, 0.95);
    float alpha = redLayer ? 0.5 : 0.25;
    float alphaWeight = pow(min(1.0, alpha * 10.0) + 0.01, 3.0);
    float depthWeight = pow(0.1 + gl_FragCoord.z * 0.9, 3.0);
    float weight = clamp(alphaWeight * 1e8 * depthWeight, 1e-2, 3e3);
    cwAccumulation = vec4(color * alpha, alpha) * weight;
}
)";
            static const String revealageSource = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
blend_state { enabled = true; color = { zero, srcia, add }; alpha = { zero, srcia, add }; };
#type vertex
#version 450

layout(location = 0) flat out int cwLayer;

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    cwLayer = gl_InstanceIndex;
}

#type fragment
#version 450

layout(location = 0) flat in int cwLayer;
layout(location = 0) out vec4 cwRevealage;

void main()
{
    bool reverseOrder = gl_FragCoord.y >= 32.0;
    bool redLayer = (cwLayer == 0) != reverseOrder;
    if ((redLayer && gl_FragCoord.x >= 48.0) || (!redLayer && gl_FragCoord.x < 16.0))
        discard;
    float alpha = redLayer ? 0.5 : 0.25;
    cwRevealage = vec4(alpha);
}
)";
            static const String copySource = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450

layout(location = 0) out vec2 cwUv;

void main()
{
    cwUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(cwUv * 2.0 - 1.0, 0.0, 1.0);
}

#type fragment
#version 450

layout(location = 0) in vec2 cwUv;
layout(set = 0, binding = 0) uniform sampler2D cwHdrColor;
layout(location = 0) out vec4 cwOutput;

void main()
{
    cwOutput = texture(cwHdrColor, cwUv);
}
)";

            Ref<ShaderRenderPass> accumulationPass;
            Ref<ShaderRenderPass> revealagePass;
            Ref<ShaderRenderPass> copyPass;
            if (!CompileGraphicsPass("Crowny-RenderTests/WeightedOitAccumulation.glsl", accumulationSource, accumulationPass, error) ||
                !CompileGraphicsPass("Crowny-RenderTests/WeightedOitRevealage.glsl", revealageSource, revealagePass, error) ||
                !CompileGraphicsPass("Crowny-RenderTests/WeightedOitCopy.glsl", copySource, copyPass, error))
                return false;

            const Ref<Texture> accumulation =
              CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/OitAccumulation");
            const Ref<Texture> revealage =
              CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::R32F, "RenderTests/OitRevealage");
            const Ref<Texture> hdrColor =
              CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/OitHdrColor", true);
            const Ref<Texture> output = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "RenderTests/OitOutput");
            const Ref<RenderTexture> accumulationTarget = CreateTarget({ accumulation }, TEST_WIDTH, TEST_HEIGHT);
            const Ref<RenderTexture> revealageTarget = CreateTarget({ revealage }, TEST_WIDTH, TEST_HEIGHT);
            const Ref<RenderTexture> hdrTarget = CreateTarget({ hdrColor }, TEST_WIDTH, TEST_HEIGHT);
            const Ref<RenderTexture> outputTarget = CreateTarget({ output }, TEST_WIDTH, TEST_HEIGHT);
            if (!accumulation || !revealage || !hdrColor || !output || !accumulationTarget || !revealageTarget || !hdrTarget || !outputTarget)
            {
                error = "Could not create weighted-OIT textures or render targets";
                return false;
            }

            RenderAPI::Get().SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
            RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
            RenderAPI::Get().SetDrawMode(DrawMode::TRIANGLE_LIST);
            RenderAPI::Get().SetRenderTarget(hdrTarget);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.1f, 0.2f, 0.3f, 1.0f));
            RenderAPI::Get().SetRenderTarget(accumulationTarget);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.0f));
            RenderAPI::Get().SetGraphicsPipeline(accumulationPass->GetGraphicsPipeline());
            RenderAPI::Get().Draw(0u, 3u, 2u);
            RenderAPI::Get().SetRenderTarget(revealageTarget);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(1.0f));
            RenderAPI::Get().SetGraphicsPipeline(revealagePass->GetGraphicsPipeline());
            RenderAPI::Get().Draw(0u, 3u, 2u);

            if (AssetManager::TryGet() == nullptr)
            {
                error = "Asset manager is unavailable for the weighted-OIT composite shader";
                return false;
            }
            ComputeMaterial composite;
            if (!composite.Initialize(AssetManager::TryGet()->Load<Shader>("Resources/Shaders/WeightedOitComposite.asset")))
            {
                error = "Could not initialize the weighted-OIT composite shader: " + composite.GetError();
                return false;
            }
            struct CompositeConstants
            {
                glm::uvec2 Resolution = glm::uvec2(TEST_WIDTH, TEST_HEIGHT);
            } constants;
            if (!composite.WriteUniformBlock(0u, 0u, &constants, sizeof(constants)))
            {
                error = "Could not write weighted-OIT composite constants";
                return false;
            }
            if (!composite.SetLoadStoreTexture(0u, 1u, hdrColor) || !composite.SetTexture(0u, 2u, accumulation) ||
                !composite.SetTexture(0u, 3u, revealage))
            {
                error = "Could not bind weighted-OIT composite textures";
                return false;
            }
            if (!composite.Dispatch(TEST_WIDTH / 8u, TEST_HEIGHT / 8u))
            {
                error = "Could not dispatch the weighted-OIT composite shader";
                return false;
            }

            SamplerStateDesc samplerDesc;
            samplerDesc.MinFilter = TextureFilter::NEAREST;
            samplerDesc.MagFilter = TextureFilter::NEAREST;
            samplerDesc.MipFilter = TextureFilter::NEAREST;
            samplerDesc.AddressMode = { TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE };
            const Ref<SamplerState> sampler = SamplerState::Create(samplerDesc);
            const Ref<UniformParams> copyUniforms = UniformParams::Create(copyPass->GetGraphicsPipeline());
            if (!sampler || !copyUniforms)
            {
                error = "Could not create weighted-OIT copy resources";
                return false;
            }
            copyUniforms->SetTexture(0u, 0u, hdrColor);
            copyUniforms->SetSamplerState(0u, 0u, sampler);
            RenderAPI::Get().SetRenderTarget(outputTarget);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.0f));
            RenderAPI::Get().SetGraphicsPipeline(copyPass->GetGraphicsPipeline());
            RenderAPI::Get().SetUniforms(copyUniforms);
            RenderAPI::Get().Draw(0u, 3u, 1u);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            if (!Capture(output, image, error))
                return false;

            struct ExpectedPixel
            {
                uint32_t X;
                std::array<uint8_t, 4> Color;
            };
            constexpr std::array expected = {
                ExpectedPixel{ 8u, { 127u, 38u, 45u, 255u } },
                ExpectedPixel{ 32u, { 108u, 40u, 84u, 255u } },
                ExpectedPixel{ 56u, { 22u, 51u, 118u, 255u } },
            };
            for (const ExpectedPixel& sample : expected)
            {
                const uint8_t* forwardOrder = image.Pixel(sample.X, 8u);
                const uint8_t* reverseOrder = image.Pixel(sample.X, 56u);
                for (uint32_t channel = 0; channel < sample.Color.size(); ++channel)
                {
                    if (std::abs(static_cast<int32_t>(forwardOrder[channel]) - sample.Color[channel]) > 2 ||
                        std::abs(static_cast<int32_t>(reverseOrder[channel]) - sample.Color[channel]) > 2 ||
                        std::abs(static_cast<int32_t>(forwardOrder[channel]) - reverseOrder[channel]) > 1)
                    {
                        error = "Weighted-OIT result failed analytic color or order-independence validation";
                        return false;
                    }
                }
            }
            return true;
        }

        bool RenderToonSilhouette(Image& image, String& error)
        {
            constexpr uint32_t toonTestSize = 256u;
            struct CompatibilityRendererScope
            {
                CompatibilityRendererScope()
                {
                    Renderer2D::Init();
                    ForwardRenderer::Init();
                }

                ~CompatibilityRendererScope()
                {
                    Renderer2D::Shutdown();
                    ForwardRenderer::Shutdown();
                }
            } compatibilityRenderers;

            AssetManager* assetManager = AssetManager::TryGet();
            if (assetManager == nullptr)
            {
                error = "The asset manager is unavailable";
                return false;
            }

            const AssetHandle<Shader> toonShader = assetManager->Load<Shader>(TOON_SHADER_PATH);
            const Ref<Material> material = toonShader ? Material::CreateToon(toonShader) : nullptr;
            if (!material)
            {
                error = "Could not create the built-in toon material";
                return false;
            }
            material->SetColor("tint", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            material->SetColor("outlineColor", glm::vec4(1.0f, 0.0f, 0.75f, 1.0f));
            material->SetFloat("thickness", 0.0f);
            material->SetFloat("toonSilhouetteWidth", 1.2f);
            material->SetFloat("toonSpecularStrength", 0.0f);
            material->SetFloat("toonRimStrength", 0.0f);
            material->SetFloat("toonPatternStrength", 0.0f);
            material->SetInt("toonPatternMapping", static_cast<int>(ToonPatternMapping::ProceduralHatch));

            const Ref<MeshData> sphereData = MeshFactory::CreateSphereData(0.75f, 32u, 16u);
            if (!sphereData)
            {
                error = "Could not create the toon test sphere";
                return false;
            }
            MeshDesc meshDesc;
            meshDesc.Data = sphereData;
            meshDesc.Usage = MeshUsage::Dynamic;
            meshDesc.SubMeshes.emplace_back(0u, sphereData->GetIndexCount(), DrawMode::TRIANGLE_LIST);
            meshDesc.GpuGeometry = MeshProcessing::BuildGpuGeometry(*sphereData, meshDesc.SubMeshes);
            if (meshDesc.GpuGeometry.IsEmpty())
            {
                error = "Could not build GPU geometry for the toon test sphere";
                return false;
            }

            const Ref<Mesh> mesh = Mesh::Create(meshDesc);
            const AssetHandle<Mesh> meshHandle = static_asset_cast<Mesh>(assetManager->CreateAssetHandle(mesh));
            const AssetHandle<Material> materialHandle = static_asset_cast<Material>(assetManager->CreateAssetHandle(material));
            if (!meshHandle || !materialHandle)
            {
                error = "Could not create toon test asset handles";
                return false;
            }

            const Ref<Texture> color = CreateColorTexture(toonTestSize, toonTestSize, "RenderTests/ToonSilhouetteColor");
            const Ref<Texture> depth =
              CreateRenderTexture(toonTestSize, toonTestSize, TextureFormat::DEPTH32F, "RenderTests/ToonSilhouetteDepth");
            const Ref<RenderTexture> target = CreateTarget({ color }, toonTestSize, toonTestSize, depth);
            if (!color || !depth || !target)
            {
                error = "Could not create the toon silhouette render target";
                return false;
            }

            const Ref<Scene> scene = CreateRef<Scene>("Toon silhouette render test");
            MeshRendererComponent& component = scene->CreateEntity("Toon sphere").AddComponent<MeshRendererComponent>();
            component.MeshHandle = meshHandle;
            component.SetMaterial(0u, materialHandle);
            component.CastShadows = false;
            component.MotionVectors = false;

            SceneCamera camera;
            camera.SetPerspective(glm::radians(40.0f), 0.1f, 20.0f);
            camera.SetViewportSize(toonTestSize, toonTestSize);
            camera.SetBackgroundColor(glm::vec3(0.0f));
            camera.SetOcclusionCulling(false);
            const glm::mat4 view =
              glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            SceneRenderer renderer(scene, target);
            renderer.Init();
            const bool vulkan = RenderAPI::GetAPI() == RenderAPI::API::Vulkan;
            if (vulkan)
            {
                RenderThread* renderThread = Application::Get().GetRenderThread();
                if (renderThread == nullptr)
                {
                    error = "The Vulkan render thread is unavailable";
                    return false;
                }
                RenderSnapshot& snapshot = renderThread->BeginFrame();
                snapshot.FrameNumber = 1u;
                renderer.ExtractSnapshot(snapshot, camera, view, false);
                renderThread->SubmitFrame();
                renderThread->WaitForFrameDone();
                RenderAPI::Get().SubmitCommandBuffer(nullptr);
            }
            else
            {
                RenderSnapshot snapshot;
                snapshot.FrameNumber = 1u;
                renderer.ExtractSnapshot(snapshot, camera, view, false);
                SceneRenderer::RenderFromSnapshot(snapshot);
                RenderAPI::Get().SubmitCommandBuffer(nullptr);
            }

            const GpuScene& gpuScene = Renderer::GetGpuScene();
            if (!gpuScene.HasToonSilhouetteMaterials())
            {
                error = "The toon silhouette material was not registered in the GPU scene";
                return false;
            }
            if (vulkan)
            {
                const GpuSceneUploadStats& stats = gpuScene.GetStats();
                if (stats.VisibleInstances == 0u || stats.IndirectCommands == 0u)
                {
                    error = "The Vulkan GPU scene produced no visible toon draw commands";
                    return false;
                }
            }

            if (!Capture(color, image, error))
                return false;
            uint32_t outlinePixels = 0u;
            for (uint32_t y = 0u; y < image.Height; ++y)
            {
                for (uint32_t x = 0u; x < image.Width; ++x)
                {
                    const uint8_t* pixel = image.Pixel(x, y);
                    if (pixel[0] > 48u && pixel[2] > 32u && pixel[1] * 2u < std::max(pixel[0], pixel[2]))
                        ++outlinePixels;
                }
            }
            const uint8_t* center = image.Pixel(image.Width / 2u, image.Height / 2u);
            if (outlinePixels < 12u || center[0] > 32u || center[1] > 32u || center[2] > 32u)
            {
                error = "The toon silhouette did not produce a colored hull around a dark interior";
                return false;
            }
            return true;
        }

        Vector<TestCase> BuildCases()
        {
            const Tolerance exact{};
            Tolerance shaderTolerance;
            shaderTolerance.PixelThreshold = 1u;
            shaderTolerance.MaxChannelError = 1u;
            shaderTolerance.MaxMeanAbsoluteError = 0.01;
            shaderTolerance.MaxFailingPixelRatio = 0.001;
            Tolerance toonTolerance;
            // The legacy OpenGL inverted hull is slightly wider than the pixel-scaled GPU-driven Vulkan path.
            // Ignore target-alpha differences and allow only the measured one-pixel silhouette fringe.
            toonTolerance.PixelThreshold = 8u;
            toonTolerance.MaxChannelError = 255u;
            toonTolerance.MaxMeanAbsoluteError = 8.0;
            toonTolerance.MaxFailingPixelRatio = 0.02;
            toonTolerance.CompareAlpha = false;
            return {
                { "solid-clear", exact, RenderSolidClear },
                { "mrt-clear", exact, RenderMrtClear },
                { "fullscreen-pattern", shaderTolerance, RenderFullscreenPattern },
                { "texture-mip-selection", shaderTolerance, RenderMipSelection },
                { "depth-output-matrix", exact, RenderDepthOutputMatrix },
                { "post-sharpening", shaderTolerance, RenderPostSharpening },
                { "weighted-oit", shaderTolerance, RenderWeightedOit },
                { "toon-silhouette", toonTolerance, RenderToonSilhouette },
            };
        }

        bool WriteSummary(const Path& path, const RunnerOptions& options, const Vector<TestResult>& results, const String& device, String& error)
        {
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path());
            std::ofstream stream(path, std::ios::trunc);
            if (!stream)
            {
                error = "Could not create " + path.string();
                return false;
            }
            stream << "{\n  \"backend\": \"" << BackendName(options.Backend) << "\",\n"
                   << "  \"device\": \"" << EscapeJson(device) << "\",\n"
                   << "  \"updatedReferences\": " << (options.UpdateReferences ? "true" : "false") << ",\n"
                   << "  \"tests\": [\n";
            for (size_t index = 0; index < results.size(); ++index)
            {
                const TestResult& result = results[index];
                stream << "    { \"name\": \"" << EscapeJson(result.Name) << "\", \"passed\": " << (result.Passed ? "true" : "false")
                       << ", \"updated\": " << (result.Updated ? "true" : "false")
                       << ", \"maxChannelError\": " << static_cast<uint32_t>(result.Difference.MaxChannelError)
                       << ", \"meanAbsoluteError\": " << std::fixed << std::setprecision(6) << result.Difference.MeanAbsoluteError
                       << ", \"failingPixelRatio\": " << result.Difference.FailingPixelRatio << ", \"message\": \"" << EscapeJson(result.Message)
                       << "\" }" << (index + 1u == results.size() ? "\n" : ",\n");
            }
            stream << "  ]\n}\n";
            return static_cast<bool>(stream);
        }
    } // namespace

    bool ParseOptions(int argc, char** argv, RunnerOptions& options, String& error)
    {
        for (int index = 1; index < argc; ++index)
        {
            const String argument = argv[index];
            const auto readValue = [&](StringView option) -> const char* {
                if (index + 1 >= argc)
                {
                    error = String(option) + " requires a value";
                    return nullptr;
                }
                return argv[++index];
            };
            if (argument == "--help" || argument == "-h")
                options.ShowHelp = true;
            else if (argument == "--update-references")
                options.UpdateReferences = true;
            else if (argument == "--backend")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                const String backend = value;
                if (backend == "vulkan")
                    options.Backend = RenderAPI::API::Vulkan;
                else if (backend == "opengl")
                    options.Backend = RenderAPI::API::OpenGL;
                else
                {
                    error = "Unknown backend '" + backend + "'";
                    return false;
                }
            }
            else if (argument == "--references")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.References = value;
            }
            else if (argument == "--artifacts")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.Artifacts = value;
            }
            else if (argument == "--filter")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.Filter = value;
            }
            else
            {
                error = "Unknown argument '" + argument + "'";
                return false;
            }
        }
        return true;
    }

    void PrintUsage()
    {
        std::cout << "Crowny renderer regression tests\n\n"
                  << "  --backend vulkan|opengl   Select the renderer backend\n"
                  << "  --references PATH        Golden BMP directory\n"
                  << "  --artifacts PATH         Actual, expected, diff, and JSON output\n"
                  << "  --filter TEXT            Run cases whose names contain TEXT\n"
                  << "  --update-references      Replace goldens with this backend's output\n";
    }

    int RunSuite(const RunnerOptions& options)
    {
        const Path backendArtifacts = options.Artifacts / BackendName(options.Backend);
        fs::create_directories(backendArtifacts);
        const Vector<TestCase> cases = BuildCases();
        Vector<TestResult> results;
        uint32_t failed = 0u;
        for (const TestCase& test : cases)
        {
            if (!options.Filter.empty() && test.Name.find(options.Filter) == String::npos)
                continue;
            std::cout << "[ RUN      ] " << test.Name << '\n';
            TestResult result;
            result.Name = test.Name;
            Image actual;
            if (!test.Render(actual, result.Message))
            {
                ++failed;
                results.push_back(result);
                std::cout << "[  FAILED  ] " << test.Name << ": " << result.Message << '\n';
                continue;
            }

            String imageError;
            const Path actualPath = backendArtifacts / (test.Name + ".actual.bmp");
            if (!SaveBmp(actualPath, actual, imageError))
            {
                result.Message = imageError;
                ++failed;
                results.push_back(result);
                std::cout << "[  FAILED  ] " << test.Name << ": " << result.Message << '\n';
                continue;
            }

            const Path referencePath = options.References / (test.Name + ".bmp");
            if (options.UpdateReferences)
            {
                result.Passed = SaveBmp(referencePath, actual, result.Message);
                result.Updated = result.Passed;
                if (!result.Passed)
                    ++failed;
                results.push_back(result);
                std::cout << (result.Passed ? "[  UPDATED ] " : "[  FAILED  ] ") << test.Name;
                if (!result.Message.empty())
                    std::cout << ": " << result.Message;
                std::cout << '\n';
                continue;
            }

            Image expected;
            if (!LoadBmp(referencePath, expected, result.Message))
            {
                result.Message += ". Run with --update-references after reviewing the actual image.";
                ++failed;
                results.push_back(result);
                std::cout << "[  FAILED  ] " << test.Name << ": " << result.Message << '\n';
                continue;
            }

            result.Difference = Compare(expected, actual, test.AllowedDifference);
            result.Passed = result.Difference.Passed;
            result.Message = result.Difference.Message;
            if (!result.Passed)
            {
                ++failed;
                SaveBmp(backendArtifacts / (test.Name + ".expected.bmp"), expected, imageError);
                const Image diff = BuildDiff(expected, actual);
                SaveBmp(backendArtifacts / (test.Name + ".diff.bmp"), diff, imageError);
            }
            results.push_back(result);
            std::cout << (result.Passed ? "[       OK ] " : "[  FAILED  ] ") << test.Name;
            if (!result.Message.empty())
                std::cout << ": " << result.Message;
            std::cout << '\n';
        }

        RenderAPI::Get().SetGraphicsPipeline(nullptr);
        RenderAPI::Get().SetVertexLayout(nullptr);
        RenderAPI::Get().SetRenderTarget(nullptr);
        RenderAPI::Get().SubmitCommandBuffer(nullptr);

        const String device = RenderAPI::Get().GetCapabilities().DeviceName;
        String summaryError;
        if (!WriteSummary(backendArtifacts / "summary.json", options, results, device, summaryError))
        {
            std::cerr << summaryError << '\n';
            ++failed;
        }
        std::cout << "Ran " << results.size() << " render tests on " << BackendName(options.Backend) << " (" << device << "): " << failed
                  << " failed\n";
        return failed == 0u ? 0 : 1;
    }

    int CompareBackendDirectories(const Path& first, const Path& second, const Path& artifacts)
    {
        fs::create_directories(artifacts);
        uint32_t compared = 0u;
        uint32_t failed = 0u;
        const Vector<TestCase> cases = BuildCases();
        const Tolerance defaultTolerance{ 1u, 1u, 0.01, 0.001, true };
        for (const fs::directory_entry& entry : fs::directory_iterator(first))
        {
            const String suffix = ".actual.bmp";
            const String filename = entry.path().filename().string();
            if (!entry.is_regular_file() || filename.size() <= suffix.size() || filename.substr(filename.size() - suffix.size()) != suffix)
                continue;
            const Path otherPath = second / entry.path().filename();
            ++compared;
            Image firstImage;
            Image secondImage;
            String error;
            if (!LoadBmp(entry.path(), firstImage, error) || !LoadBmp(otherPath, secondImage, error))
            {
                ++failed;
                std::cout << "[  FAILED  ] " << filename << ": " << error << '\n';
                continue;
            }
            const String testName = filename.substr(0u, filename.size() - suffix.size());
            const auto test = std::find_if(cases.begin(), cases.end(),
                                           [&](const TestCase& candidate) { return candidate.Name == testName; });
            const Tolerance& tolerance = test != cases.end() ? test->AllowedDifference : defaultTolerance;
            const Comparison comparison = Compare(firstImage, secondImage, tolerance);
            if (!comparison.Passed)
            {
                ++failed;
                String saveError;
                SaveBmp(artifacts / filename, BuildDiff(firstImage, secondImage), saveError);
            }
            std::cout << (comparison.Passed ? "[       OK ] " : "[  FAILED  ] ") << filename;
            if (!comparison.Message.empty())
                std::cout << ": " << comparison.Message;
            std::cout << '\n';
        }
        std::cout << "Compared " << compared << " backend captures: " << failed << " failed\n";
        return compared > 0u && failed == 0u ? 0 : 1;
    }
} // namespace Crowny::RenderTests
