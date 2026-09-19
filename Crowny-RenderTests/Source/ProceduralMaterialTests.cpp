#include "ProceduralMaterialTests.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/ProceduralMaterialProgram.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/Renderer.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/SceneCamera.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Serialization/FileEncoder.h"
#include "Crowny/Serialization/MaterialSerializer.h"
#include "RenderTestImage.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Crowny::RenderTests
{
    namespace
    {
        constexpr uint32_t Width = 65, Height = 37, Count = Width * Height;

        Vector<uint8_t> ReadBytes(const Path& path)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input || input.tellg() <= 0 || input.tellg() > 64 * 1024 * 1024)
                throw std::runtime_error("Missing or oversized procedural artifact: " + path.string());
            Vector<uint8_t> bytes(static_cast<size_t>(input.tellg()));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
                throw std::runtime_error("Cannot read procedural artifact: " + path.string());
            return bytes;
        }

        void Load(ProceduralMaterialProgram& program, const Path& path)
        {
            String error;
            if (!program.Initialize(path, error))
                throw std::runtime_error(error);
        }

        AssetHandle<Texture> MakeImageInput(uint32_t seed)
        {
            TextureDesc desc;
            desc.Width = desc.Height = 32;
            desc.MipLevels = 5;
            desc.Format = TextureFormat::RGBA8;
            desc.sRGB = false;
            const auto texture = Texture::Create(desc);
            for (uint32_t mip = 0; mip <= desc.MipLevels; ++mip)
            {
                const auto pixels = texture->AllocatePixelData(0, mip);
                for (uint32_t y = 0; y < pixels->GetHeight(); ++y)
                    for (uint32_t x = 0; x < pixels->GetWidth(); ++x)
                        pixels->SetColorAt(x, y,
                                           glm::vec4(float((x * 17 + seed * 53) % 256) / 255, float((y * 31 + mip * 71) % 256) / 255,
                                                     float((x * 13 + y * 7 + seed * 23 + mip * 41) % 256) / 255,
                                                     0.25f + 0.75f * float((x + y + seed) % 8) / 7));
                texture->WriteData(*pixels, mip);
            }
            return static_asset_cast<Texture>(AssetManager::Get().CreateAssetHandle(texture));
        }

        Ref<Texture> MakeTexture(TextureFormat format, TextureUsage usage, const Vector<glm::vec4>* values = nullptr, uint32_t width = Width,
                                 uint32_t height = Height)
        {
            TextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = format;
            desc.Usage = usage;
            desc.sRGB = false;
            desc.ReadWrite = true;
            auto texture = Texture::Create(desc);
            if (values)
            {
                PixelData pixels(width, height, 1, format);
                pixels.AllocateInternalBuffer();
                std::memcpy(pixels.GetData(), values->data(), values->size() * sizeof(glm::vec4));
                texture->WriteData(pixels);
            }
            return texture;
        }

        Vector<glm::vec4> Draw(const Ref<Material>& material, const AssetHandle<Mesh>& mesh, const Ref<RenderTexture>& target,
                               const Ref<Texture>& color, const Path& previewPath)
        {
            const uint32_t width = color->GetWidth(), height = color->GetHeight(), count = width * height;
            auto& api = RenderAPI::Get();
            api.SetRenderTarget(target);
            api.SetViewport(0, 0, 1, 1);
            api.ClearViewport(FBT_COLOR | FBT_DEPTH, glm::vec4(0.125f, 0.0f, 0.25f, 0.0f), 0.0f);
            ForwardRenderer::BeginForwardOnlyScene(glm::mat4(1), glm::mat4(1), glm::vec3(0, 0, 3), nullptr);
            RenderLightData light{};
            light.DirectionOuterCosine = glm::vec4(0, 0, -1, 0);
            light.ColorIntensity = glm::vec4(1, 1, 1, 3);
            light.Metadata.x = static_cast<uint32_t>(LightType::Directional);
            light.Metadata.y = static_cast<uint32_t>(RenderLightFlags::Enabled | RenderLightFlags::AffectDiffuse | RenderLightFlags::AffectSpecular);
            ForwardRenderer::SetLights(&light, 1);
            const auto handle = static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(material));
            const Array<AssetHandle<Material>, 1> materials{ handle };
            ForwardRenderer::SubmitForwardOnlyOpaque(mesh, materials, glm::mat4(1));
            api.SetRenderTarget(nullptr);
            api.SubmitCommandBuffer(nullptr);
            PixelData pixels(width, height, 1, TextureFormat::RGBA32F);
            pixels.AllocateInternalBuffer();
            color->ReadData(pixels);
            Vector<glm::vec4> values(count);
            std::memcpy(values.data(), pixels.GetData(), values.size() * sizeof(glm::vec4));
            Image preview(width, height);
            for (uint32_t i = 0; i < count; ++i)
                for (uint32_t c = 0; c < 4; ++c)
                    preview.Pixel(i % width, i / width)[c] = static_cast<uint8_t>(glm::clamp(values[i][c], 0.0f, 1.0f) * 255 + 0.5f);
            String error;
            if (!SaveBmp(previewPath, preview, error))
                throw std::runtime_error(error);
            return values;
        }

        void DrawScene(const Ref<Material>& first, const Ref<Material>& second, const Path& path)
        {
            constexpr uint32_t width = 512, height = 256;
            TextureDesc desc;
            desc.Width = width;
            desc.Height = height;
            desc.Format = TextureFormat::RGBA8;
            desc.Usage = TextureUsage::TEXTURE_RENDERTARGET;
            desc.sRGB = false;
            const auto color = Texture::Create(desc);
            desc.Format = TextureFormat::DEPTH32F;
            desc.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            const auto depth = Texture::Create(desc);
            RenderTextureDesc targetDesc;
            targetDesc.Width = width;
            targetDesc.Height = height;
            targetDesc.ColorSurfaces[0].Texture = color;
            targetDesc.DepthSurface.Texture = depth;
            const auto target = RenderTexture::Create(targetDesc);
            const auto scene = CreateRef<Scene>("Live procedural material test");
            const auto mesh = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(MeshFactory::CreateSphere(0.65f, 64, 32)));
            for (uint32_t i = 0; i < 2; ++i)
            {
                auto entity = scene->CreateEntity(i == 0 ? "First instance" : "Second instance");
                entity.GetTransform().SetPosition(glm::vec3(i == 0 ? -0.8f : 0.8f, 0, 0));
                auto& renderer = entity.AddComponent<MeshRendererComponent>();
                renderer.MeshHandle = mesh;
                renderer.SetMaterial(0, static_asset_cast<Material>(AssetManager::Get().CreateAssetHandle(i == 0 ? first : second)));
                renderer.CastShadows = false;
                renderer.MotionVectors = false;
            }
            auto lightEntity = scene->CreateEntity("Directional light");
            auto& light = lightEntity.AddComponent<LightComponent>();
            light.Type = LightType::Directional;
            light.Intensity = 3;
            lightEntity.GetTransform().SetRotation(glm::quat(glm::radians(glm::vec3(-25, -20, 0))));
            SceneCamera camera;
            camera.SetPerspective(glm::radians(40.0f), 0.1f, 20.0f);
            camera.SetViewportSize(width, height);
            camera.SetBackgroundColor(glm::vec3(0));
            camera.SetOcclusionCulling(false);
            const auto view = glm::lookAt(glm::vec3(0, 0, 3), glm::vec3(0), glm::vec3(0, 1, 0));
            SceneRenderer renderer(scene, target);
            renderer.Init();
            auto settings = renderer.GetRenderPipelineSettings();
            settings.EnableTaa = false;
            settings.EnableBloom = false;
            settings.EnableGtao = false;
            renderer.SetRenderPipelineSettings(settings);
            auto* renderThread = Application::Get().GetRenderThread();
            if (!renderThread)
                throw std::runtime_error("Procedural scene test requires the Vulkan render thread");
            auto& snapshot = renderThread->BeginFrame();
            snapshot.FrameNumber = 1;
            renderer.ExtractSnapshot(snapshot, camera, view, false);
            renderThread->SubmitFrame();
            renderThread->WaitForFrameDone();
            RenderAPI::Get().SubmitCommandBuffer(nullptr);
            if (!Renderer::GetGpuScene().HasForwardOnlyOpaqueMaterials() || !SceneRenderer::GetStatistics().RenderGraphSucceeded)
                throw std::runtime_error("Scene renderer did not route the procedural materials through its custom opaque pass");
            PixelData pixels(width, height, 1, TextureFormat::RGBA8);
            pixels.AllocateInternalBuffer();
            color->ReadData(pixels);
            Image image(width, height);
            uint32_t covered[2]{};
            for (uint32_t y = 0; y < height; ++y)
                for (uint32_t x = 0; x < width; ++x)
                {
                    const auto value = pixels.GetColorAt(x, y);
                    if (std::max({ value.r, value.g, value.b }) > 0.08f)
                        ++covered[x < width / 2 ? 0 : 1];
                    for (uint32_t c = 0; c < 4; ++c)
                        image.Pixel(x, height - 1 - y)[c] = static_cast<uint8_t>(glm::clamp(value[c], 0.0f, 1.0f) * 255 + 0.5f);
                }
            String error;
            if (!SaveBmp(path, image, error))
                throw std::runtime_error(error);
            if (covered[0] < 1000 || covered[1] < 1000)
                throw std::runtime_error("Procedural instances are missing from the scene render");
        }
    } // namespace

    int CaptureProceduralPlane(const Path& package, const Path& artifacts)
    {
        try
        {
            struct ForwardScope
            {
                ForwardScope()
                {
                    Renderer2D::Init();
                    ForwardRenderer::Init();
                }
                ~ForwardScope()
                {
                    Renderer2D::Shutdown();
                    ForwardRenderer::Shutdown();
                }
            } forwardScope;
            fs::create_directories(artifacts);
            ProceduralMaterialProgram program;
            Load(program, package);
            const auto material = program.CreateMaterial();
            if (!material)
                throw std::runtime_error("Cannot create procedural preview material");
            material->SetFloat("roughness", 1.0f);
            material->SetFloat("useIBL", 0.0f);
            // Face-on plane, UVs spanning [0, 1]. The 7:4 aspect preserves the
            // Chaos Mandelbrot example's 3.5 by 2 complex-coordinate domain.
            constexpr uint32_t width = 1400, height = 800;
            const auto meshData = MeshFactory::CreateQuadData(2, 2, glm::vec3(0, 0, 1));
            meshData->SetPositions({ { -1, -1, 0.5f }, { 1, -1, 0.5f }, { -1, 1, 0.5f }, { 1, 1, 0.5f } });
            meshData->SetUVs(0, { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } });
            meshData->SetIndices({ 0, 1, 2, 2, 1, 3 });
            MeshDesc meshDesc;
            meshDesc.Data = meshData;
            meshDesc.SubMeshes.emplace_back(0, meshData->GetIndexCount(), DrawMode::TRIANGLE_LIST);
            const auto mesh = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(Mesh::Create(meshDesc)));
            const auto color = MakeTexture(TextureFormat::RGBA32F, TextureUsage::TEXTURE_RENDERTARGET, nullptr, width, height);
            const auto depth = MakeTexture(TextureFormat::DEPTH32F, TextureUsage::TEXTURE_DEPTHSTENCIL, nullptr, width, height);
            RenderTextureDesc targetDesc;
            targetDesc.Width = width;
            targetDesc.Height = height;
            targetDesc.ColorSurfaces[0].Texture = color;
            targetDesc.DepthSurface.Texture = depth;
            const auto target = RenderTexture::Create(targetDesc);
            const auto pixels = Draw(material, mesh, target, color, artifacts / "plane.bmp");
            for (const auto& pixel : pixels)
                if (!std::isfinite(pixel.r) || !std::isfinite(pixel.g) || !std::isfinite(pixel.b) || pixel.a < 0.99f)
                    throw std::runtime_error("Procedural preview contains invalid or uncovered pixels");
            std::cout << "Captured live procedural plane: " << (artifacts / "plane.bmp") << '\n';
            return 0;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Procedural preview failed: " << exception.what() << '\n';
            return 1;
        }
    }

    int RunProceduralMaterialTest(const Path& package, const Path& referencePackage, const Path& artifacts)
    {
        try
        {
            struct ForwardScope
            {
                ForwardScope()
                {
                    Renderer2D::Init();
                    ForwardRenderer::Init();
                }
                ~ForwardScope()
                {
                    Renderer2D::Shutdown();
                    ForwardRenderer::Shutdown();
                }
            } forwardScope;
            fs::create_directories(artifacts);
            ProceduralMaterialProgram program, oracle;
            Load(program, package);
            Load(oracle, referencePackage);
            const auto material = program.CreateMaterial();
            const auto second = program.CreateMaterial();
            const auto reference = oracle.CreateMaterial();
            if (!material || !second || !reference || material->GetGraphicsPipeline() != second->GetGraphicsPipeline() ||
                material->GetUniformParams() == second->GetUniformParams() || !MaterialRenderClassifier::Classify(*material).IsForwardOnlyOpaque())
                throw std::runtime_error("Procedural material sharing or routing is incorrect");
            String error;
            if (program.Initialize({}, {}, {}, error) || !program.CreateMaterial())
                throw std::runtime_error("Failed reload did not preserve the previous procedural program");

            const bool checker = material->HasBinding("checkerFrequency");
            const bool textured = material->GetTextureDescriptors().contains("osl_image");
            // Separately compiled texture coordinates/gradients can straddle a
            // hardware filtering precision step. Keep the arithmetic-only cases
            // stricter; image comparisons still require sub-8-bit-channel error.
            const float tolerance = textured ? 0.0005f : 0.0002f;
            AssetHandle<Texture> imageInput, detailInput;
            if (textured)
            {
                imageInput = MakeImageInput(1);
                detailInput = MakeImageInput(3);
                material->SetTexture("osl_image", imageInput);
                material->SetTexture("osl_detail", detailInput);
                second->SetTexture("osl_image", detailInput);
                second->SetTexture("osl_detail", imageInput);
            }
            YAML::Node parameters;
            Vector<uint8_t> parameterFrames;
            if (std::filesystem::exists(package / "material.parameters.json"))
            {
                parameters = YAML::LoadFile((package / "material.parameters.json").string())["Parameters"];
                if (parameters.size())
                    parameterFrames = ReadBytes(package / "surface-parameters.bin");
                if (parameterFrames.size() != parameters.size() * 16 * 3)
                    throw std::runtime_error("Unexpected edited OSL parameter fixture size");
                // Exercise the same compiled shader asset encoding used by project import.
                const Path encoded = artifacts / "shader.asset";
                FileEncoder<Asset, SerializerType::Binary>(encoded).Encode(material->GetShader().GetInternalPtr());
                const auto restored = DynamicRefCast<Shader>(FileDecoder<Asset, SerializerType::Binary>(encoded).Decode());
                if (!restored)
                    throw std::runtime_error("OSL shader asset did not round-trip");
                const auto restoredHandle = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(restored));
                const auto restoredMaterial = Material::Create(restoredHandle);
                if (textured && (!restoredMaterial->GetTextureDescriptors().contains("osl_image") ||
                                 restoredMaterial->GetAnnotations().at("osl_image").DisplayName != "Image" ||
                                 restoredMaterial->GetAnnotations().at("osl_image").DefaultValueStr != "black"))
                    throw std::runtime_error("OSL texture reflection/defaults did not survive shader serialization");
                for (const auto& parameter : parameters)
                {
                    const auto name = parameter["Name"].as<String>();
                    if (!restoredMaterial->HasBinding(name) ||
                        restoredMaterial->GetAnnotations().find(name) == restoredMaterial->GetAnnotations().end())
                        throw std::runtime_error("OSL parameter reflection/annotations did not survive asset serialization");
                    if (parameter["Type"].as<String>() == "float" &&
                        restoredMaterial->GetDataParam<float>(name) != parameter["Default"][0].as<float>())
                        throw std::runtime_error("OSL input defaults did not survive asset serialization");
                }
            }
            Vector<glm::vec4> cpu(Count * 3);
            if (!checker && !textured)
            {
                const auto bytes = ReadBytes(package / "surface-reference.bin");
                if (bytes.size() != cpu.size() * sizeof(glm::vec4))
                    throw std::runtime_error("Unexpected CPU surface reference size");
                std::memcpy(cpu.data(), bytes.data(), bytes.size());
            }
            // Explicit streams make UVs increase along Vulkan framebuffer x/y.
            const auto meshData = MeshFactory::CreateQuadData(2, 2, glm::vec3(0, 0, 1));
            meshData->SetPositions({ { -1, -1, 0.5f }, { 1, -1, 0.5f }, { -1, 1, 0.5f }, { 1, 1, 0.5f } });
            meshData->SetUVs(0, { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } });
            meshData->SetIndices({ 0, 1, 2, 2, 1, 3 });
            MeshDesc meshDesc;
            meshDesc.Data = meshData;
            meshDesc.SubMeshes.emplace_back(0, meshData->GetIndexCount(), DrawMode::TRIANGLE_LIST);
            const auto mesh = static_asset_cast<Mesh>(AssetManager::Get().CreateAssetHandle(Mesh::Create(meshDesc)));
            const auto color = MakeTexture(TextureFormat::RGBA32F, TextureUsage::TEXTURE_RENDERTARGET);
            const auto depth = MakeTexture(TextureFormat::DEPTH32F, TextureUsage::TEXTURE_DEPTHSTENCIL);
            RenderTextureDesc targetDesc;
            targetDesc.Width = Width;
            targetDesc.Height = Height;
            targetDesc.ColorSurfaces[0].Texture = color;
            targetDesc.DepthSurface.Texture = depth;
            const auto target = RenderTexture::Create(targetDesc);
            float maxError = 0;
            uint32_t failures = 0, changed = 0;
            Vector<glm::vec4> previous;
            for (uint32_t frame = 0; frame < 3; ++frame)
            {
                const auto instance = frame == 1 ? second : material;
                const float time = frame * 0.73f;
                const float frequency = 3.3f + frame * 1.1f;
                const glm::vec4 colorA(0.04f, 0.25f, 0.8f, 1), colorB(0.9f, 0.5f, 0.08f, 1);
                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    const auto name = parameters[i]["Name"].as<String>();
                    const auto type = parameters[i]["Type"].as<String>();
                    const auto* bytes = parameterFrames.data() + (frame * parameters.size() + i) * 16;
                    if (type == "int")
                    {
                        int value;
                        std::memcpy(&value, bytes, 4);
                        instance->SetInt(name, value);
                    }
                    else if (type == "float")
                    {
                        float value;
                        std::memcpy(&value, bytes, 4);
                        instance->SetFloat(name, value);
                    }
                    else
                    {
                        glm::vec3 value;
                        std::memcpy(&value, bytes, 12);
                        instance->SetVector3(name, value);
                    }
                }
                if (parameters.size())
                {
                    if (frame == 2)
                    {
                        const auto previousPipeline = material->GetGraphicsPipeline();
                        const auto replacement = ProceduralMaterialProgram::LoadShader(package, error);
                        if (!replacement)
                            throw std::runtime_error(error);
                        const auto handle = AssetManager::Get().CreateAssetHandle(replacement, material->GetShader().GetUUID());
                        material->NotifyAssetChanged(handle);
                        second->NotifyAssetChanged(handle);
                        if (material->GetGraphicsPipeline() == previousPipeline || material->GetGraphicsPipeline() != second->GetGraphicsPipeline())
                            throw std::runtime_error("OSL shader reimport did not replace and share the pipeline");
                    }
                    const auto saved = MaterialSerializer(instance).SerializeToString();
                    const auto reloaded = Material::Create(instance->GetShader());
                    if (!MaterialSerializer(reloaded).DeserializeFromString(saved))
                        throw std::runtime_error("Edited OSL material could not be reloaded");
                    if (textured)
                        for (const char* name : { "osl_image", "osl_detail" })
                            if (reloaded->GetTextureHandle(name).GetUUID() != instance->GetTextureHandle(name).GetUUID() ||
                                !reloaded->GetTextureHandle(name).IsLoaded())
                                throw std::runtime_error("OSL texture asset binding was lost during material serialization");
                    for (const auto& parameter : parameters)
                    {
                        const auto name = parameter["Name"].as<String>();
                        const auto type = parameter["Type"].as<String>();
                        if ((type == "int" && reloaded->GetDataParam<int>(name) != instance->GetDataParam<int>(name)) ||
                            (type == "float" && reloaded->GetDataParam<float>(name) != instance->GetDataParam<float>(name)) ||
                            (type != "int" && type != "float" && reloaded->GetDataParam<glm::vec3>(name) != instance->GetDataParam<glm::vec3>(name)))
                            throw std::runtime_error("Edited OSL value was lost during material serialization: " + name);
                    }
                }
                if (checker)
                {
                    instance->SetFloat("checkerFrequency", frequency);
                    instance->SetColor("checkerColorA", colorA);
                    instance->SetColor("checkerColorB", colorB);
                    for (uint32_t y = 0; y < Height; ++y)
                        for (uint32_t x = 0; x < Width; ++x)
                        {
                            const float u = (x + 0.5f) / Width * 3 - 1 + time * 0.1f;
                            const float v = (y + 0.5f) / Height * 2 - 0.5f;
                            const int parity = static_cast<int>(std::floor(u * frequency) + std::floor(v * frequency));
                            cpu[frame * Count + y * Width + x] = parity % 2 != 0 ? colorB : colorA;
                        }
                }
                if (textured)
                {
                    reference->SetFloat("imageScale", instance->GetDataParam<float>("osl_scale"));
                    reference->SetFloat("imageBlend", instance->GetDataParam<float>("osl_blend"));
                    reference->SetTexture("imageMap", frame == 1 ? detailInput : imageInput);
                    reference->SetTexture("detailMap", frame == 1 ? imageInput : detailInput);
                    if (instance->GetTextureHandle("osl_image").GetUUID() != (frame == 1 ? detailInput : imageInput).GetUUID())
                        throw std::runtime_error("OSL texture was lost during shader reimport");
                }
                else
                {
                    const Vector<glm::vec4> frameReference(cpu.begin() + frame * Count, cpu.begin() + (frame + 1) * Count);
                    reference->SetTexture("referenceMap", MakeTexture(TextureFormat::RGBA32F, TextureUsage::TEXTURE_STATIC, &frameReference));
                }
                // Both an image and a procedure contribute to the same base-color input.
                const Vector<glm::vec4> imageValues(Count, glm::vec4(0.65f, 0.8f, 0.95f, 1));
                const auto image = MakeTexture(TextureFormat::RGBA32F, TextureUsage::TEXTURE_STATIC, &imageValues);
                for (const auto& current : { instance, reference })
                {
                    current->SetColor("proceduralUVTransform", glm::vec4(3, 2, -1, -0.5f));
                    current->SetFloat("proceduralTime", time);
                    current->SetFloat("proceduralAmount", frame == 2 ? 0.45f : 1.0f);
                    current->SetColor("albedo", glm::vec4(1, 0.85f, 0.7f, 1));
                    current->SetFloat("roughness", 0.3f + 0.2f * frame);
                    current->SetTexture("albedoMap", image);
                }
                const auto actual = Draw(instance, mesh, target, color, artifacts / ("material-" + std::to_string(frame) + ".bmp"));
                const auto expected = Draw(reference, mesh, target, color, artifacts / ("reference-" + std::to_string(frame) + ".bmp"));
                for (uint32_t i = 0; i < Count; ++i)
                {
                    if (actual[i].a != 1.0f || expected[i].a != 1.0f)
                        throw std::runtime_error("Procedural surface did not cover every pixel");
                    for (uint32_t c = 0; c < 4; ++c)
                    {
                        const float difference = std::abs(actual[i][c] - expected[i][c]);
                        maxError = std::max(maxError, difference);
                        failures += !std::isfinite(actual[i][c]) || !std::isfinite(expected[i][c]) || difference > tolerance;
                        if (!previous.empty() && std::abs(actual[i][c] - previous[i][c]) > 0.001f)
                            ++changed;
                    }
                }
                previous = actual;
            }
            if (material->GetDataParam<float>("proceduralTime") != 1.46f || second->GetDataParam<float>("proceduralTime") != 0.73f)
                throw std::runtime_error("Procedural instance parameters leaked across materials");
            if (textured)
            {
                if (second->GetTextureHandle("osl_image").GetUUID() != detailInput.GetUUID())
                    throw std::runtime_error("OSL texture input leaked across material instances");
                second->SetTexture("osl_image", AssetHandle<Texture>());
                const auto& descriptor = second->GetTextureDescriptors().at("osl_image");
                if (second->GetTexture(descriptor.Set, descriptor.Slot) != Texture::BLACK)
                    throw std::runtime_error("Clearing an OSL texture did not restore its black default");
                second->SetTexture("osl_image", detailInput);
            }
            for (size_t i = 0; i < parameters.size(); ++i)
            {
                const auto name = parameters[i]["Name"].as<String>();
                const auto type = parameters[i]["Type"].as<String>();
                const auto* bytes = parameterFrames.data() + (parameters.size() + i) * 16;
                if (type == "int")
                {
                    int value;
                    std::memcpy(&value, bytes, 4);
                    if (second->GetDataParam<int>(name) != value)
                        throw std::runtime_error("OSL integer input leaked across instances");
                }
                else if (type == "float")
                {
                    float value;
                    std::memcpy(&value, bytes, 4);
                    if (second->GetDataParam<float>(name) != value)
                        throw std::runtime_error("OSL float input leaked across instances");
                }
                else
                {
                    glm::vec3 value;
                    std::memcpy(&value, bytes, 12);
                    if (second->GetDataParam<glm::vec3>(name) != value)
                        throw std::runtime_error("OSL triple input leaked across instances");
                }
            }
            if (changed < Count)
                throw std::runtime_error("Procedural material did not respond to instance edits");
            std::ofstream report(artifacts / "comparison.txt");
            report << "pixels=" << Count * 3 << "\nmax_absolute_error=" << maxError << "\nfailing_components=" << failures
                   << "\nchanged_components=" << changed << "\ntolerance=" << tolerance << '\n';
            if (!report)
                throw std::runtime_error("Cannot write procedural material report");
            std::cout << "Procedural material: " << Count * 3 << " pixels, max error " << maxError << ", " << failures << " failures\n";
            if (!failures)
                DrawScene(material, second, artifacts / "scene.bmp");
            return failures ? 1 : 0;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Procedural material test failed: " << exception.what() << '\n';
            return 1;
        }
    }
} // namespace Crowny::RenderTests
