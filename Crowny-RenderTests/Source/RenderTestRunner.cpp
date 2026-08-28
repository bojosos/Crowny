#include "RenderTestRunner.h"

#include "RenderTestImage.h"

#include "Crowny/Common/Constants.h"
#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Utils/PixelUtils.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>

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

        Ref<Texture> CreateRenderTexture(uint32_t width, uint32_t height, TextureFormat format, StringView debugName)
        {
            TextureDesc textureDesc;
            textureDesc.Width = width;
            textureDesc.Height = height;
            textureDesc.Format = format;
            textureDesc.Usage = PixelUtils::IsDepthFormat(format) ? TextureUsage::TEXTURE_DEPTHSTENCIL : TextureUsage::TEXTURE_RENDERTARGET;
            textureDesc.sRGB = false;
            textureDesc.ReadWrite = true;
            textureDesc.DebugName = String(debugName);
            return Texture::Create(textureDesc);
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
    gl_Position = vec4(uv * 2.0 - 1.0, 0.75, 1.0);
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

        Vector<TestCase> BuildCases()
        {
            const Tolerance exact{};
            Tolerance shaderTolerance;
            shaderTolerance.PixelThreshold = 1u;
            shaderTolerance.MaxChannelError = 1u;
            shaderTolerance.MaxMeanAbsoluteError = 0.01;
            shaderTolerance.MaxFailingPixelRatio = 0.001;
            return {
                { "solid-clear", exact, RenderSolidClear },
                { "mrt-clear", exact, RenderMrtClear },
                { "fullscreen-pattern", shaderTolerance, RenderFullscreenPattern },
                { "texture-mip-selection", shaderTolerance, RenderMipSelection },
                { "depth-output-matrix", exact, RenderDepthOutputMatrix },
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
        const Tolerance tolerance{ 1u, 1u, 0.01, 0.001, true };
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
