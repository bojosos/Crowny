#include "RenderTestRunner.h"

#include "DecalShowcase.h"
#include "OslTextureTests.h"
#include "ProceduralMaterialTests.h"
#include "RenderTestImage.h"
#include "Sprite2DRenderTests.h"

#include "Crowny/Animation/Skeleton.h"
#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Import/ImageLoader.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Import/TextureImporter.h"
#include "Crowny/RenderAPI/GenericGpuBuffer.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/BasisTextureCodec.h"
#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/DecalGpuGrid.h"
#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/GpuDecalWorld.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/MeshProcessing.h"
#include "Crowny/Renderer/PrimitiveMeshLibrary.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneCamera.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"
#include "Crowny/Serialization/MaterialSerializer.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "Crowny/Utils/PixelUtils.h"
#include "Crowny/Utils/ShaderCompiler.h"
#include "Editor/MaterialEditing.h"
#include "Editor/PreviewRenderer.h"
#include "Panels/MaterialParameterPresentation.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <glm/gtc/matrix_transform.hpp>

namespace Crowny::RenderTests
{
    namespace
    {
        constexpr uint32_t TEST_WIDTH = 64u;
        constexpr uint32_t TEST_HEIGHT = 64u;
        Scope<DecalShowcaseWriter> s_DecalShowcase;
        String s_CurrentTestName;

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

        Ref<Texture> CreateRenderTexture(uint32_t width, uint32_t height, TextureFormat format, StringView debugName, bool loadStore = false)
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

        Ref<RenderTexture> CreateTarget(const Vector<Ref<Texture>>& colors, uint32_t width, uint32_t height, const Ref<Texture>& depth = nullptr)
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

        enum class CaptureOrigin
        {
            Backend,
            SceneViewport
        };

        bool Capture(const Ref<Texture>& texture, Image& image, String& error, CaptureOrigin origin = CaptureOrigin::Backend)
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
            // Scene viewports display their texture with UV (0, 1) at the top on
            // both backends. Match that presentation when capturing scene renders.
            const bool flipVertically = origin == CaptureOrigin::SceneViewport || RenderAPI::GetAPI() == RenderAPI::API::OpenGL;
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

        bool RenderDepthOutputCase(bool writeVelocity, bool writeObjectID, Image& image, uint32_t destinationX, uint32_t destinationY, String& error)
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
            const ShaderCompileResult compileResult = ShaderCompiler::CompileWithDiagnostics(caseName, source, ShaderLanguage::VKSL, defines);
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

            const auto encodeUnit = [](float value) { return static_cast<uint8_t>(std::round(glm::clamp(value, 0.0f, 1.0f) * 255.0f)); };
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
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(1.0f, 0.125f, 0.25f, 1.0f), 1.0f, 0u, 1u << 0u);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0.125f, 0.875f, 0.5f, 1.0f), 1.0f, 0u, 1u << 1u);
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

        bool RenderStorageBufferBindings(Image& image, String& error)
        {
            // Reuse slot 3 in three sets and share one buffer across stages with
            // different block/instance names. Numeric flattening exceeds small
            // GL binding limits even though only three buffers are needed.
            const String source = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450
layout(set = 2, binding = 3, std430) readonly buffer VertexPaint { vec4 Value; } vertexPaint;
layout(location = 0) out vec3 tint;
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    tint = vertexPaint.Value.rgb;
}
#type fragment
#version 450
layout(set = 2, binding = 3, std430) readonly buffer FragmentPaint { vec4 Color; } fragmentPaint;
layout(set = 0, binding = 3, std430) readonly buffer GreenPaint { vec4 Value; } greenPaint;
layout(set = 1, binding = 3, std430) readonly buffer BluePaint { vec4 Value; } bluePaint;
layout(location = 0) in vec3 tint;
layout(location = 0) out vec4 color;
void main() {
    color = vec4(tint + greenPaint.Value.rgb + bluePaint.Value.rgb, fragmentPaint.Color.a);
}
)";
            Ref<ShaderRenderPass> pass;
            if (!CompileGraphicsPass("StorageBufferBindings", source, pass, error))
                return false;
            const auto params = UniformParams::Create(pass->GetGraphicsPipeline());
            const Array<glm::vec4, 3> values{ glm::vec4(0, 0.5f, 0, 0), glm::vec4(0, 0, 0.75f, 0), glm::vec4(0.25f, 0, 0, 1) };
            for (uint32_t set = 0; set < values.size(); ++set)
            {
                const auto buffer =
                  GenericGpuBuffer::Create({ 1, sizeof(glm::vec4), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_STATIC_DRAW });
                if (!buffer || !params)
                {
                    error = "Could not allocate storage-buffer binding test resources";
                    return false;
                }
                buffer->WriteData(0, sizeof(glm::vec4), &values[set]);
                params->SetBuffer(set, 3, buffer);
            }
            const auto color = CreateColorTexture(TEST_WIDTH, TEST_HEIGHT, "StorageBufferBindings");
            const auto target = CreateTarget({ color }, TEST_WIDTH, TEST_HEIGHT);
            RenderAPI& api = RenderAPI::Get();
            api.SetRenderTarget(target);
            api.SetViewport(0, 0, 1, 1);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0));
            api.SetGraphicsPipeline(pass->GetGraphicsPipeline());
            api.SetVertexLayout(CreateRef<BufferLayout>());
            api.SetUniforms(params);
            api.SetDrawMode(DrawMode::TRIANGLE_LIST);
            api.Draw(0, 3, 1);
            api.SubmitCommandBuffer(nullptr);
            if (!Capture(color, image, error))
                return false;
            const uint8_t* pixel = image.Pixel(TEST_WIDTH / 2, TEST_HEIGHT / 2);
            if (std::abs(int(pixel[0]) - 64) > 1 || std::abs(int(pixel[1]) - 128) > 1 || std::abs(int(pixel[2]) - 191) > 1 || pixel[3] != 255)
            {
                error = "Storage-buffer bindings aliased descriptor sets or disagreed across shader stages";
                return false;
            }
            return true;
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
            textureDesc.MipLevels = 3u; // Three levels after the base level: 8, 4, 2, 1.
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
            samplerDesc.AddressMode = { TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE };
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
            ShaderVariation variation;
            variation.Set("CW_TONEMAP_OBJECT_ID", true);
            if (!toneMap.Initialize(assetManager->Load<Shader>("Resources/Shaders/ToneMap.asset"), variation))
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

            const Ref<Texture> accumulation = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/OitAccumulation");
            const Ref<Texture> revealage = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::R32F, "RenderTests/OitRevealage");
            const Ref<Texture> hdrColor = CreateRenderTexture(TEST_WIDTH, TEST_HEIGHT, TextureFormat::RGBA16F, "RenderTests/OitHdrColor", true);
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
            const auto silhouetteShader = AssetManager::Get().Load<Shader>("Resources/Shaders/ToonSilhouette.asset");
            if (!silhouetteShader || silhouetteShader->GetTechniques().empty() ||
                silhouetteShader->GetTechniques().front()->GetRenderPasses().empty())
            {
                error = "The silhouette shader has no render pass";
                return false;
            }
            const auto& silhouettePass = silhouetteShader->GetTechniques().front()->GetRenderPasses().front()->GetPassDesc();
            if (!silhouettePass.RasterizationState || silhouettePass.RasterizationState->CullMode != CullingMode::CULL_CLOCKWISE ||
                !silhouettePass.DepthStencilState || !silhouettePass.DepthStencilState->EnableDepthRead ||
                silhouettePass.DepthStencilState->EnableDepthWrite ||
                silhouettePass.DepthStencilState->DepthCompareFunction != CompareFunction::GREATER_EQUAL)
            {
                error = "The silhouette shader must cull front faces and read reverse depth without writing it";
                return false;
            }
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

            MaterialSerializer serializer(material);
            const String legacyMaterial =
              "Version: 2\nParameters:\n  - Name: thickness\n    Type: " + std::to_string(static_cast<uint32_t>(ShaderDataType::Float)) +
              "\n    Value: 0.625\n";
            if (!serializer.DeserializeFromString(legacyMaterial) ||
                std::abs(material->GetDataParam<float>("toonSilhouetteWidth") - 0.625f) > 0.0001f)
            {
                error = "Legacy toon thickness did not migrate to silhouette width";
                return false;
            }
            if (!material->ApplyToonPreset(ToonMaterialPreset::Hatched))
            {
                error = "The built-in toon material rejected a compatible preset";
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

            Ref<MeshData> sphereData = MeshFactory::CreateSphereData(0.75f, 32u, 16u);
            if (!sphereData)
            {
                error = "Could not create the toon test sphere";
                return false;
            }
            if (RenderAPI::GetAPI() == RenderAPI::API::Vulkan)
            {
                const BufferLayout colorlessLayout = { { ShaderDataType::Float3, VertexAttribute::Position },
                                                       { ShaderDataType::Float3, VertexAttribute::Normal },
                                                       { ShaderDataType::Float3, VertexAttribute::Tangent },
                                                       { ShaderDataType::Float3, VertexAttribute::Bitangent },
                                                       { ShaderDataType::Float2, VertexAttribute::TexCoord0 } };
                const Ref<MeshData> colorlessSphere =
                  MeshData::Create(sphereData->GetVertexCount(), sphereData->GetIndexCount(), colorlessLayout, sphereData->GetIndexType());
                colorlessSphere->SetPositions(sphereData->GetPositions());
                colorlessSphere->SetNormals(sphereData->GetNormals());
                colorlessSphere->SetTangents(sphereData->GetTangents());
                colorlessSphere->SetBitangents(sphereData->GetBitangents());
                colorlessSphere->SetUVs(0u, sphereData->GetUVs());
                colorlessSphere->SetIndices(sphereData->GetIndices());
                sphereData = colorlessSphere;
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
            const Ref<Texture> depth = CreateRenderTexture(toonTestSize, toonTestSize, TextureFormat::DEPTH32F, "RenderTests/ToonSilhouetteDepth");
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
            const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

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

        bool CheckTextureImportCache(String& error)
        {
            const Path root = Application::Get().GetInternalDirectory() / "TextureCacheTest" /
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            const Path sourcePath = root / "source.bmp";
            const Path cacheRoot = root / "cache";
            Image source(8, 8);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x)
                {
                    auto* pixel = source.Pixel(x, y);
                    pixel[0] = x * 32;
                    pixel[1] = y * 32;
                    pixel[2] = 128;
                    pixel[3] = x < 4 ? 0 : 255;
                }
            fs::create_directories(root);
            if (!SaveBmp(sourcePath, source, error))
                return false;
            TextureImporter importer(cacheRoot);
            auto options = CreateRef<TextureImportOptions>();
            options->DiskFormat = TextureDiskFormat::UASTC;
            const auto import = [&] { return StaticRefCast<Texture>(importer.Import(sourcePath, options)); };
            const auto cold = import();
            if (!cold || !fs::is_directory(cacheRoot) || fs::is_empty(cacheRoot))
            {
                error = "Texture import did not publish a reusable cook";
                return false;
            }
            Path cacheFile;
            for (const auto& entry : fs::directory_iterator(cacheRoot))
                if (entry.path().extension() == ".texturecook")
                    cacheFile = entry.path();
            if (cacheFile.empty())
            {
                error = "Texture import did not retain the shared pixel cook";
                return false;
            }
            const Path sourceCacheFile = fs::directory_iterator(cacheRoot / "Sources-v1")->path();
            const auto oldTime = fs::file_time_type::clock::now() - std::chrono::hours(24);
            fs::last_write_time(cacheFile, oldTime);
            fs::last_write_time(sourceCacheFile, oldTime);
            const auto retainedTime = fs::last_write_time(cacheFile);
            const auto warm = import();
            if (!warm || cold->GetEncodedSourceData() != warm->GetEncodedSourceData() || fs::last_write_time(cacheFile) != retainedTime ||
                fs::last_write_time(sourceCacheFile) != retainedTime || warm->GetDesc().Format != cold->GetDesc().Format)
            {
                error = "Unchanged texture import recompressed or changed its cached payload";
                return false;
            }
            // Corruption must fall back to a real cook, never reach the GPU decoder.
            {
                std::ofstream corrupt(cacheFile, std::ios::binary | std::ios::trunc);
                corrupt << "broken";
                std::ofstream corruptSource(sourceCacheFile, std::ios::binary | std::ios::trunc);
                corruptSource << "broken";
            }
            const auto recovered = import();
            if (!recovered || recovered->GetEncodedSourceData().empty() || fs::file_size(cacheFile) <= 64)
            {
                error = "A corrupt texture cook was not rebuilt";
                return false;
            }
            options->GenerateMips = false;
            const auto noMips = import();
            if (!noMips || noMips->GetDesc().MipLevels != 0)
            {
                error = "Texture cook reused incompatible mip settings";
                return false;
            }
            options->SRGB = false;
            const auto linear = import();
            if (!linear || linear->GetEncodedSourceData() == noMips->GetEncodedSourceData())
            {
                error = "Texture cook ignored a color-space change";
                return false;
            }
            const auto previousCooks = std::distance(fs::directory_iterator(cacheRoot), fs::directory_iterator{});
            options->UASTCEffort = 1;
            const auto fast = import();
            if (!fast || std::distance(fs::directory_iterator(cacheRoot), fs::directory_iterator{}) <= previousCooks)
            {
                error = "Texture cook reused an incompatible compression effort";
                return false;
            }
            options->UASTCEffort = 2;
            const auto originalEffort = import();
            if (!originalEffort || originalEffort->GetEncodedSourceData() != linear->GetEncodedSourceData())
            {
                error = "Texture cook did not retain the previous compression effort";
                return false;
            }
            source.Pixel(0, 0)[0] = 255;
            source.Pixel(0, 0)[3] = 255;
            if (!SaveBmp(sourcePath, source, error))
                return false;
            const auto edited = import();
            if (!edited || edited->GetEncodedSourceData() == linear->GetEncodedSourceData())
            {
                error = "Texture cook ignored edited source pixels";
                return false;
            }
            Vector<std::pair<Ref<Asset>, Path>> repeatedTextures;
            for (uint32_t index = 0; index < 33; ++index)
                repeatedTextures.emplace_back(cold, root / "published" / (std::to_string(index) + ".asset"));
            if (!AssetManager::Get().SaveBatch(repeatedTextures))
            {
                error = "A batch containing repeated texture references failed to publish";
                return false;
            }
            const Path unchangedPath = repeatedTextures[1].second;
            fs::last_write_time(unchangedPath, oldTime);
            const auto unchangedTime = fs::last_write_time(unchangedPath);
            if (!AssetManager::Get().SaveBatch(repeatedTextures) || fs::last_write_time(unchangedPath) != unchangedTime)
            {
                error = "An unchanged texture asset was rewritten on reimport";
                return false;
            }
            {
                std::fstream corrupt(unchangedPath, std::ios::in | std::ios::out | std::ios::binary);
                corrupt.seekp(-1, std::ios::end);
                const char damaged = static_cast<char>(cold->GetEncodedSourceData().back() ^ 0xffu);
                corrupt.write(&damaged, 1);
            }
            if (!AssetManager::Get().SaveBatch(repeatedTextures))
            {
                error = "Reimport failed to repair a damaged texture asset";
                return false;
            }
            const auto published = AssetManager::Get().Load<Texture>(repeatedTextures.back().second, false, true);
            if (!published || published->GetEncodedSourceData() != cold->GetEncodedSourceData() ||
                !AssetManager::Get().Save(edited, repeatedTextures.front().second))
            {
                error = "A shared texture output did not round trip or accept replacement";
                return false;
            }
            const auto retained = AssetManager::Get().Load<Texture>(repeatedTextures[1].second, false, true);
            const auto replaced = AssetManager::Get().Load<Texture>(repeatedTextures.front().second, false, true);
            if (!retained || !replaced || retained->GetEncodedSourceData() != cold->GetEncodedSourceData() ||
                replaced->GetEncodedSourceData() != edited->GetEncodedSourceData())
            {
                error = "Replacing one shared texture cache path modified another asset's content";
                return false;
            }
            for (const auto& texture : { cold, warm, recovered, noMips, linear, edited })
                texture->Init();
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            fs::remove_all(root);
            return true;
        }

        bool CheckModelTextureBatch(String& error)
        {
            const Path root = fs::temp_directory_path() / ("crowny-model-batch-" + UuidGenerator::Generate().ToString());
            struct Cleanup
            {
                Path Root;
                Path Internal;
                ~Cleanup()
                {
                    Application::Get().SetInternalDirectory(Internal);
                    std::error_code ignored;
                    fs::remove_all(Root, ignored);
                }
            } cleanup{ root, Application::Get().GetInternalDirectory() };
            Application::Get().SetInternalDirectory({}); // Every pass must cook from source.
            const Path model = root / "batch.obj";
            String obj = "mtllib batch.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\n";
            String mtl;
            fs::create_directories(root);
            for (uint32_t index = 0; index < 9; ++index)
            {
                const String name = "material" + std::to_string(index);
                obj += "o " + name + "\nusemtl " + name + "\nf 1/1/1 2/2/1 3/3/1\n";
                // The last material shares the first texture. Other sources remain distinct.
                mtl += "newmtl " + name + "\nKd 1 1 1\nd 1\nmap_Kd color" + std::to_string(index % 8) + ".bmp\n";
                if (index == 8)
                    continue;
                Image pixels(8, 8);
                for (uint32_t y = 0; y < 8; ++y)
                    for (uint32_t x = 0; x < 8; ++x)
                    {
                        auto* pixel = pixels.Pixel(x, y);
                        pixel[0] = static_cast<uint8_t>(30 + (index == 7 ? 3 : index) * 25);
                        pixel[1] = 100;
                        pixel[2] = 200;
                        pixel[3] = index % 2 == 0 && x < 4 ? 0 : 255;
                    }
                if (!SaveBmp(root / ("color" + std::to_string(index) + ".bmp"), pixels, error))
                    return false;
            }
            if (!FileSystem::WriteTextFileAtomic(model, obj) || !FileSystem::WriteTextFileAtomic(root / "batch.mtl", mtl))
                return false;
            auto options = CreateRef<MeshImportOptions>();
            options->GeneratePrefab = false;
            options->GenerateMeshlets = false;
            options->GenerateLods = false;
            options->GenerateCollision = false;
            options->ImportAnimations = false;
            Vector<Vector<uint8_t>> previous;
            for (uint32_t pass = 0; pass < 2; ++pass)
            {
                const auto assets = Importer::Get().ImportAll(model, options);
                if (assets.size() != 19 || assets.front()->GetAssetType() != AssetType::Mesh)
                {
                    error = "Parallel model texture import changed the dependent asset sequence";
                    return false;
                }
                for (uint32_t index = 0; index < 9; ++index)
                {
                    const auto& textureAsset = assets[1 + index * 2];
                    const auto& materialAsset = assets[2 + index * 2];
                    if (textureAsset->GetAssetType() != AssetType::Texture || materialAsset->GetAssetType() != AssetType::Material)
                        return false;
                    const auto texture = StaticRefCast<Texture>(textureAsset);
                    const auto material = StaticRefCast<Material>(materialAsset);
                    const auto& bytes = texture->GetEncodedSourceData();
                    if (bytes.empty() || texture->GetName() != "color" + std::to_string(index % 8) + ".bmp" ||
                        material->GetName() != "material" + std::to_string(index) ||
                        material->GetAlphaMode() != (index % 2 == 0 ? AlphaMode::Mask : AlphaMode::Opaque) || (pass && previous[index] != bytes))
                    {
                        error = "Parallel model texture import mixed names, alpha modes or cooked contents";
                        return false;
                    }
                    if (!pass)
                        previous.push_back(bytes);
                }
                if (assets[1] != assets[17])
                {
                    error = "Parallel model import lost shared texture identity";
                    return false;
                }
                if (assets[7] == assets[15] ||
                    StaticRefCast<Texture>(assets[7])->GetEncodedSourceData() != StaticRefCast<Texture>(assets[15])->GetEncodedSourceData())
                {
                    error = "Identical texture files must share cooked contents while retaining separate asset identities";
                    return false;
                }
            }
            return true;
        }

        bool CheckModelAlphaCache(String& error)
        {
            const Path originalInternal = Application::Get().GetInternalDirectory();
            const Path root = fs::temp_directory_path() / ("crowny-model-alpha-" + UuidGenerator::Generate().ToString());
            struct Cleanup
            {
                Path Root;
                Path Internal;
                ~Cleanup()
                {
                    Application::Get().SetInternalDirectory(Internal);
                    std::error_code ignored;
                    fs::remove_all(Root, ignored);
                }
            } cleanup{ root, originalInternal };
            Application::Get().SetInternalDirectory(root / "Internal/Assets");
            const Path model = root / "triangle.obj";
            if (!FileSystem::WriteTextFileAtomic(
                  model, "mtllib triangle.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nusemtl test\nf 1/1/1 2/2/1 3/3/1\n") ||
                !FileSystem::WriteTextFileAtomic(root / "triangle.mtl", "newmtl test\nKd 1 1 1\nd 1\nmap_Kd coverage.bmp\n"))
                return false;
            Image pixels(8, 8);
            for (uint32_t y = 0; y < 8; ++y)
                for (uint32_t x = 0; x < 8; ++x)
                {
                    auto* pixel = pixels.Pixel(x, y);
                    pixel[0] = 200;
                    pixel[1] = 120;
                    pixel[2] = 80;
                    pixel[3] = x < 4 ? 0 : 255;
                }
            if (!SaveBmp(root / "coverage.bmp", pixels, error))
                return false;
            auto options = CreateRef<MeshImportOptions>();
            options->ImportMaterials = true;
            options->GenerateMeshlets = false;
            options->GenerateLods = false;
            options->GenerateCollision = false;
            options->ImportAnimations = false;
            const auto check = [&](AlphaMode expected) {
                const auto assets = Importer::Get().ImportAllDeferred(model, options);
                for (const auto& asset : assets)
                    if (asset->GetAssetType() == AssetType::Material)
                        return StaticRefCast<Material>(asset)->GetAlphaMode() == expected;
                return false;
            };
            if (!check(AlphaMode::Mask))
            {
                error = "Varying model albedo coverage was not imported as a cutout";
                return false;
            }
            const Path cache = root / "Internal/ImportCache/Alpha-v1";
            if (!fs::is_directory(cache) || fs::is_empty(cache))
            {
                error = "Model alpha analysis did not create a reusable result";
                return false;
            }
            const Path entry = fs::directory_iterator(cache)->path();
            fs::last_write_time(entry, fs::file_time_type::clock::now() - std::chrono::hours(24));
            const auto retainedTime = fs::last_write_time(entry);
            if (!check(AlphaMode::Mask) || fs::last_write_time(entry) != retainedTime)
            {
                error = "Unchanged model alpha coverage was recomputed or changed";
                return false;
            }
            FileSystem::WriteTextFileAtomic(entry, "broken");
            if (!check(AlphaMode::Mask) || fs::file_size(entry) != 65)
            {
                error = "Corrupt model alpha coverage was not recovered";
                return false;
            }
            for (uint8_t alpha : { uint8_t{ 255 }, uint8_t{ 0 } })
            {
                for (uint32_t y = 0; y < 8; ++y)
                    for (uint32_t x = 0; x < 8; ++x)
                        pixels.Pixel(x, y)[3] = alpha;
                if (!SaveBmp(root / "coverage.bmp", pixels, error) || !check(AlphaMode::Opaque))
                {
                    error = "Model alpha analysis ignored edited pixels or treated constant alpha as cutout coverage";
                    return false;
                }
            }
            return true;
        }

        bool CheckEnvironmentCook(String& error)
        {
            if (!CheckTextureImportCache(error) || !CheckModelAlphaCache(error) || !CheckModelTextureBatch(error))
                return false;
            const auto brdf = AssetManager::Get().Load<Texture>("Resources/Textures/Brdf.asset");
            if (!brdf)
            {
                error = "The built-in BRDF lookup is missing";
                return false;
            }
            const Ref<PixelData> lookup = brdf->AllocatePixelData(0, 0);
            brdf->ReadData(*lookup);
            for (uint32_t y = 0; y < brdf->GetHeight(); y++)
            {
                for (uint32_t x = 0; x < brdf->GetWidth(); x++)
                {
                    const glm::vec4 value = lookup->GetColorAt(x, y);
                    if (!std::isfinite(value.r) || !std::isfinite(value.g) || value.r < 0.0f || value.g < 0.0f || value.r > 1.1f || value.g > 1.1f)
                    {
                        error = "The built-in BRDF lookup contains invalid reflection weights";
                        return false;
                    }
                }
            }
            if (lookup->GetColorAt(brdf->GetWidth() - 1, brdf->GetHeight() / 2).r < 0.1f)
            {
                error = "The built-in BRDF lookup lost its integrated reflection weights";
                return false;
            }
            const Path source = fs::temp_directory_path() / ("crowny-environment-" + UuidGenerator::Generate().ToString() + ".hdr");
            {
                std::ofstream output(source, std::ios::binary);
                output << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 4 +X 4\n";
                for (uint32_t index = 0; index < 16; index++)
                {
                    const unsigned char pixel[] = { static_cast<unsigned char>(32 + (index % 4) * 40),
                                                    static_cast<unsigned char>(32 + (index / 4) * 40), 64,
                                                    static_cast<unsigned char>(index == 5 ? 144 : 129) };
                    output.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
                }
            }
            EnvironmentMap::Settings settings;
            settings.CubemapResolution = 8;
            settings.IrradianceResolution = 4;
            settings.PrefilteredResolution = 8;
            settings.PrefilterSamples = 8;
            const Ref<EnvironmentMap> environment = CreateRef<EnvironmentMap>(source, settings);
            fs::remove(source);
            const Path cooked = fs::temp_directory_path() / ("crowny-environment-" + UuidGenerator::Generate().ToString() + ".asset");
            if (!environment->IsValid() || !AssetManager::Get().Save(environment, cooked))
            {
                error = "HDR environment generation or cooking failed";
                return false;
            }
            const auto loaded = AssetManager::Get().Load<EnvironmentMap>(cooked);
            fs::remove(cooked);
            if (!loaded || !loaded->IsValid())
            {
                error = "Cooked HDR environment did not reload";
                return false;
            }
            Ref<ShaderRenderPass> samplePass;
            if (!CompileGraphicsPass("EnvironmentSample", R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450
void main() {
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450
layout(set = 0, binding = 9) uniform samplerCube environment;
layout(location = 0) out vec4 color;
void main() {
    vec3 directions[6] = vec3[6](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));
    color = textureLod(environment, directions[int(gl_FragCoord.x)], floor(gl_FragCoord.y));
}
)",
                                     samplePass, error))
                return false;
            const Ref<Texture> sampled = CreateRenderTexture(6, 4, TextureFormat::RGBA32F, "Environment sampling");
            const auto sampleTarget = CreateTarget({ sampled }, 6, 4);
            const auto sampleUniforms = UniformParams::Create(samplePass->GetGraphicsPipeline());
            sampleUniforms->SetTexture(0, 9, loaded->GetPrefilteredMap());
            RenderAPI::Get().SetRenderTarget(sampleTarget);
            RenderAPI::Get().SetViewport(0, 0, 1, 1);
            RenderAPI::Get().SetGraphicsPipeline(samplePass->GetGraphicsPipeline());
            RenderAPI::Get().SetVertexLayout(CreateRef<BufferLayout>());
            RenderAPI::Get().SetUniforms(sampleUniforms);
            RenderAPI::Get().Draw(0, 3, 1);
            RenderAPI::Get().SetRenderTarget(nullptr);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            const auto sampledPixels = sampled->AllocatePixelData(0, 0);
            sampled->ReadData(*sampledPixels);
            for (uint32_t y = 0; y < 4; y++)
                for (uint32_t x = 0; x < 6; x++)
                {
                    const auto color = glm::vec3(sampledPixels->GetColorAt(x, y));
                    const auto cube = loaded->GetPrefilteredMap();
                    const auto face = cube->AllocatePixelData(x, y);
                    cube->ReadData(*face, y, x);
                    const uint32_t extent = std::max(cube->GetWidth() >> y, 1u);
                    const uint32_t low = (extent - 1u) / 2u;
                    const uint32_t high = extent / 2u;
                    const glm::vec3 expected = glm::vec3(face->GetColorAt(low, low) + face->GetColorAt(high, low) + face->GetColorAt(low, high) +
                                                         face->GetColorAt(high, high)) *
                                               0.25f;
                    if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) ||
                        glm::length(color - expected) > std::max(glm::length(expected) * 0.02f, 0.1f))
                    {
                        error = "Cooked environment shader sampling failed at " + std::to_string(x) + ", " + std::to_string(y) + ": " +
                                std::to_string(color.r) + ", " + std::to_string(color.g) + ", " + std::to_string(color.b);
                        return false;
                    }
                }
            const Ref<Texture> cubes[] = { loaded->GetEnvironmentCubemap(), loaded->GetPrefilteredMap(), loaded->GetIrradianceMap() };
            for (uint32_t map = 0; map < 3; map++)
            {
                const Ref<Texture>& cube = cubes[map];
                for (uint32_t mip = 0; mip <= cube->GetDesc().MipLevels; mip++)
                {
                    for (uint32_t face = 0; face < 6; face++)
                    {
                        const Ref<PixelData> pixels = cube->AllocatePixelData(face, mip);
                        cube->ReadData(*pixels, mip, face);
                        const glm::vec4 color =
                          pixels->GetColorAt(std::max(1u, cube->GetWidth() >> mip) / 2, std::max(1u, cube->GetHeight() >> mip) / 2);
                        if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) || color.r < 0.1f || color.r > 65504.0f ||
                            color.g < 0.1f || color.g > 65504.0f || color.b < 0.4f || color.b > 65504.0f)
                        {
                            error = "Cooked environment map " + std::to_string(map) + " mip " + std::to_string(mip) + " face " +
                                    std::to_string(face) + " lost its HDR color: " + std::to_string(color.r) + ", " + std::to_string(color.g) + ", " +
                                    std::to_string(color.b);
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        bool CheckModelMaterialImport(String& error)
        {
            const Path source = fs::temp_directory_path() / ("crowny-material-" + UuidGenerator::Generate().ToString() + ".gltf");
            {
                std::ofstream output(source);
                output
                  << R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":42,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"}],
                    "bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
                    "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
                    "materials":[{"name":"Glass","alphaMode":"BLEND","emissiveFactor":[0.2,0.4,0.6],"pbrMetallicRoughness":{"baseColorFactor":[1,0.8,0.6,0.4],"metallicFactor":0.2,"roughnessFactor":0.8}}],
                    "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"material":0}]}],
                    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
            }
            const Ref<MeshImportOptions> options = CreateRef<MeshImportOptions>();
            options->GeneratePrefab = true;
            options->GenerateMeshlets = false;
            options->GenerateLods = false;
            options->GenerateCollision = false;
            MeshImporter importer;
            const Vector<Ref<Asset>> assets = importer.ImportAll(source, options);
            fs::remove(source);
            Ref<Material> material;
            Ref<Prefab> prefab;
            for (const Ref<Asset>& asset : assets)
            {
                if (asset->GetAssetType() == AssetType::Material)
                    material = StaticRefCast<Material>(asset);
                if (asset->GetAssetType() == AssetType::Prefab)
                    prefab = StaticRefCast<Prefab>(asset);
            }
            if (!material || !prefab || assets.empty() || assets.front()->GetAssetType() != AssetType::Mesh)
            {
                error = "Model import did not return a primary mesh, material and prefab";
                return false;
            }
            const glm::vec4 color = material->GetDataParam<glm::vec4>("albedo");
            const glm::vec4 emission = material->GetDataParam<glm::vec4>("emissive");
            if (material->GetAlphaMode() != AlphaMode::WeightedOIT || std::abs(color.a - 0.4f) > 0.001f || std::abs(emission.z - 0.6f) > 0.001f ||
                std::abs(material->GetDataParam<float>("roughness") - 0.8f) > 0.001f)
            {
                error = "Imported material lost source opacity, emission or roughness";
                return false;
            }
            const UUID meshId = UuidGenerator::Generate();
            const UUID materialId = UuidGenerator::Generate();
            prefab->OnDependentAssigned(assets.front(), meshId);
            prefab->OnDependentAssigned(material, materialId);
            const auto& renderer = prefab->GetRootEntity().GetComponent<MeshRendererComponent>();
            if (renderer.MeshHandle.GetUUID() != meshId || renderer.Materials.size() != 1 || renderer.Materials.front().GetUUID() != materialId)
            {
                error = "Imported prefab did not retain assigned mesh and material UUIDs";
                return false;
            }
            const Path cooked = fs::temp_directory_path() / ("crowny-prefab-" + UuidGenerator::Generate().ToString() + ".asset");
            if (!AssetManager::Get().Save(prefab, cooked))
            {
                error = "Imported prefab could not be cooked";
                return false;
            }
            const AssetHandle<Prefab> loaded = AssetManager::Get().Load<Prefab>(cooked);
            fs::remove(cooked);
            if (!loaded || loaded->GetRootEntity().GetComponent<MeshRendererComponent>().MeshHandle.GetUUID() != meshId ||
                loaded->GetRootEntity().GetComponent<MeshRendererComponent>().Materials.front().GetUUID() != materialId)
            {
                error = "Cooked model prefab lost its asset references";
                return false;
            }
            return true;
        }

        bool CheckMaterialEditing(String& error)
        {
            const Ref<Material> material = Material::CreateDefault();
            if (!material || !material->HasBinding("roughness") || !material->HasBinding("albedo") ||
                material->GetShader().GetUUID() != BuiltInShaderCatalog::MakeStableUuid(PBRIBL_SHADER_PATH))
            {
                error = "New material did not expose the built-in PBR shader and parameters";
                return false;
            }
            const AssetHandle<Texture> texture = static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(Texture::WHITE));
            const auto previewMaterial = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(material));
            PreviewMaterialRenderer preview(previewMaterial);
            if (!preview.Setup(64, 64))
            {
                error = "Material preview could not create its color and depth target";
                return false;
            }
            Image initialPreview, editedPreview;
            if (!Capture(preview.RenderPreview(), initialPreview, error))
                return false;
            material->SetColor("albedo", glm::vec4(0.05f, 0.8f, 0.1f, 1.0f));
            if (!Capture(preview.RenderPreview(), editedPreview, error))
                return false;
            const Path previewCapture = Path("artifacts") / ("material-preview-" + BackendName(RenderAPI::GetAPI()) + ".bmp");
            fs::create_directories(previewCapture.parent_path());
            if (!SaveBmp(previewCapture, editedPreview, error))
                return false;
            uint32_t previewChanges = 0;
            for (uint32_t y = 0; y < initialPreview.Height; ++y)
                for (uint32_t x = 0; x < initialPreview.Width; ++x)
                    if (std::memcmp(initialPreview.Pixel(x, y), editedPreview.Pixel(x, y), 3) != 0)
                        ++previewChanges;
            if (previewChanges < 100)
            {
                error = "Live material preview did not visibly update after an albedo edit";
                return false;
            }
            material->SetTexture("albedoMap", texture);
            material->SetFloat("roughness", 0.73f);
            material->SetColor("albedo", glm::vec4(0.2f, 0.4f, 0.6f, 1.0f));
            const String pbrYaml = MaterialSerializer(material).SerializeToString();
            const Ref<Material> restored = Material::CreateDefault();
            if (!MaterialSerializer(restored).DeserializeFromString(pbrYaml) ||
                std::abs(restored->GetDataParam<float>("roughness") - 0.73f) > 0.001f ||
                restored->GetTextureHandle("albedoMap").GetUUID() != texture.GetUUID())
            {
                error = "Material parameter or texture edit did not survive YAML reload";
                return false;
            }
            restored->SetTexture("normalMap", texture);
            ShaderParameterDesc roughnessParameter;
            roughnessParameter.Identifier = "roughness";
            roughnessParameter.Type = ShaderParamType::Float;
            ResetMaterialParameter(*restored, *Material::CreateDefault(), roughnessParameter);
            if (std::abs(restored->GetDataParam<float>("roughness") - 0.5f) > 0.001f ||
                restored->GetTextureHandle("albedoMap").GetUUID() != texture.GetUUID())
            {
                error = "Resetting roughness did not restore its default while preserving the texture edit";
                return false;
            }
            restored->SetTexture("normalMap", AssetHandle<Texture>{});
            const auto normal = restored->GetTextureDescriptors().find("normalMap");
            if (normal == restored->GetTextureDescriptors().end() || restored->GetTextureHandle("normalMap").HasUUID() ||
                restored->GetTexture(normal->second.Set, normal->second.Slot) != Texture::NORMAL ||
                ChangeMaterialShader(*restored, restored->GetShader()))
            {
                error = "Clearing a normal map did not restore its default, or reselecting the current shader reset the material";
                return false;
            }
            const AssetHandle<Shader> toon = AssetManager::Get().Load<Shader>(TOON_SHADER_PATH);
            const Ref<Material> toonDefaults = Material::Create(toon);
            const auto toonTexture = toonDefaults->GetTextureDescriptors().find("albedoMap");
            if (toonTexture == toonDefaults->GetTextureDescriptors().end() ||
                toonDefaults->GetAnnotations().find("bands") == toonDefaults->GetAnnotations().end() ||
                std::abs(toonDefaults->GetDataParam<float>("bands") - 4.0f) > 0.001f ||
                toonDefaults->GetTexture(toonTexture->second.Set, toonTexture->second.Slot) != Texture::WHITE)
            {
                error = "Toon's second pass did not expose its annotations, texture, and parameter defaults";
                return false;
            }
            const bool shaderChanged = ChangeMaterialShader(*material, toon);
            if (!shaderChanged || !material->HasBinding("bands") || material->HasBinding("roughness") ||
                material->GetTextureHandle("albedoMap").GetUUID() != texture.GetUUID())
            {
                error = "PBR to Toon change failed: changed=" + std::to_string(shaderChanged) +
                        ", bands=" + std::to_string(material->HasBinding("bands")) +
                        ", roughness=" + std::to_string(material->HasBinding("roughness")) +
                        ", texture=" + std::to_string(material->GetTextureHandle("albedoMap").GetUUID() == texture.GetUUID()) + ", shader name='" +
                        (toon ? toon->GetName() : "unloaded") + "'";
                return false;
            }
            material->SetFloat("bands", 5.0f);
            // Use a saturated tint so tone mapping and highlights cannot hide a stale material binding.
            material->SetColor("tint", glm::vec4(0.8f, 0.02f, 0.01f, 1.0f));
            Image toonPreview;
            if (!Capture(preview.RenderPreview(), toonPreview, error))
                return false;
            if (!SaveBmp(Path("artifacts") / ("material-preview-toon-" + BackendName(RenderAPI::GetAPI()) + ".bmp"), toonPreview, error))
                return false;
            uint32_t tintedPixels = 0;
            for (uint32_t y = 0; y < toonPreview.Height; ++y)
                for (uint32_t x = 0; x < toonPreview.Width; ++x)
                {
                    const uint8_t* pixel = toonPreview.Pixel(x, y);
                    if (pixel[0] > 30 && pixel[0] > pixel[1] * 1.25f && pixel[0] > pixel[2] * 1.25f)
                        ++tintedPixels;
                }
            if (tintedPixels < 100)
            {
                error = "Material preview did not render the edited Toon tint after switching shaders";
                return false;
            }
            for (const char* extension : { ".cwmat", ".mat" })
            {
                const Path path = fs::temp_directory_path() / ("crowny-material-edit-" + UuidGenerator::Generate().ToString() + extension);
                if (!MaterialSerializer(material).Serialize(path))
                {
                    error = "Could not save edited Toon material";
                    return false;
                }
                const Ref<Asset> imported = MaterialImporter().Import(path, nullptr);
                fs::remove(path);
                const Ref<Material> loaded = imported ? StaticRefCast<Material>(imported) : nullptr;
                if (!loaded || loaded->GetShader().GetUUID() != toon.GetUUID() || std::abs(loaded->GetDataParam<float>("bands") - 5.0f) > 0.001f ||
                    loaded->GetTextureHandle("albedoMap").GetUUID() != texture.GetUUID())
                {
                    error = "Edited Toon material did not survive source reimport";
                    return false;
                }
            }
            return true;
        }

        bool CheckGpuMaterialUpdates(String& error)
        {
            if (!RenderAPI::Get().GetCapabilities().HasCapability(CW_LOAD_STORE))
                return true;
            AssetManager& manager = AssetManager::Get();
            const Ref<Material> first = Material::CreateDefault();
            const Ref<Material> second = Material::CreateDefault();
            if (!first || !second)
            {
                error = "Material update check could not create standard materials";
                return false;
            }
            const auto firstHandle = static_asset_cast<Material>(manager.CreateAssetHandle(first));
            const auto secondHandle = static_asset_cast<Material>(manager.CreateAssetHandle(second));
            GpuScene gpuScene;
            const RenderMaterialResourceChange create[] = { { 1, 1, RenderResourceChangeType::CreateOrUpdate, firstHandle },
                                                            { 7, 1, RenderResourceChangeType::CreateOrUpdate, secondHandle } };
            gpuScene.ApplyResources(nullptr, 0, create, 2);
            const Ref<GenericGpuBuffer> buffer = gpuScene.GetMaterialBuffer();
            if (!buffer)
            {
                error = "Material update check has no GPU material buffer";
                return false;
            }
            Vector<GpuMaterialData> before(gpuScene.GetMaterialCount());
            buffer->ReadData(0, static_cast<uint32_t>(before.size() * sizeof(GpuMaterialData)), before.data());
            const uint64_t textureVersion = gpuScene.GetBindlessTextureVersion();
            first->SetFloat("roughness", 0.73f);
            gpuScene.Apply(nullptr, 0, nullptr, 0);
            const RenderMaterialResourceChange update{ 1, 2, RenderResourceChangeType::CreateOrUpdate, firstHandle };
            gpuScene.ApplyResources(nullptr, 0, &update, 1);
            if (gpuScene.GetMaterialBuffer() != buffer || gpuScene.GetStats().UploadedBytes != sizeof(GpuMaterialData) ||
                gpuScene.GetStats().MaterialRanges != 1 || gpuScene.GetBindlessTextureVersion() != textureVersion)
            {
                error = "A scalar material edit rebuilt resources or uploaded more than its own GPU record";
                return false;
            }
            Vector<GpuMaterialData> after(before.size());
            buffer->ReadData(0, static_cast<uint32_t>(after.size() * sizeof(GpuMaterialData)), after.data());
            for (uint32_t index = 0; index < after.size(); index++)
            {
                const GpuMaterialData& expected = index == 1 ? *gpuScene.GetMaterialData(1) : before[index];
                if (std::memcmp(&after[index], &expected, sizeof(expected)) != 0)
                {
                    error = "A partial material upload changed a neighboring record or lost the edited record";
                    return false;
                }
            }
            gpuScene.Apply(nullptr, 0, nullptr, 0);
            gpuScene.ApplyResources(nullptr, 0, &update, 1);
            if (gpuScene.GetStats().UploadedBytes != 0)
            {
                error = "An unchanged material uploaded GPU data";
                return false;
            }
            return true;
        }

        enum class DecalAcceptance
        {
            None,
            Box,
            CylinderPartial,
            CylinderFull,
            Coating,
            Toon,
            ToonCoating,
            Correction,
            Wetness,
            NormalDetail,
            Unlit,
            CameraInside,
            Overlap,
            Benchmark
        };

        bool CheckPersistentDecalTables(String& error)
        {
            if (RenderAPI::GetAPI() != RenderAPI::API::Vulkan)
                return true;
            GpuDecalWorld world;
            RenderableDecal first;
            first.Handle = { 0, 1 };
            first.MaterialId = UUID(0, 0, 0, 101);
            auto second = first;
            second.Handle = { 4, 1 };
            const Array<RenderableDecalChange, 2> created{ RenderableDecalChange{ first.Handle, DecalChangeType::Create, first },
                                                           RenderableDecalChange{ second.Handle, DecalChangeType::Create, second } };
            world.Apply(created);
            DecalRenderStats statistics;
            if (!world.Prepare(nullptr, statistics) || world.GetMaterialCount() != 1)
            {
                error = "Persistent decals did not share their material record";
                return false;
            }
            const auto oldFirst = world.GetDecalBuffer()->QueueReadback(0, sizeof(GpuDecalSlot));
            const auto oldMaterial = world.GetMaterialBuffer()->QueueReadback(0, sizeof(GpuDecalMaterialSlot));
            first.Data.Tint.a = 0.25f;
            const RenderableDecalChange updated{ first.Handle, DecalChangeType::Update, first };
            world.Apply({ &updated, 1 });
            statistics = {};
            if (!world.Prepare(nullptr, statistics) || statistics.UploadedBytes != sizeof(GpuDecalSlot))
            {
                error = "Editing one persistent decal did not upload exactly its changed record";
                return false;
            }
            const auto changedFirst = world.GetDecalBuffer()->QueueReadback(0, sizeof(GpuDecalSlot));
            const auto unchangedSecond = world.GetDecalBuffer()->QueueReadback(4 * sizeof(GpuDecalSlot), sizeof(GpuDecalSlot));
            statistics = {};
            if (!world.Prepare(nullptr, statistics) || statistics.UploadedBytes != 0)
            {
                error = "An unchanged persistent decal world uploaded records again";
                return false;
            }
            const Array<RenderableDecalChange, 2> removed{ RenderableDecalChange{ first.Handle, DecalChangeType::Destroy, {} },
                                                           RenderableDecalChange{ second.Handle, DecalChangeType::Destroy, {} } };
            world.Apply(removed);
            first.Handle.Generation = 2;
            first.MaterialId = UUID(0, 0, 0, 102);
            first.Material.Color = { 0.1f, 0.2f, 0.9f, 1 };
            const RenderableDecalChange reused{ first.Handle, DecalChangeType::Create, first };
            world.Apply({ &reused, 1 });
            if (!world.Prepare(nullptr, statistics))
                return false;
            const auto newFirst = world.GetDecalBuffer()->QueueReadback(0, sizeof(GpuDecalSlot));
            const auto newMaterial = world.GetMaterialBuffer()->QueueReadback(0, sizeof(GpuDecalMaterialSlot));
            auto color = CreateColorTexture(1, 1, "PersistentDecalCompletion");
            auto target = CreateTarget({ color }, 1, 1);
            RenderAPI::Get().SetRenderTarget(target);
            RenderAPI::Get().ClearRenderTarget(FBT_COLOR, glm::vec4(0));
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            Image completion;
            if (!Capture(color, completion, error))
                return false;
            GpuDecalSlot original, changed, untouched, replacement;
            GpuDecalMaterialSlot originalMaterial, replacementMaterial;
            if (!oldFirst || !changedFirst || !unchangedSecond || !newFirst || !oldMaterial || !newMaterial ||
                !oldFirst->TryRead(&original, sizeof(original)) || !changedFirst->TryRead(&changed, sizeof(changed)) ||
                !unchangedSecond->TryRead(&untouched, sizeof(untouched)) || !newFirst->TryRead(&replacement, sizeof(replacement)) ||
                !oldMaterial->TryRead(&originalMaterial, sizeof(originalMaterial)) ||
                !newMaterial->TryRead(&replacementMaterial, sizeof(replacementMaterial)))
            {
                error = "Persistent decal table readbacks did not complete";
                return false;
            }
            if (original.Data.Tint.a != 1 || changed.Data.Tint.a != 0.25f || untouched.Data.Tint.a != 1 || original.Generations.x != 1 ||
                replacement.Generations.x != 2 || replacement.Generations.y != 2 || originalMaterial.Generation.x != 1 ||
                replacementMaterial.Generation.x != 2 || originalMaterial.Data.Color != glm::vec4(1) ||
                replacementMaterial.Data.Color != first.Material.Color)
            {
                error = "Persistent decal/material slot reuse changed records retained by an earlier draw";
                return false;
            }
            return true;
        }

        bool CheckDecalGridReadback(String& error)
        {
            if (RenderAPI::GetAPI() != RenderAPI::API::Vulkan)
                return true;
            DecalGpuGrid gpu;
            DecalClusterGrid reference;
            ClusteredLightGridDesc desc;
            desc.ViewportWidth = desc.ViewportHeight = 64;
            const auto projection = glm::perspective(glm::radians(60.0f), 1.0f, desc.NearPlane, desc.FarPlane);
            const Vector<glm::vec4> overflowing(65, glm::vec4(0, 0, -2, 0.5f));
            const Vector<glm::vec4> sparse(2, overflowing.front());
            // Both views reuse the GPU tables before submission. Their copied
            // diagnostics must retain the original view and frame identities.
            if (!gpu.Build(desc, glm::mat4(1), projection, overflowing, 11, 3) || !gpu.Build(desc, glm::mat4(1), projection, sparse, 22, 7))
            {
                error = "Could not build the two-camera decal grid regression";
                return false;
            }
            reference.Build(desc, glm::mat4(1), projection, sparse);
            if (!gpu.MatchesReference(reference.Cells, reference.Indices))
            {
                error = "Sparse GPU decal list differs from its CPU reference";
                return false;
            }
            DecalGridStatistics first, second;
            reference.Build(desc, glm::mat4(1), projection, overflowing);
            const bool haveFirst = gpu.GetStatistics(11, first), haveSecond = gpu.GetStatistics(22, second);
            if (!haveFirst || !haveSecond || first.FrameNumber != 3 || second.FrameNumber != 7 || first.Overflow != reference.Overflow ||
                first.MaxCandidates != 65 || first.OccupiedCells != reference.Overflow || second.Overflow != 0 || second.MaxCandidates != 2 ||
                second.OccupiedCells != first.OccupiedCells)
            {
                error = fmt::format("Deferred GPU decal statistics: first ready={} frame={} overflow={} max={} occupied={}; "
                                    "second ready={} frame={} overflow={} max={} occupied={}; expected overflow={}",
                                    haveFirst, first.FrameNumber, first.Overflow, first.MaxCandidates, first.OccupiedCells, haveSecond,
                                    second.FrameNumber, second.Overflow, second.MaxCandidates, second.OccupiedCells, reference.Overflow);
                return false;
            }
            if (!std::isfinite(first.GpuMilliseconds) || first.GpuMilliseconds <= 0 || !std::isfinite(second.GpuMilliseconds) ||
                second.GpuMilliseconds <= 0)
            {
                error = "Decal grid timestamp queries did not report finite GPU durations for both cameras";
                return false;
            }
            if (!gpu.Build(desc, glm::mat4(1), projection, overflowing, 11, 9))
            {
                error = "Could not rebuild the overflowing decal view";
                return false;
            }
            // Releasing a view also retires an unsubmitted readback safely.
            gpu.ReleaseView(11);
            if (!gpu.MatchesReference(reference.Cells, reference.Indices))
            {
                error = "Overflow GPU decal list differs from its complete-list CPU reference";
                return false;
            }
            if (gpu.GetStatistics(11, first) || !gpu.GetStatistics(22, second))
            {
                error = "Releasing decal diagnostics affected another camera or resurrected a released view";
                return false;
            }
            return true;
        }

        bool CheckRecordedBufferDiscard(String& error)
        {
            if (Texture::NORMAL->GetDesc().sRGB)
            {
                error = "The neutral normal texture must be sampled in linear space";
                return false;
            }
            PixelData neutral(1, 1, 1, Texture::NORMAL->GetDesc().Format);
            neutral.AllocateInternalBuffer();
            Texture::NORMAL->ReadData(neutral);
            if (neutral.GetColorAt(0, 0) != glm::vec4(0.5f, 0.5f, 1.0f, 1.0f))
            {
                error = "The neutral normal texture must encode an exact (0, 0, 1) tangent-space normal";
                return false;
            }
            const String source = R"(#lang glsl
#pragma depth_read false
#pragma depth_write false
#pragma cull none
#type vertex
#version 450
void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450
layout(set = 0, binding = 0, std430) readonly buffer Paint { vec4 value; } paint;
layout(location = 0) out vec4 color;
void main() { color = paint.value; }
)";
            Ref<ShaderRenderPass> pass;
            if (!CompileGraphicsPass("RecordedBufferDiscard", source, pass, error))
                return false;
            const auto params = UniformParams::Create(pass->GetGraphicsPipeline());
            const auto buffer =
              GenericGpuBuffer::Create({ 1, sizeof(glm::vec4), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
            const auto color = CreateColorTexture(32, 16, "RecordedBufferDiscard");
            const auto target = CreateTarget({ color }, 32, 16);
            RenderAPI& api = RenderAPI::Get();
            api.SetRenderTarget(target);
            api.ClearRenderTarget(FBT_COLOR, glm::vec4(0));
            api.SetGraphicsPipeline(pass->GetGraphicsPipeline());
            api.SetVertexLayout(CreateRef<BufferLayout>());
            api.SetDrawMode(DrawMode::TRIANGLE_LIST);
            params->SetBuffer(0, 0, buffer);
            const Array<glm::vec4, 2> colors{ glm::vec4(1, 0, 0, 1), glm::vec4(0, 1, 0, 1) };
            Array<Ref<GpuBufferReadback>, 2> readbacks;
            for (uint32_t draw = 0; draw < colors.size(); ++draw)
            {
                // The second discard must preserve bytes consumed by the first
                // draw, which has been recorded but has not been submitted yet.
                buffer->WriteData(0, sizeof(glm::vec4), &colors[draw], BWT_DISCARD);
                api.SetViewport(draw * 0.5f, 0, 0.5f, 1);
                api.SetUniforms(params);
                api.Draw(0, 3, 1);
                if (RenderAPI::GetAPI() == RenderAPI::API::Vulkan)
                {
                    readbacks[draw] = buffer->QueueReadback(0, sizeof(glm::vec4));
                    glm::vec4 premature;
                    if (!readbacks[draw] || readbacks[draw]->TryRead(&premature, sizeof(premature)))
                    {
                        error = "Buffer readback completed before its command buffer was submitted";
                        return false;
                    }
                }
            }
            const Array<uint32_t, 8> originalWords{ 11, 22, 33, 44, 55, 66, 77, 88 };
            auto patchedWords = originalWords;
            patchedWords[3] = 99;
            Array<Ref<GpuBufferReadback>, 4> rangeReadbacks;
            if (RenderAPI::GetAPI() == RenderAPI::API::Vulkan)
            {
                for (uint32_t usage = 0; usage < 2; ++usage)
                {
                    auto ranged = GenericGpuBuffer::Create({ 8, sizeof(uint32_t), GpuBufferType::Structured, BF_UNKNOWN,
                                                             usage == 0 ? BufferUsage::BU_DYNAMIC_DRAW : BufferUsage::BU_LOADSTORE });
                    ranged->WriteData(0, sizeof(originalWords), originalWords.data(), BWT_DISCARD);
                    rangeReadbacks[usage * 2] = ranged->QueueReadback(0, sizeof(originalWords));
                    ranged->WriteData(3 * sizeof(uint32_t), sizeof(uint32_t), &patchedWords[3], BWT_NORMAL);
                    rangeReadbacks[usage * 2 + 1] = ranged->QueueReadback(0, sizeof(patchedWords));
                }
            }
            api.SubmitCommandBuffer(nullptr);
            Image result;
            if (!Capture(color, result, error))
                return false;
            for (size_t i = 0; i < readbacks.size(); ++i)
                if (readbacks[i])
                {
                    glm::vec4 captured;
                    if (!readbacks[i]->TryRead(&captured, sizeof(captured)) || captured != colors[i])
                    {
                        error = "Asynchronous buffer readback did not retain its recorded data across source reuse";
                        return false;
                    }
                }
            for (size_t i = 0; i < rangeReadbacks.size(); ++i)
                if (RenderAPI::GetAPI() == RenderAPI::API::Vulkan)
                {
                    Array<uint32_t, 8> captured{};
                    if (!rangeReadbacks[i] || !rangeReadbacks[i]->TryRead(captured.data(), sizeof(captured)) ||
                        captured != (i % 2 == 0 ? originalWords : patchedWords))
                    {
                        error = "A recorded partial buffer update lost untouched bytes or changed an earlier readback";
                        return false;
                    }
                }
            const uint8_t* first = result.Pixel(8, 8);
            const uint8_t* second = result.Pixel(24, 8);
            if (first[0] != 255 || first[1] != 0 || second[0] != 0 || second[1] != 255)
            {
                error = fmt::format("Recorded buffer discard mismatch: left ({}, {}, {}, {}), right ({}, {}, {}, {})", first[0], first[1], first[2],
                                    first[3], second[0], second[1], second[2], second[3]);
                return false;
            }
            return true;
        }

        // Mirrors Create > 3D Object > Sphere, including the default material, sun,
        // editor camera, sky and two frames of history-dependent scene rendering.
        bool RenderPrimitiveLitSphere(Image& image, String& error, DecalAcceptance acceptance = DecalAcceptance::None,
                                      RenderingPath path = RenderingPath::ForwardPlus)
        {
            const bool testDecals = acceptance != DecalAcceptance::None;
            if (testDecals && (!CheckRecordedBufferDiscard(error) || !CheckPersistentDecalTables(error) || !CheckDecalGridReadback(error)))
                return false;
            const bool coating = acceptance == DecalAcceptance::Coating || acceptance == DecalAcceptance::ToonCoating;
            const bool cylinder = acceptance == DecalAcceptance::CylinderPartial || acceptance == DecalAcceptance::CylinderFull || coating;
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

            if (!testDecals &&
                (!CheckGpuMaterialUpdates(error) || !CheckMaterialEditing(error) || !CheckEnvironmentCook(error) || !CheckModelMaterialImport(error)))
                return false;
            GenericGpuBufferDesc bufferDesc;
            // Cross the 32 MB staging-pool cutoff used before large FBX uploads were supported.
            bufferDesc.ElementCount = 8u * 1024u * 1024u + 37u;
            bufferDesc.ElementSize = sizeof(uint32_t);
            bufferDesc.Type = GpuBufferType::Structured;
            const Ref<GenericGpuBuffer> buffer = GenericGpuBuffer::Create(bufferDesc);
            const GpuBuffer& baseBuffer = *buffer;
            if (baseBuffer.GetSize() != bufferDesc.ElementCount * sizeof(uint32_t) || baseBuffer.GetSize() != buffer->GetBufferSize())
            {
                error = "Generic GPU buffer capacity differs from its allocation";
                return false;
            }
            Vector<uint32_t> upload(bufferDesc.ElementCount, 0x1234abcd);
            buffer->WriteData(0, baseBuffer.GetSize(), upload.data(), BWT_DISCARD);
            uint32_t lastWord = 0;
            buffer->ReadData(baseBuffer.GetSize() - sizeof(lastWord), sizeof(lastWord), &lastWord);
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            if (lastWord != upload.back())
            {
                error = "Large GPU buffer upload did not preserve its final word";
                return false;
            }

            constexpr uint32_t testSize = 256u;
            if (AssetManager::TryGet() == nullptr)
            {
                error = "The asset manager is unavailable";
                return false;
            }

            PrimitiveMeshLibrary::EnsureRegistered();
            const AssetHandle<Mesh> sphere = PrimitiveMeshLibrary::GetMesh(PrimitiveMeshType::Sphere);
            if (!sphere)
            {
                error = "PrimitiveMeshLibrary::GetMesh(Sphere) returned an unloaded handle";
                return false;
            }

            const Ref<Texture> color = CreateColorTexture(testSize, testSize, "RenderTests/PrimitiveLitSphereColor");
            const Ref<Texture> depth = CreateRenderTexture(testSize, testSize, TextureFormat::DEPTH32F, "RenderTests/PrimitiveLitSphereDepth");
            const Ref<Texture> objectId = CreateRenderTexture(testSize, testSize, TextureFormat::R32I, "RenderTests/PrimitiveLitSphereObjectId");
            const Ref<RenderTexture> target = CreateTarget({ color, objectId }, testSize, testSize, depth);
            if (!color || !depth || !objectId || !target)
            {
                error = "Could not create the primitive sphere render target";
                return false;
            }

            const Ref<Scene> scene = CreateRef<Scene>("Primitive lit sphere render test");
            Entity sphereEntity = scene->CreateEntity("Sphere");
            MeshRendererComponent& meshRenderer = sphereEntity.AddComponent<MeshRendererComponent>();
            meshRenderer.MeshHandle = sphere; // No material, exactly like EntityFactory::CreateMeshEntity.
            if (cylinder)
            {
                const auto data = MeshFactory::CreateCylinderData(0.5f, 1.0f, 64);
                auto positions = data->GetPositions();
                auto normals = data->GetNormals();
                for (size_t i = 0; i < positions.size(); ++i)
                {
                    const float radius = glm::mix(0.5f, 0.35f, positions[i].y + 0.5f);
                    positions[i].x *= radius / 0.5f;
                    positions[i].z *= radius / 0.5f;
                    if (std::abs(normals[i].y) < 0.5f)
                        normals[i] = glm::normalize(glm::vec3(normals[i].x, 0.15f, normals[i].z));
                }
                data->SetPositions(positions);
                data->SetNormals(normals);
                MeshDesc mesh;
                mesh.Data = data;
                mesh.SubMeshes.emplace_back(0, data->GetIndexCount(), DrawMode::TRIANGLE_LIST);
                meshRenderer.MeshHandle = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(Mesh::Create(mesh)));
            }

            Entity lightEntity = scene->CreateEntity("Directional Light");
            LightComponent& light = lightEntity.AddComponent<LightComponent>();
            light.Type = LightType::Directional;
            light.Color = glm::vec3(1.0f);
            // Keep edits measurable at the fixture's fixed unit exposure.
            light.Intensity = 3.0f;
            light.Shadows.Mode = LightShadowMode::Soft;
            lightEntity.GetTransform().SetRotation(glm::quat(glm::radians(glm::vec3(-50.0f, -30.0f, 0.0f))));

            EditorCamera camera(45.0f, 1.0f, 0.1f, 1000.0f);
            camera.SetViewportSize(static_cast<float>(testSize), static_cast<float>(testSize));
            camera.SetFocalPoint(glm::vec3(0.0f));
            camera.SetDistance(3.0f);
            camera.SetPitch(0.0f);
            camera.SetYaw(0.0f);

            SceneRenderer renderer(scene, target);
            renderer.Init();
            {
                auto settings = renderer.GetRenderPipelineSettings();
                settings.Path = path;
                settings.EnableTaa = false;
                // Compare sampled surfaces without Vulkan-only screen effects.
                settings.EnableBloom = false;
                settings.EnableGtao = false;
                renderer.SetRenderPipelineSettings(settings);
            }
            const bool vulkan = RenderAPI::GetAPI() == RenderAPI::API::Vulkan;
            const auto renderFrame = [&](uint32_t frame, bool validateLists = false) {
                if (vulkan)
                {
                    RenderThread* renderThread = Application::Get().GetRenderThread();
                    if (renderThread == nullptr)
                    {
                        error = "The Vulkan render thread is unavailable";
                        return false;
                    }
                    RenderSnapshot& snapshot = renderThread->BeginFrame();
                    snapshot.FrameNumber = frame;
                    snapshot.EnableObjectID = true;
                    renderer.ExtractSnapshot(snapshot, camera, camera.GetViewMatrix(), false);
                    snapshot.EnableObjectID = true;
                    snapshot.ValidateDecalLists = validateLists || (testDecals && acceptance != DecalAcceptance::Benchmark);
                    renderThread->SubmitFrame();
                    renderThread->WaitForFrameDone();
                    RenderAPI::Get().SubmitCommandBuffer(nullptr);
                    if ((validateLists || (testDecals && acceptance != DecalAcceptance::Benchmark)) &&
                        SceneRenderer::GetStatistics().Decals.ListValidationFailures)
                    {
                        error = "GPU decal lists disagreed with the CPU reference on frame " + std::to_string(frame);
                        return false;
                    }
                }
                else
                {
                    RenderSnapshot snapshot;
                    snapshot.FrameNumber = frame;
                    snapshot.EnableObjectID = true;
                    renderer.ExtractSnapshot(snapshot, camera, camera.GetViewMatrix(), false);
                    snapshot.EnableObjectID = true;
                    SceneRenderer::RenderFromSnapshot(snapshot);
                    RenderAPI::Get().SubmitCommandBuffer(nullptr);
                }
                return true;
            };

            for (uint32_t frame = 1u; frame <= 2u; ++frame)
                if (!renderFrame(frame))
                    return false;

            if (!testDecals)
            {
                // Editor guides depth-test against this final target after tone mapping.
                const bool reverseDepth = RenderAPI::Get().GetCapabilities().GetFeatureTier() != RenderFeatureTier::Compatibility;
                float backgroundDepth = -1.0f;
                float sphereDepth = -1.0f;
                if (!depth->ReadPixel(2u, 2u, &backgroundDepth, sizeof(backgroundDepth)) ||
                    !depth->ReadPixel(testSize / 2u, testSize / 2u, &sphereDepth, sizeof(sphereDepth)) || !std::isfinite(backgroundDepth) ||
                    !std::isfinite(sphereDepth) || std::abs(backgroundDepth - (reverseDepth ? 0.0f : 1.0f)) > 0.00001f || sphereDepth <= 0.0f ||
                    sphereDepth >= 1.0f || (reverseDepth ? sphereDepth <= backgroundDepth : sphereDepth >= backgroundDepth))
                {
                    error = "Scene rendering did not preserve depth for viewport guides: background=" + std::to_string(backgroundDepth) +
                            " sphere=" + std::to_string(sphereDepth);
                    return false;
                }
            }

            if (!Capture(color, image, error, CaptureOrigin::SceneViewport))
                return false;

            // The sphere covers the middle of the frame; anything that differs from the background corner counts as drawn.
            const uint8_t* corner = image.Pixel(2u, 2u);
            uint32_t spherePixels = 0u;
            for (uint32_t y = 0; y < image.Height; y++)
                for (uint32_t x = 0; x < image.Width; x++)
                {
                    const uint8_t* pixel = image.Pixel(x, y);
                    if (pixel[0] != corner[0] || pixel[1] != corner[1] || pixel[2] != corner[2])
                        spherePixels++;
                }
            if (spherePixels < 2000u)
            {
                const GpuSceneUploadStats& stats = Renderer::GetGpuScene().GetStats();
                std::ostringstream diagnostics;
                diagnostics << "spherePixels=" << spherePixels << " activeInstances=" << stats.ActiveInstances
                            << " activeLights=" << stats.ActiveLights << " visibleInstances=" << stats.VisibleInstances
                            << " indirectCommands=" << stats.IndirectCommands << " drawBins=" << stats.DrawBinCount
                            << " heapPages=" << stats.GeometryHeapPages;
                error = "The primitive sphere was not visible: " + diagnostics.str();
                return false;
            }
            int32_t pickedId = 0;
            if (!objectId->ReadPixel(testSize / 2, testSize / 2, &pickedId, sizeof(pickedId)) ||
                pickedId != static_cast<int32_t>(static_cast<uint32_t>(sphereEntity.GetHandle())) + 1)
            {
                error = "Viewport material drop could not pick the sphere from its object ID";
                return false;
            }
            const bool unlit = acceptance == DecalAcceptance::Unlit;
            const bool toon = acceptance == DecalAcceptance::Toon || acceptance == DecalAcceptance::ToonCoating;
            const bool standard = !unlit && !toon;
            const char* colorParameter = standard ? "albedo" : "tint";
            const Ref<Material> edited = unlit  ? Material::CreateUnlit(AssetManager::Get().Load<Shader>("Resources/Shaders/Unlit.asset"))
                                         : toon ? Material::CreateToon(AssetManager::Get().Load<Shader>("Resources/Shaders/Toon.asset"))
                                                : Material::CreateDefault();
            edited->SetColor(colorParameter, glm::vec4(0.05f, 0.8f, 0.1f, 1.0f));
            if (standard)
                edited->SetFloat("roughness", 0.73f);
            if (toon)
            {
                edited->SetFloat("thickness", 0.0f);
                edited->SetFloat("toonSilhouetteWidth", 0.0f);
            }
            if (coating)
            {
                edited->SetAlphaMode(AlphaMode::WeightedOIT);
                edited->SetColor(colorParameter, glm::vec4(0.05f, 0.8f, 0.1f, 0.15f));
                Entity behind = scene->CreateEntity("Transparent object behind the paper label");
                behind.GetTransform().SetPosition({ 0, 0, -0.8f });
                auto& receiver = behind.AddComponent<MeshRendererComponent>();
                receiver.MeshHandle = sphere;
                auto blue = Material::CreateDefault();
                blue->SetColor("albedo", glm::vec4(0.05f, 0.1f, 0.9f, 0.6f));
                blue->SetAlphaMode(AlphaMode::WeightedOIT);
                receiver.Materials.push_back(static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(blue)));
            }
            const AssetHandle<Material> editedHandle = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(edited));
            Entity picked(static_cast<entt::entity>(pickedId - 1), scene.get());
            if (!AssignViewportMaterial(picked, editedHandle))
            {
                error = "Viewport material drop did not assign the material";
                return false;
            }
            for (uint32_t frame = 3u; frame <= 4u; ++frame)
                if (!renderFrame(frame))
                    return false;
            Image assignedImage;
            if (!Capture(color, assignedImage, error, CaptureOrigin::SceneViewport))
                return false;
            if ((standard && std::abs(edited->GetDataParam<float>("roughness") - 0.73f) > 0.001f) ||
                std::abs(edited->GetDataParam<glm::vec4>(colorParameter).g - 0.8f) > 0.001f)
            {
                error = "Rendering overwrote the assigned material's editable parameters";
                return false;
            }
            uint32_t changedPixels = 0;
            for (uint32_t y = 0; y < image.Height; ++y)
                for (uint32_t x = 0; x < image.Width; ++x)
                    if (std::memcmp(image.Pixel(x, y), assignedImage.Pixel(x, y), 3) != 0)
                        ++changedPixels;
            if (changedPixels < 100u)
            {
                error = "Assigning an edited material did not change the sphere's rendered appearance";
                return false;
            }
            if (testDecals)
            {
                Entity projector = scene->CreateEntity("Box decal crossing curved receiver");
                auto& decal = projector.AddComponent<DecalComponent>();
                decal.Size = { 0.7f, 0.7f, 1.2f };
                if (cylinder)
                {
                    decal.Projection = DecalProjection::Cylinder;
                    decal.Height = 0.5f;
                    decal.BottomRadius = 0.4625f;
                    decal.TopRadius = 0.3875f;
                    decal.ShellThickness = 0.04f;
                    decal.Arc = acceptance == DecalAcceptance::CylinderFull ? 360.0f : 180.0f;
                    // Put the full-wrap seam on the visible front of the bottle.
                    decal.SeamRotation = acceptance == DecalAcceptance::CylinderFull ? 0.0f : -90.0f;
                }
                const Ref<Material> sticker = Material::CreateDecal();
                if (acceptance == DecalAcceptance::CameraInside)
                    decal.Size = glm::vec3(10.0f);
                if (!sticker || sticker->GetDomain() != MaterialDomain::Decal)
                {
                    error = "The built-in decal material was not available";
                    return false;
                }
                sticker->SetColor("decalColor", glm::vec4(0.9f, 0.03f, 0.02f, 1));
                if (cylinder || acceptance == DecalAcceptance::Correction || acceptance == DecalAcceptance::NormalDetail)
                {
                    TextureDesc pattern;
                    pattern.Width = pattern.Height = 64;
                    pattern.MipLevels = 6;
                    pattern.Format = TextureFormat::RGBA8;
                    pattern.Usage = TextureUsage::TEXTURE_STATIC;
                    pattern.sRGB = false;
                    pattern.DebugName = "Decal seamless checker";
                    const auto texture = Texture::Create(pattern);
                    for (uint32_t mip = 0; mip <= 6; ++mip)
                    {
                        const uint32_t size = std::max(64u >> mip, 1u);
                        PixelData pixels(size, size, 1, TextureFormat::RGBA8);
                        pixels.AllocateInternalBuffer();
                        for (uint32_t y = 0; y < size; ++y)
                            for (uint32_t x = 0; x < size; ++x)
                            {
                                const float intensity = mip > 3 ? 0.6f : (((x << mip) / 8 + (y << mip) / 8) & 1) ? 0.2f : 1.0f;
                                if (acceptance == DecalAcceptance::NormalDetail)
                                {
                                    const float tilt = mip > 3 ? 0.0f : intensity > 0.5f ? 0.45f : -0.45f;
                                    const glm::vec3 normal = glm::normalize(glm::vec3(tilt, 0, 1));
                                    pixels.SetColorAt(x, y, glm::vec4(normal * 0.5f + 0.5f, 1));
                                }
                                else
                                    pixels.SetColorAt(x, y, glm::vec4(intensity, intensity, intensity, 1));
                            }
                        texture->WriteData(pixels, mip);
                    }
                    const char* textureSlot = acceptance == DecalAcceptance::Correction     ? "decalMaskMap"
                                              : acceptance == DecalAcceptance::NormalDetail ? "decalNormalMap"
                                                                                            : "decalColorMap";
                    sticker->SetTexture(textureSlot, static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(texture)));
                    decal.EdgeFeather = 0.015f;
                }
                if (acceptance == DecalAcceptance::Correction)
                {
                    sticker->SetInt("decalChannels", 64);
                    sticker->SetFloat("decalHue", 120.0f);
                    sticker->SetFloat("decalExposure", -0.5f);
                    sticker->SetFloat("decalContrast", 1.2f);
                }
                if (acceptance == DecalAcceptance::Wetness)
                {
                    sticker->SetInt("decalChannels", 4);
                    sticker->SetFloat("decalRoughness", 0.05f);
                }
                if (acceptance == DecalAcceptance::NormalDetail)
                    sticker->SetInt("decalChannels", 2);
                if (coating)
                {
                    sticker->SetInt("decalChannels", 129);
                    decal.TargetMode = DecalTargetMode::Entity;
                    decal.Target = sphereEntity.GetUuid();
                }
                decal.Material = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(sticker));
                for (uint32_t frame = 5; frame <= 6; ++frame)
                    if (!renderFrame(frame))
                        return false;
                if (vulkan && (!SceneRenderer::GetStatistics().Decals.GpuBuiltLists || SceneRenderer::GetStatistics().Decals.ListValidationFailures))
                {
                    error = "The Vulkan decal grid was unavailable or disagreed with its CPU reference";
                    return false;
                }
                Image decalImage;
                if (!Capture(color, decalImage, error, CaptureOrigin::SceneViewport))
                    return false;
                if (s_DecalShowcase && !s_DecalShowcase->WriteScene(scene, s_CurrentTestName, error))
                    return false;
                uint32_t changed = 0;
                for (uint32_t y = 0; y < image.Height; ++y)
                    for (uint32_t x = 0; x < image.Width; ++x)
                        if (std::memcmp(assignedImage.Pixel(x, y), decalImage.Pixel(x, y), 3) != 0)
                            ++changed;
                if (changed < 100)
                {
                    error = "The decal did not change its curved receiver";
                    return false;
                }
                if (!objectId->ReadPixel(testSize / 2, testSize / 2, &pickedId, sizeof(pickedId)) ||
                    pickedId != static_cast<int32_t>(static_cast<uint32_t>(sphereEntity.GetHandle())) + 1)
                {
                    error = "The decal replaced the receiver's picking identity";
                    return false;
                }
                if (coating)
                {
                    Entity behind = scene->FindEntityByName("Transparent object behind the paper label");
                    const auto behindMaterial = behind.GetComponent<MeshRendererComponent>().GetMaterial(0);
                    behindMaterial->SetColor("albedo", glm::vec4(0.9f, 0.9f, 0.02f, 0.6f));
                    if (!renderFrame(7))
                        return false;
                    Image changedBehind;
                    if (!Capture(color, changedBehind, error, CaptureOrigin::SceneViewport))
                        return false;
                    for (uint32_t y = testSize / 2 - 4; y < testSize / 2 + 4; ++y)
                        for (uint32_t x = testSize / 2 - 4; x < testSize / 2 + 4; ++x)
                            for (uint32_t channel = 0; channel < 3; ++channel)
                                if (std::abs(int(changedBehind.Pixel(x, y)[channel]) - int(decalImage.Pixel(x, y)[channel])) > 2)
                                {
                                    error = "The transparent object behind the opaque label leaked through its core";
                                    return false;
                                }
                    behindMaterial->SetColor("albedo", glm::vec4(0.05f, 0.1f, 0.9f, 0.6f));
                }
                if (acceptance == DecalAcceptance::Correction)
                {
                    sticker->SetFloat("decalHue", 0.0f);
                    sticker->SetFloat("decalExposure", 0.0f);
                    sticker->SetFloat("decalContrast", 1.0f);
                    if (!renderFrame(7))
                        return false;
                    Image neutralImage;
                    if (!Capture(color, neutralImage, error, CaptureOrigin::SceneViewport))
                        return false;
                    for (uint32_t y = 0; y < image.Height; ++y)
                        for (uint32_t x = 0; x < image.Width; ++x)
                            for (uint32_t channel = 0; channel < 3; ++channel)
                                if (std::abs(int(assignedImage.Pixel(x, y)[channel]) - int(neutralImage.Pixel(x, y)[channel])) > 1)
                                {
                                    error = "Neutral masked corrections changed the receiver";
                                    return false;
                                }
                    sticker->SetFloat("decalHue", 120.0f);
                    sticker->SetFloat("decalExposure", -0.5f);
                    sticker->SetFloat("decalContrast", 1.2f);
                }
                if (unlit)
                {
                    light.Intensity = 0.0f;
                    if (!renderFrame(7))
                        return false;
                    Image withoutLight;
                    if (!Capture(color, withoutLight, error, CaptureOrigin::SceneViewport))
                        return false;
                    for (uint32_t y = 0; y < image.Height; ++y)
                        for (uint32_t x = 0; x < image.Width; ++x)
                            if (std::memcmp(decalImage.Pixel(x, y), withoutLight.Pixel(x, y), 3) != 0)
                            {
                                error = "A decal caused an unlit receiver to respond to lighting";
                                return false;
                            }
                    light.Intensity = 3.0f;
                }
                meshRenderer.ReceiveDecals = false;
                const uint32_t rejectionStart = coating || acceptance == DecalAcceptance::Correction || unlit ? 8u : 7u;
                for (uint32_t frame = rejectionStart; frame < rejectionStart + 2; ++frame)
                    if (!renderFrame(frame))
                        return false;
                Image rejectedImage;
                if (!Capture(color, rejectedImage, error, CaptureOrigin::SceneViewport))
                    return false;
                uint64_t difference = 0;
                for (uint32_t y = 0; y < image.Height; ++y)
                    for (uint32_t x = 0; x < image.Width; ++x)
                        for (uint32_t channel = 0; channel < 3; ++channel)
                            difference += std::abs(int(assignedImage.Pixel(x, y)[channel]) - int(rejectedImage.Pixel(x, y)[channel]));
                if (difference > image.Width * image.Height * 3u)
                {
                    error = "Disabling decal reception did not restore the receiver";
                    return false;
                }
                image = std::move(decalImage);
                if (acceptance == DecalAcceptance::Box)
                {
                    const auto originalMesh = meshRenderer.MeshHandle;
                    const auto originalSettings = static_cast<const DecalSettings&>(decal);
                    meshRenderer.ReceiveDecals = true;
                    decal.Offset.z = 0.3f;
                    decal.Size.z = 0.02f;
                    const BufferLayout layout = {
                        { ShaderDataType::Float3, VertexAttribute::Position },  { ShaderDataType::Float3, VertexAttribute::Normal },
                        { ShaderDataType::Float3, VertexAttribute::Tangent },   { ShaderDataType::Float3, VertexAttribute::Bitangent },
                        { ShaderDataType::Float2, VertexAttribute::TexCoord0 }, { ShaderDataType::Float4, VertexAttribute::BlendWeights },
                        { ShaderDataType::Int4, VertexAttribute::BlendIndices }
                    };
                    const auto source = MeshData::Create(4, 6, layout);
                    source->SetPositions({ { -0.45f, -0.45f, 0.3f }, { 0.45f, -0.45f, 0.3f }, { 0.45f, 0.45f, 0.3f }, { -0.45f, 0.45f, 0.3f } });
                    source->SetNormals(Vector<glm::vec3>(4, { 0, 0, 1 }));
                    source->SetTangents(Vector<glm::vec3>(4, { 1, 0, 0 }));
                    source->SetBitangents(Vector<glm::vec3>(4, { 0, 1, 0 }));
                    source->SetUVs(0, { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } });
                    source->SetIndices({ 0, 1, 2, 2, 3, 0 });
                    const Array<glm::vec4, 4> weights{ glm::vec4(1, 0, 0, 0), glm::vec4(1, 0, 0, 0), glm::vec4(1, 0, 0, 0), glm::vec4(1, 0, 0, 0) };
                    const Array<glm::ivec4, 4> bones{ glm::ivec4(0), glm::ivec4(0), glm::ivec4(1), glm::ivec4(1) };
                    source->SetVertexData(VertexAttribute::BlendWeights, weights.data(), sizeof(weights));
                    source->SetVertexData(VertexAttribute::BlendIndices, bones.data(), sizeof(bones));
                    const auto skeleton =
                      Skeleton::Create({ { "Root", INVALID_BONE_INDEX, Transform(), glm::mat4(1) }, { "Upper", 0, Transform(), glm::mat4(1) } });
                    auto& animation = sphereEntity.AddComponent<AnimationComponent>();
                    animation.Deformer = CreateRef<MeshDeformer>();
                    SkeletonPose pose(skeleton);
                    if (!animation.Deformer->Initialize(source, skeleton) || !animation.Deformer->Deform(&pose))
                    {
                        error = "Could not initialize the skinned decal receiver";
                        return false;
                    }
                    MeshDesc description;
                    description.Data = animation.Deformer->GetOutputMeshData();
                    description.Usage = MeshUsage::Dynamic | MeshUsage::CpuCached;
                    description.SubMeshes.emplace_back(0, 6, DrawMode::TRIANGLE_LIST);
                    animation.RuntimeMesh = Mesh::Create(description);
                    animation.RuntimeMeshHandle = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(animation.RuntimeMesh));
                    if (!renderFrame(20))
                        return false;
                    Image bindPose;
                    if (!Capture(color, bindPose, error, CaptureOrigin::SceneViewport))
                        return false;
                    const auto* center = bindPose.Pixel(testSize / 2, testSize / 2);
                    if (center[0] <= center[1])
                    {
                        String captureError;
                        SaveBmp(Path("artifacts/decals") / ("skin-bind-" + BackendName(RenderAPI::GetAPI()) + ".bmp"), bindPose, captureError);
                        error = "The thin decal volume did not cover the skinned receiver's bind pose: " + std::to_string(center[0]) + "," +
                                std::to_string(center[1]) + "," + std::to_string(center[2]);
                        return false;
                    }
                    pose.GetLocalTransform(1).SetPosition({ 0.4f, 0, 0 });
                    pose.RebuildMatrices();
                    animation.Deformer->Deform(&pose);
                    animation.RuntimeMesh->WriteData(animation.Deformer->GetOutputMeshData(), true);
                    if (!renderFrame(21))
                        return false;
                    Image deformed;
                    if (!Capture(color, deformed, error, CaptureOrigin::SceneViewport))
                        return false;
                    const auto* projectedCenter = deformed.Pixel(testSize / 2, testSize / 2);
                    const auto* outsideProjector = deformed.Pixel(180, testSize / 2);
                    if (projectedCenter[0] <= projectedCenter[1] || outsideProjector[1] <= outsideProjector[0])
                    {
                        error = "The decal followed skin coordinates instead of its fixed world projection";
                        return false;
                    }
                    uint32_t changed = 0;
                    for (uint32_t y = 0; y < deformed.Height; ++y)
                        for (uint32_t x = 0; x < deformed.Width; ++x)
                            changed += std::memcmp(deformed.Pixel(x, y), bindPose.Pixel(x, y), 3) != 0;
                    if (changed < 100)
                    {
                        error = "Skinning did not move the receiver through the fixed decal projection";
                        return false;
                    }
                    // Move the entire skin just behind the thin projection band. It must match disabled reception.
                    pose.GetLocalTransform(0).SetPosition({ 0, 0, -0.04f });
                    pose.RebuildMatrices();
                    animation.Deformer->Deform(&pose);
                    animation.RuntimeMesh->WriteData(animation.Deformer->GetOutputMeshData(), true);
                    if (!renderFrame(22))
                        return false;
                    Image behindVolume;
                    if (!Capture(color, behindVolume, error, CaptureOrigin::SceneViewport))
                        return false;
                    meshRenderer.ReceiveDecals = false;
                    if (!renderFrame(23))
                        return false;
                    Image excluded;
                    if (!Capture(color, excluded, error, CaptureOrigin::SceneViewport))
                        return false;
                    for (uint32_t y = 0; y < excluded.Height; ++y)
                        for (uint32_t x = 0; x < excluded.Width; ++x)
                            if (std::memcmp(excluded.Pixel(x, y), behindVolume.Pixel(x, y), 3) != 0)
                            {
                                error = "A decal leaked onto skinned geometry behind its thin projection volume";
                                return false;
                            }
                    sphereEntity.RemoveComponent<AnimationComponent>();
                    meshRenderer.MeshHandle = originalMesh;
                    static_cast<DecalSettings&>(decal) = originalSettings;
                }
                if (acceptance == DecalAcceptance::CylinderFull)
                {
                    meshRenderer.ReceiveDecals = true;
                    decal.UVScale = glm::vec2(256.0f);
                    if (!renderFrame(20))
                        return false;
                    Image minified;
                    if (!Capture(color, minified, error, CaptureOrigin::SceneViewport))
                        return false;
                    // The authored coarse checker mips are constant 0.6. Compare with that constant
                    // sampled directly, including the front-facing seam and compatibility atlas wrapping.
                    const auto checker = sticker->GetTextureHandle("decalColorMap");
                    sticker->SetTexture("decalColorMap", Texture::WHITE);
                    sticker->SetColor("decalColor", { 0.54f, 0.018f, 0.012f, 1 });
                    if (!renderFrame(21))
                        return false;
                    Image average;
                    if (!Capture(color, average, error, CaptureOrigin::SceneViewport))
                        return false;
                    for (uint32_t y = 0; y < average.Height; ++y)
                        for (uint32_t x = 0; x < average.Width; ++x)
                            for (uint32_t channel = 0; channel < 3; ++channel)
                                if (std::abs(int(minified.Pixel(x, y)[channel]) - int(average.Pixel(x, y)[channel])) > 2)
                                {
                                    error = "Minified cylinder wrap did not converge to its authored mip color at " + std::to_string(x) + "," +
                                            std::to_string(y);
                                    return false;
                                }
                    sticker->SetTexture("decalColorMap", checker);
                }
                if (vulkan && acceptance == DecalAcceptance::Box)
                {
                    // Compare the first changed TAA frame with the same surface rendered without history.
                    // Warm each state first so movement, fading, material edits and removal all exercise live history.
                    meshRenderer.ReceiveDecals = true;
                    auto settings = renderer.GetRenderPipelineSettings();
                    uint32_t frame = 30;
                    for (uint32_t change = 0; change < 4; ++change)
                    {
                        settings.EnableTaa = true;
                        renderer.SetRenderPipelineSettings(settings);
                        for (uint32_t warmup = 0; warmup < 8; ++warmup)
                            if (!renderFrame(frame++))
                                return false;
                        switch (change)
                        {
                        case 0:
                            projector.SetPosition({ 0.2f, 0, 0 });
                            break;
                        case 1:
                            decal.Lifetime = 1.0f;
                            decal.FadeOut = 1.0f;
                            decal.Age = 1.5f;
                            break;
                        case 2:
                            sticker->SetColor("decalColor", { 0.02f, 0.03f, 0.9f, 1.0f });
                            break;
                        case 3:
                            scene->DestroyEntity(projector);
                            break;
                        }
                        if (!renderFrame(frame++))
                            return false;
                        Image temporal;
                        if (!Capture(color, temporal, error, CaptureOrigin::SceneViewport))
                            return false;
                        settings.EnableTaa = false;
                        renderer.SetRenderPipelineSettings(settings);
                        if (!renderFrame(frame++))
                            return false;
                        Image current;
                        if (!Capture(color, current, error, CaptureOrigin::SceneViewport))
                            return false;
                        for (uint32_t y = 0; y < current.Height; ++y)
                            for (uint32_t x = 0; x < current.Width; ++x)
                                for (uint32_t channel = 0; channel < 3; ++channel)
                                    if (std::abs(int(temporal.Pixel(x, y)[channel]) - int(current.Pixel(x, y)[channel])) > 1)
                                    {
                                        error = "Decal TAA history survived change " + std::to_string(change) + " at " + std::to_string(x) + "," +
                                                std::to_string(y);
                                        return false;
                                    }
                    }
                }
                if (acceptance == DecalAcceptance::Overlap)
                {
                    meshRenderer.ReceiveDecals = true;
                    const auto settings = decal;
                    Entity upperEntity = scene->CreateEntity("Overlapping blue decal");
                    auto& upper = upperEntity.AddComponent<DecalComponent>();
                    upper = settings;
                    upper.SortOrder = 1;
                    upperEntity.SetPosition({ 0.15f, 0, 0 });
                    auto blue = Material::CreateDecal();
                    blue->SetColor("decalColor", { 0.02f, 0.03f, 0.9f, 1.0f });
                    upper.Material = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(blue));
                    if (!renderFrame(20))
                        return false;
                    Image higher;
                    if (!Capture(color, higher, error, CaptureOrigin::SceneViewport))
                        return false;
                    upper.SortOrder = -1;
                    if (!renderFrame(21))
                        return false;
                    Image lower;
                    if (!Capture(color, lower, error, CaptureOrigin::SceneViewport))
                        return false;
                    const auto* original = image.Pixel(testSize / 2, testSize / 2);
                    const auto* below = lower.Pixel(testSize / 2, testSize / 2);
                    const auto* above = higher.Pixel(testSize / 2, testSize / 2);
                    if (std::memcmp(original, below, 3) != 0 || above[2] <= above[0] || below[0] <= below[2])
                    {
                        error = "Changing decal sort order did not restore the underlying red layer";
                        return false;
                    }
                    scene->DestroyEntity(upperEntity);
                    if (!renderFrame(22))
                        return false;
                    Image removed;
                    if (!Capture(color, removed, error, CaptureOrigin::SceneViewport))
                        return false;
                    if (std::memcmp(original, removed.Pixel(testSize / 2, testSize / 2), 3) != 0)
                    {
                        error = "Removing an overlapping decal did not restore the underlying layer";
                        return false;
                    }
                    image = std::move(higher);
                }
                if (acceptance == DecalAcceptance::Benchmark)
                {
                    meshRenderer.ReceiveDecals = true;
                    decal.Enabled = false;
                    Vector<Entity> projectors;
                    const Path output = Path("artifacts/decals") / ("benchmark-" + BackendName(RenderAPI::GetAPI()) + ".json");
                    fs::create_directories(output.parent_path());
                    std::ofstream metrics(output);
                    metrics << "{\"device\":\"" << EscapeJson(RenderAPI::Get().GetCapabilities().DeviceName)
                            << "\",\"resolution\":256,\"taa\":false,\"bloom\":false,\"gtao\":false,"
                               "\"measurement\":\"CPU submission plus GPU completion latency\",\"scenarios\":[";
                    uint32_t frame = 9;
                    for (uint32_t scenario = 0; scenario < 3; ++scenario)
                    {
                        for (Entity previous : projectors)
                            scene->DestroyEntity(previous);
                        projectors.clear();
                        const uint32_t count = scenario == 0 ? 0 : scenario == 1 ? 1000 : 65;
                        for (uint32_t index = 0; index < count; ++index)
                        {
                            Entity instance = scene->CreateEntity("Benchmark decal");
                            auto& d = instance.AddComponent<DecalComponent>();
                            d.Material = decal.Material;
                            if (scenario == 1)
                            {
                                d.Size = glm::vec3(0.12f);
                                instance.GetTransform().SetPosition(glm::vec3(index % 10, index / 10 % 10, index / 100) * 0.1f - 0.45f);
                            }
                            else
                                d.Size = glm::vec3(1.2f);
                            projectors.push_back(instance);
                        }
                        for (uint32_t warmup = 0; warmup < 3; ++warmup)
                            if (!renderFrame(frame++))
                                return false;
                        const auto started = std::chrono::steady_clock::now();
                        constexpr uint32_t samples = 12;
                        double gridMilliseconds = 0;
                        uint32_t gridSamples = 0;
                        uint64_t lastGridFrame = 0;
                        for (uint32_t sample = 0; sample < samples; ++sample)
                        {
                            if (!renderFrame(frame++))
                                return false;
                            uint8_t completedPixel[4]{};
                            if (!color->ReadPixel(0, 0, completedPixel, sizeof(completedPixel)))
                            {
                                error = "Decal benchmark completion readback failed";
                                return false;
                            }
                            const auto stats = SceneRenderer::GetStatistics().Decals;
                            if (stats.GpuBuiltLists && stats.ClusterStatisticsAvailable && stats.StatisticsFrame != lastGridFrame)
                            {
                                gridMilliseconds += stats.GridGpuMilliseconds;
                                ++gridSamples;
                                lastGridFrame = stats.StatisticsFrame;
                            }
                        }
                        const double milliseconds =
                          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count() / samples;
                        // Validate the larger and overflowing lists outside the timed interval.
                        if (!renderFrame(frame++, true))
                            return false;
                        const auto stats = SceneRenderer::GetStatistics().Decals;
                        if (!stats.ClusterStatisticsAvailable || (scenario == 2 && stats.OverflowClusters == 0))
                        {
                            error = "Decal benchmark did not receive completed cluster overflow statistics";
                            return false;
                        }
                        if (scenario)
                            metrics << ',';
                        metrics << "{\"sceneDecals\":" << count << ",\"visible\":" << stats.Visible
                                << ",\"overflowClusters\":" << stats.OverflowClusters << ",\"milliseconds\":" << milliseconds
                                << ",\"gridGpuSamples\":" << gridSamples << ",\"gridGpuMilliseconds\":";
                        if (gridSamples)
                            metrics << gridMilliseconds / gridSamples;
                        else
                            metrics << "null";
                        metrics << '}';
                    }
                    metrics << "]}";
                    if (!metrics)
                    {
                        error = "Could not write decal benchmark measurements";
                        return false;
                    }
                }
            }
            return true;
        }

        bool RenderCoatingCore(Image& image, String& error, DecalAcceptance acceptance, RenderingPath path = RenderingPath::ForwardPlus)
        {
            Image scene;
            if (!RenderPrimitiveLitSphere(scene, error, acceptance, path))
                return false;
            // This rectangle is fully inside the authored opaque label. Compare it
            // independently of the two backends' different glass compositors.
            image = Image(65, 40);
            for (uint32_t y = 0; y < image.Height; ++y)
                for (uint32_t x = 0; x < image.Width; ++x)
                    std::memcpy(image.Pixel(x, y), scene.Pixel(x + 95, y + 108), 4);
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
            Tolerance textTolerance;
            // MSDF coverage amplifies subtexel sampling/interpolation differences
            // at antialiased edges. Iris Xe Vulkan/OpenGL measured max 13/255
            // and mean 0.0191/255; layout, IDs and legacy parity are checked separately.
            textTolerance.PixelThreshold = 3u;
            textTolerance.MaxChannelError = 16u;
            textTolerance.MaxMeanAbsoluteError = 0.025;
            textTolerance.MaxFailingPixelRatio = 0.005;
            Tolerance toonTolerance;
            // The legacy OpenGL inverted hull is slightly wider than the pixel-scaled GPU-driven Vulkan path.
            // Ignore target-alpha differences and allow only the measured one-pixel silhouette fringe.
            toonTolerance.PixelThreshold = 8u;
            toonTolerance.MaxChannelError = 255u;
            toonTolerance.MaxMeanAbsoluteError = 8.0;
            toonTolerance.MaxFailingPixelRatio = 0.02;
            toonTolerance.CompareAlpha = false;
            Tolerance decalTolerance;
            // Opaque surfaces share lighting and tone mapping. Allow the measured
            // background clear and narrow cylindrical seam/interpolation differences.
            decalTolerance.PixelThreshold = 8u;
            decalTolerance.MaxChannelError = 40u;
            decalTolerance.MaxMeanAbsoluteError = 5.2;
            decalTolerance.MaxFailingPixelRatio = 0.001;
            decalTolerance.CompareAlpha = false;
            Tolerance coatingTolerance = decalTolerance;
            // Sorted LDR glass and HDR weighted OIT differ outside the label core.
            // Dedicated core captures below retain a strict coating comparison.
            coatingTolerance.MaxChannelError = 160u;
            coatingTolerance.MaxMeanAbsoluteError = 10.0;
            coatingTolerance.MaxFailingPixelRatio = 0.10;
            Tolerance coatingCoreTolerance = decalTolerance;
            coatingCoreTolerance.PixelThreshold = 12u;
            coatingCoreTolerance.MaxChannelError = 16u;
            coatingCoreTolerance.MaxMeanAbsoluteError = 0.5;
            return {
                { "solid-clear", exact, RenderSolidClear },
                { "persistent-sprites", shaderTolerance, RenderPersistentSprites },
                { "persistent-text", textTolerance, RenderPersistentText },
                { "mixed-2d-order", shaderTolerance, RenderMixed2DOrder },
                { "storage-buffer-bindings", shaderTolerance, RenderStorageBufferBindings },
                { "mrt-clear", exact, RenderMrtClear },
                { "integer-clear-draw", exact, RenderIntegerClearDraw },
                { "fullscreen-pattern", shaderTolerance, RenderFullscreenPattern },
                { "texture-mip-selection", shaderTolerance, RenderMipSelection },
                { "depth-output-matrix", exact, RenderDepthOutputMatrix },
                { "post-sharpening", shaderTolerance, RenderPostSharpening },
                { "weighted-oit", shaderTolerance, RenderWeightedOit },
                { "toon-silhouette", toonTolerance, RenderToonSilhouette },
                { "primitive-lit-sphere", decalTolerance, [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error); } },
                { "decal-curved-forward", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Box); } },
                { "decal-curved-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Box, RenderingPath::DeferredPlus);
                  } },
                { "decal-tapered-partial", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::CylinderPartial); } },
                { "decal-tapered-wrap", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::CylinderFull); } },
                { "decal-glass-coating", coatingTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Coating); } },
                { "decal-toon-coating", coatingTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::ToonCoating); } },
                { "decal-toon-coating-deferred", coatingTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::ToonCoating, RenderingPath::DeferredPlus);
                  } },
                { "decal-glass-coating-core", coatingCoreTolerance,
                  [](Image& image, String& error) { return RenderCoatingCore(image, error, DecalAcceptance::Coating); } },
                { "decal-toon-coating-core", coatingCoreTolerance,
                  [](Image& image, String& error) { return RenderCoatingCore(image, error, DecalAcceptance::ToonCoating); } },
                { "decal-toon-coating-core-deferred", coatingCoreTolerance,
                  [](Image& image, String& error) {
                      return RenderCoatingCore(image, error, DecalAcceptance::ToonCoating, RenderingPath::DeferredPlus);
                  } },
                { "decal-toon", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Toon); } },
                { "decal-toon-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Toon, RenderingPath::DeferredPlus);
                  } },
                { "decal-masked-correction", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Correction); } },
                { "decal-masked-correction-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Correction, RenderingPath::DeferredPlus);
                  } },
                { "decal-wetness", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Wetness); } },
                { "decal-wetness-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Wetness, RenderingPath::DeferredPlus);
                  } },
                { "decal-normal-detail", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::NormalDetail); } },
                { "decal-normal-detail-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::NormalDetail, RenderingPath::DeferredPlus);
                  } },
                { "decal-unlit", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Unlit); } },
                { "decal-unlit-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Unlit, RenderingPath::DeferredPlus);
                  } },
                { "decal-benchmark", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Benchmark); } },
                { "decal-camera-inside", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::CameraInside); } },
                { "decal-camera-inside-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::CameraInside, RenderingPath::DeferredPlus);
                  } },
                { "decal-ordered-overlap", decalTolerance,
                  [](Image& image, String& error) { return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Overlap); } },
                { "decal-ordered-overlap-deferred", decalTolerance,
                  [](Image& image, String& error) {
                      return RenderPrimitiveLitSphere(image, error, DecalAcceptance::Overlap, RenderingPath::DeferredPlus);
                  } },
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
            else if (argument == "--benchmark-sprites" || argument == "--benchmark-sprites-smoke")
            {
                options.BenchmarkSprites = true;
                options.BenchmarkSpritesSmoke = argument == "--benchmark-sprites-smoke";
            }
            else if (argument == "--benchmark-no-cache")
                options.BenchmarkNoCache = true;
            else if (argument == "--benchmark-legacy-textures")
                options.BenchmarkLegacyTextures = true;
            else if (argument == "--benchmark-serial-writes")
                options.BenchmarkSerialWrites = true;
            else if (argument == "--validate-importers")
                options.ValidateImporters = true;
            else if (argument == "--benchmark-texture-normal")
                options.BenchmarkTextureNormal = true;
            else if (argument == "--benchmark-texture")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.BenchmarkTexture = value;
            }
            else if (argument == "--procedural-package" || argument == "--procedural-reference")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                (argument == "--procedural-package" ? options.ProceduralPackage : options.ProceduralReference) = value;
            }
            else if (argument == "--procedural-preview")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.ProceduralPreview = value;
            }
            else if (argument == "--osl-package")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.OslPackage = value;
            }
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
            else if (argument == "--export-decal-scenes")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.DecalShowcase = value;
            }
            else if (argument == "--benchmark-import")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                options.BenchmarkImport = value;
            }
            else if (argument == "--benchmark-project" || argument == "--benchmark-scene")
            {
                const char* value = readValue(argument);
                if (!value)
                    return false;
                (argument == "--benchmark-project" ? options.BenchmarkProject : options.BenchmarkScene) = value;
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
        if (options.BenchmarkProject.empty() != options.BenchmarkScene.empty())
        {
            error = "--benchmark-project and --benchmark-scene must be provided together";
            return false;
        }
        return true;
    }

    void PrintUsage()
    {
        std::cout << "Crowny renderer regression tests\n\n"
                  << "  --backend vulkan|opengl   Select the renderer backend\n"
                  << "  --references PATH        Golden BMP directory\n"
                  << "  --artifacts PATH         Actual, expected, diff, and JSON output\n"
                  << "  --export-decal-scenes PATH  Export decal source scenes to an empty editor project\n"
                  << "  --filter TEXT            Run cases whose names contain TEXT\n"
                  << "  --validate-importers     Check texture and model import contracts\n"
                  << "  --osl-package PATH       Compare GPU OSL evaluation with an offline CPU reference\n"
                  << "  --procedural-package PATH --procedural-reference PATH\n"
                  << "                           Compare live PBR procedural shading with CPU texture results\n"
                  << "  --procedural-preview PATH Capture a procedural package on a flat plane at its default settings\n"
                  << "  --benchmark-sprites    100,000 moving ECS sprites, 300 warm-up + 1,800 measured frames\n"
                  << "  --benchmark-sprites-smoke    Same workload, 5 warm-up + 10 measured frames; not an acceptance run\n"
                  << "  --benchmark-texture PATH [--benchmark-texture-normal]\n"
                  << "                           Compare texture compression time and reconstruction error\n"
                  << "  --benchmark-project PATH --benchmark-scene PATH\n"
                  << "                           Time cached scene/dependency loading without editing the project\n"
                  << "  --benchmark-import PATH  Import using the adjacent .meta options and save into artifacts\n"
                  << "  --benchmark-no-cache --benchmark-serial-writes\n"
                  << "                           Measure the uncached, sequential control path\n"
                  << "  --benchmark-legacy-textures\n"
                  << "                           Compare the previous model texture compression policy\n"
                  << "  --update-references      Replace goldens with this backend's output\n";
    }

    int RunSuite(const RunnerOptions& options)
    {
        const Path backendArtifacts = options.Artifacts / BackendName(options.Backend);
        fs::create_directories(backendArtifacts);
        if (!options.ProceduralPreview.empty())
            return CaptureProceduralPlane(options.ProceduralPreview, backendArtifacts / "procedural-preview");
        if (!options.ProceduralPackage.empty())
            return RunProceduralMaterialTest(options.ProceduralPackage, options.ProceduralReference, backendArtifacts / "procedural");
        if (!options.OslPackage.empty())
            return RunOslTextureTest(options.OslPackage, backendArtifacts / "osl");
        if (options.BenchmarkSprites)
            return RunSprite2DBenchmark(backendArtifacts, options.BenchmarkSpritesSmoke);
        if (!options.BenchmarkTexture.empty())
        {
            const auto image = ImageLoader::Load(ImageLoadRequest::FromFile(options.BenchmarkTexture));
            if (!image || !image.Pixels)
                return 1;
            auto rgba = PixelData::Create(image.Info.Width, image.Info.Height, 1, TextureFormat::RGBA8);
            if (!PixelUtils::ConvertPixels(*image.Pixels, *rgba))
                return 1;
            BasisTextureSource source;
            source.Subresources = { rgba };
            std::ofstream report(backendArtifacts / "texture-benchmark.csv");
            report << "format,effort,encode_ms,bytes,rgb_psnr_db,normal_mean_degrees,normal_max_degrees,alpha_max_error,cutout_mismatches\n";
            const std::pair<TextureDiskFormat, uint32_t> settings[] = { { TextureDiskFormat::UASTC, 2 }, { TextureDiskFormat::ETC1S, 192 },
                                                                        { TextureDiskFormat::UASTC, 0 }, { TextureDiskFormat::UASTC, 1 },
                                                                        { TextureDiskFormat::UASTC, 2 }, { TextureDiskFormat::ETC1S, 192 },
                                                                        { TextureDiskFormat::UASTC, 0 }, { TextureDiskFormat::UASTC, 1 } };
            for (const auto& [diskFormat, effort] : settings)
            {
                source.UASTCEffort = effort;
                Vector<uint8_t> encoded;
                String error;
                const auto start = std::chrono::steady_clock::now();
                if (!BasisTextureCodec::Encode(source, diskFormat, !options.BenchmarkTextureNormal, encoded, nullptr, &error))
                {
                    std::cerr << error << std::endl;
                    return 1;
                }
                const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                BasisTextureTranscodeResult decoded;
                if (!BasisTextureCodec::Transcode(encoded.data(), encoded.size(), TextureFormat::RGBA8, TextureFormat::RGBA8, 1, decoded, &error))
                    return 1;
                double squaredError = 0.0, angleSum = 0.0, angleMax = 0.0, alphaMax = 0.0;
                uint64_t cutoutMismatches = 0;
                Image preview(image.Info.Width, image.Info.Height);
                for (uint32_t y = 0; y < image.Info.Height; ++y)
                    for (uint32_t x = 0; x < image.Info.Width; ++x)
                    {
                        glm::vec4 original, restored;
                        if (!rgba->TryGetColorAt(x, y, 0, original) || !decoded.Subresources.front().Pixels->TryGetColorAt(x, y, 0, restored))
                            return 1;
                        const glm::vec3 difference = glm::vec3(original - restored);
                        squaredError += glm::dot(difference, difference);
                        alphaMax = std::max(alphaMax, static_cast<double>(std::abs(original.a - restored.a)));
                        cutoutMismatches += (original.a >= 0.5f) != (restored.a >= 0.5f);
                        if (options.BenchmarkTextureNormal)
                        {
                            const auto normal = [](glm::vec4 color) {
                                const glm::vec3 value = glm::vec3(color) * 2.0f - 1.0f;
                                return glm::dot(value, value) > 1e-10f ? glm::normalize(value) : glm::vec3(0, 0, 1);
                            };
                            const double angle = glm::degrees(std::acos(glm::clamp(glm::dot(normal(original), normal(restored)), -1.0f, 1.0f)));
                            angleSum += angle;
                            angleMax = std::max(angleMax, angle);
                        }
                        std::memcpy(preview.Pixel(x, y),
                                    decoded.Subresources.front().Pixels->GetData() + y * decoded.Subresources.front().Pixels->GetRowPitch() + x * 4u,
                                    4);
                    }
                const double pixels = static_cast<double>(image.Info.Width) * image.Info.Height;
                const double psnr = squaredError > 0 ? 10.0 * std::log10(3.0 * pixels / squaredError) : 999.0;
                const String formatName = diskFormat == TextureDiskFormat::ETC1S ? "etc1s" : "uastc";
                report << formatName << ',' << effort << ',' << elapsed << ',' << encoded.size() << ',' << psnr << ',' << angleSum / pixels << ','
                       << angleMax << ',' << alphaMax << ',' << cutoutMismatches << '\n';
                if (!SaveBmp(backendArtifacts / (formatName + "-" + std::to_string(effort) + ".bmp"), preview, error))
                    return 1;
            }
            return report ? 0 : 1;
        }
        if (options.ValidateImporters)
        {
            String error;
            const bool passed =
              CheckTextureImportCache(error) && CheckModelAlphaCache(error) && CheckModelTextureBatch(error) && CheckModelMaterialImport(error);
            std::cout << (passed ? "Importer validation passed" : "Importer validation failed: " + error) << std::endl;
            return passed ? 0 : 1;
        }
        if (!options.BenchmarkImport.empty())
        {
            using Clock = std::chrono::steady_clock;
            if (options.BenchmarkNoCache)
                Application::Get().SetInternalDirectory({});
            // Editor reimports retain the loaded built-in shader identity. Keep it
            // stable across separate benchmark processes as well.
            const auto importManifest = CreateRef<AssetManifest>("Import benchmark");
            importManifest->RegisterAsset(UUID(0xb1570001u, 0xcaca0001u, 0xfbf00002u, 1u), Path(PBRIBL_SHADER_PATH));
            AssetManager::Get().RegisterAssetManifest(importManifest);
            const auto importOptions = ImportOptionsSerializer::Deserialize(YAML::LoadFile(options.BenchmarkImport.string() + ".meta"));
            if (!importOptions)
                return 1;
            if (options.BenchmarkLegacyTextures && importOptions->GetImportOptionsType() == ImportOptionsType::Mesh)
                StaticRefCast<MeshImportOptions>(importOptions)->FastTextureCompression = false;
            const auto start = Clock::now();
            auto assets = Importer::Get().ImportAll(options.BenchmarkImport, importOptions);
            const auto imported = Clock::now();
            if (assets.empty())
                return 1;
            std::cout << "Import prepared " << assets.size() << " assets in " << std::chrono::duration<double>(imported - start).count() << " s"
                      << std::endl;
            // Exercise the same dependent assignment and atomic cache writes as ProjectLibrary,
            // with temporary IDs and paths so benchmarking never modifies the source project.
            Vector<UUID> ids(assets.size());
            Vector<std::pair<Ref<Asset>, UUID>> referenceAssignments;
            for (size_t index = 0; index < assets.size(); ++index)
            {
                // Stable temporary IDs let rerunning an artifact directory exercise reimport.
                ids[index] = UUID(0xb1570001u, 0xcaca0001u, 0xfbf00001u, static_cast<uint32_t>(index + 1));
                referenceAssignments.emplace_back(assets[index], ids[index]);
                if (options.BenchmarkSerialWrites)
                    for (const auto& asset : assets)
                        asset->OnDependentAssigned(assets[index], ids[index]);
            }
            if (!options.BenchmarkSerialWrites && !Importer::ResolveDependencies(referenceAssignments))
                return 1;
            const auto assigned = Clock::now();
            uint64_t bytes = 0;
            Vector<std::pair<Ref<Asset>, Path>> cacheWrites;
            for (size_t index = 0; index < assets.size(); ++index)
            {
                const auto path = backendArtifacts / "cache" / (std::to_string(index) + ".asset");
                cacheWrites.emplace_back(assets[index], path);
            }
            if (options.BenchmarkSerialWrites)
            {
                for (const auto& [asset, path] : cacheWrites)
                    if (!AssetManager::Get().Save(asset, path))
                        return 1;
            }
            else if (!AssetManager::Get().SaveBatch(cacheWrites))
                return 1;
            const auto saved = Clock::now();
            // Workload verification is outside the timed import/publication interval.
            for (const auto& [asset, path] : cacheWrites)
                bytes += fs::file_size(path);
            UnorderedSet<const Asset*> uniqueTextures;
            uint32_t etc1sTextures = 0, uastcTextures = 0;
            uint64_t runtimeTextureBytes = 0;
            for (const auto& asset : assets)
                if (asset->GetAssetType() == AssetType::Texture && uniqueTextures.insert(asset.get()).second)
                {
                    const auto texture = StaticRefCast<Texture>(asset);
                    const auto format = texture->GetDiskFormat();
                    etc1sTextures += format == TextureDiskFormat::ETC1S;
                    uastcTextures += format == TextureDiskFormat::UASTC;
                    const auto& desc = texture->GetDesc();
                    for (uint32_t mip = 0; mip <= desc.MipLevels; ++mip)
                        runtimeTextureBytes += PixelUtils::GetMemorySize(std::max(1u, desc.Width >> mip), std::max(1u, desc.Height >> mip),
                                                                         std::max(1u, desc.Depth >> mip), desc.Format) *
                                               desc.Faces;
                }
            const auto milliseconds = [](auto begin, auto end) { return std::chrono::duration<double, std::milli>(end - begin).count(); };
            std::ofstream report(backendArtifacts / "import-benchmark.json");
            report << std::fixed << std::setprecision(3) << "{\n  \"texture_cache\": " << (options.BenchmarkNoCache ? "false" : "true")
                   << ",\n  \"parallel_writes\": " << (options.BenchmarkSerialWrites ? "false" : "true") << ",\n  \"fast_texture_compression\": "
                   << (importOptions->GetImportOptionsType() == ImportOptionsType::Mesh
                         ? (StaticRefCast<MeshImportOptions>(importOptions)->FastTextureCompression ? "true" : "false")
                         : "null")
                   << ",\n  \"import_ms\": " << milliseconds(start, imported) << ",\n  \"assignment_ms\": " << milliseconds(imported, assigned)
                   << ",\n  \"save_ms\": " << milliseconds(assigned, saved) << ",\n  \"total_ms\": " << milliseconds(start, saved)
                   << ",\n  \"assets\": " << assets.size() << ",\n  \"bytes\": " << bytes << ",\n  \"etc1s_textures\": " << etc1sTextures
                   << ",\n  \"uastc_textures\": " << uastcTextures << ",\n  \"runtime_texture_bytes\": " << runtimeTextureBytes << "\n}\n";
            std::cout << "Import and cache write: " << milliseconds(start, saved) << " ms, " << bytes << " bytes" << std::endl;
            return report ? 0 : 1;
        }
        if (!options.BenchmarkProject.empty())
        {
            using Clock = std::chrono::steady_clock;
            const auto start = Clock::now();
            const auto manifest = AssetManifest::Deserialize(options.BenchmarkProject / "Internal/AssetManifest.yaml", options.BenchmarkProject);
            if (!manifest)
                return 1;
            AssetManager::Get().RegisterAssetManifest(manifest);
            const auto manifestLoaded = Clock::now();
            const Ref<Scene> scene = CreateRef<Scene>("Load benchmark");
            SceneSerializer serializer(scene);
            const Path scenePath = options.BenchmarkScene.is_absolute() ? options.BenchmarkScene : options.BenchmarkProject / options.BenchmarkScene;
            if (!serializer.Deserialize(scenePath))
                return 1;
            const auto sceneLoaded = Clock::now();
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            const auto uploaded = Clock::now();
            uint64_t vertices = 0, indices = 0, materials = 0;
            for (const auto entity : scene->GetAllEntitiesWith<MeshRendererComponent>())
            {
                const auto& mesh = scene->GetAllEntitiesWith<MeshRendererComponent>().get<MeshRendererComponent>(entity);
                if (!mesh.MeshHandle)
                    return 1;
                vertices += mesh.MeshHandle->GetVertexCount();
                indices += mesh.MeshHandle->GetIndexCount();
                for (const auto& material : mesh.Materials)
                {
                    if (!material)
                        return 1;
                    ++materials;
                }
            }
            const auto milliseconds = [](auto begin, auto end) { return std::chrono::duration<double, std::milli>(end - begin).count(); };
            std::ofstream report(backendArtifacts / "load-benchmark.json");
            report << std::fixed << std::setprecision(3) << "{\n  \"manifest_ms\": " << milliseconds(start, manifestLoaded)
                   << ",\n  \"scene_and_assets_ms\": " << milliseconds(manifestLoaded, sceneLoaded)
                   << ",\n  \"submit_ms\": " << milliseconds(sceneLoaded, uploaded) << ",\n  \"total_ms\": " << milliseconds(start, uploaded)
                   << ",\n  \"vertices\": " << vertices << ",\n  \"indices\": " << indices << ",\n  \"material_slots\": " << materials << "\n}\n";
            std::cout << "Cached scene and GPU upload: " << milliseconds(start, uploaded) << " ms, " << vertices << " vertices, " << materials
                      << " material slots\n";
            return report ? 0 : 1;
        }
        const Vector<TestCase> cases = BuildCases();
        if (!options.DecalShowcase.empty())
            s_DecalShowcase = CreateScope<DecalShowcaseWriter>(options.DecalShowcase);
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
            s_CurrentTestName = test.Name;
            if (!test.Render(actual, result.Message))
            {
                if (actual.IsValid())
                {
                    String captureError;
                    SaveBmp(backendArtifacts / (test.Name + ".actual.bmp"), actual, captureError);
                }
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
            const auto test = std::find_if(cases.begin(), cases.end(), [&](const TestCase& candidate) { return candidate.Name == testName; });
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
