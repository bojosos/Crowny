#include "cwpch.h"

#include "Crowny/Renderer/ProceduralMaterialProgram.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Utils/ShaderCompiler.h"

#include <cmath>
#include <fstream>

namespace Crowny
{
    namespace
    {
        Vector<uint8_t> ReadArtifact(const Path& path)
        {
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream || stream.tellg() <= 0 || stream.tellg() > 64 * 1024 * 1024)
                throw std::runtime_error("Missing or oversized procedural artifact: " + path.string());
            Vector<uint8_t> bytes(static_cast<size_t>(stream.tellg()));
            stream.seekg(0);
            if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
                throw std::runtime_error("Cannot read procedural artifact: " + path.string());
            return bytes;
        }

        void ApplyParameters(UniformDesc& description, const String& text)
        {
            const auto textureCount = std::count_if(description.Textures.begin(), description.Textures.end(),
                                                    [](const auto& value) { return value.first.starts_with("osl_"); });
            if (text.empty())
            {
                if (description.Uniforms.contains("OslInputs") || textureCount)
                    throw std::runtime_error("OSL material parameter metadata is missing; load the complete package directory");
                return;
            }
            const auto document = YAML::Load(text);
            if (document["Version"].as<int>() != 1 || !document["Parameters"].IsSequence())
                throw std::runtime_error("Unsupported procedural parameter metadata");
            UnorderedSet<String> names;
            for (const auto& parameter : document["Parameters"])
            {
                const auto name = parameter["Name"].as<String>();
                const auto block = description.Uniforms.find(parameter["Block"].as<String>());
                if (!names.insert(name).second || block == description.Uniforms.end() || block->second.Set != 0 || block->second.Slot != 14)
                    throw std::runtime_error("Invalid procedural parameter block or duplicate name: " + name);
                auto member =
                  std::find_if(block->second.Members.begin(), block->second.Members.end(), [&](const auto& value) { return value.Name == name; });
                const auto type = parameter["Type"].as<String>();
                const bool triple = type == "color" || type == "point" || type == "vector" || type == "normal";
                const ShaderDataType expected = triple ? ShaderDataType::Float3 : type == "int" ? ShaderDataType::Int : ShaderDataType::Float;
                const auto defaults = parameter["Default"];
                if ((!triple && type != "int" && type != "float") || member == block->second.Members.end() || member->DataType != expected ||
                    member->Offset != parameter["Offset"].as<uint32_t>() || !defaults.IsSequence() || defaults.size() != (triple ? 3 : 1))
                    throw std::runtime_error("Procedural parameter metadata does not match SPIR-V: " + name);
                member->DefaultValue.resize(defaults.size() * 4);
                for (size_t c = 0; c < defaults.size(); ++c)
                    if (type == "int")
                    {
                        const int32_t value = defaults[c].as<int32_t>();
                        std::memcpy(member->DefaultValue.data() + c * 4, &value, 4);
                    }
                    else
                    {
                        const float value = defaults[c].as<float>();
                        if (!std::isfinite(value))
                            throw std::runtime_error("Non-finite procedural default: " + name);
                        std::memcpy(member->DefaultValue.data() + c * 4, &value, 4);
                    }
                auto& annotation = description.Annotations[name];
                annotation.DisplayName = parameter["Label"] ? parameter["Label"].as<String>() : name.substr(name.starts_with("osl_") ? 4 : 0);
                annotation.IsColor = type == "color";
                if (parameter["min"] && parameter["max"])
                {
                    annotation.RangeMin = parameter["min"].as<float>();
                    annotation.RangeMax = parameter["max"].as<float>();
                    annotation.HasRange =
                      std::isfinite(annotation.RangeMin) && std::isfinite(annotation.RangeMax) && annotation.RangeMin < annotation.RangeMax;
                }
            }
            const auto osl = description.Uniforms.find("OslInputs");
            if (osl != description.Uniforms.end() && osl->second.Members.size() != names.size())
                throw std::runtime_error("OSL material parameter metadata is incomplete");
            const auto textures = document["Textures"];
            if ((textures && !textures.IsSequence()) || (textures ? textures.size() : 0) != textureCount || textureCount > 8)
                throw std::runtime_error("OSL texture input metadata is incomplete");
            UnorderedSet<uint32_t> slots;
            if (textures)
                for (const auto& texture : textures)
                {
                    const auto name = texture["Name"].as<String>();
                    const auto binding = texture["Binding"].as<uint32_t>();
                    const auto resource = description.Textures.find(name);
                    if (!name.starts_with("osl_") || !names.insert(name).second || !slots.insert(binding).second || binding < 15 || binding >= 23 ||
                        texture["Set"].as<uint32_t>() != 0 || resource == description.Textures.end() || resource->second.Set != 0 ||
                        resource->second.Slot != binding || resource->second.Type != SAMPLER2D || resource->second.ArraySize != 1 ||
                        resource->second.RuntimeArray)
                        throw std::runtime_error("OSL texture metadata does not match SPIR-V: " + name);
                    auto& annotation = description.Annotations[name];
                    annotation.DisplayName = texture["Label"].as<String>();
                    annotation.HasDefault = true;
                    annotation.DefaultValueStr = "black";
                }
        }
    } // namespace

    Ref<Shader> ProceduralMaterialProgram::LoadShader(const Path& package, String& error)
    {
        try
        {
            String parameters;
            if (std::filesystem::exists(package / "material.parameters.json"))
            {
                const auto bytes = ReadArtifact(package / "material.parameters.json");
                parameters.assign(bytes.begin(), bytes.end());
            }
            return CreateShader(ReadArtifact(package / "material.vert.spv"), ReadArtifact(package / "material.frag.spv"),
                                ReadArtifact(package / "material.interface.spv"), parameters, error);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return nullptr;
        }
    }

    bool ProceduralMaterialProgram::Initialize(const Path& package, String& error)
    {
        if (!AssetManager::TryGet() || !RenderAPI::TryGet() || RenderAPI::GetAPI() != RenderAPI::API::Vulkan ||
            RenderAPI::Get().GetCapabilities().GetFeatureTier() == RenderFeatureTier::Compatibility)
        {
            error = "Procedural materials require the Vulkan GPU scene renderer";
            return false;
        }
        const auto shader = LoadShader(package, error);
        if (!shader)
            return false;
        shader->GetTechniques().front()->Compile();
        m_Shader = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        return true;
    }

    bool ProceduralMaterialProgram::Initialize(const Vector<uint8_t>& vertexSpirv, const Vector<uint8_t>& fragmentSpirv,
                                               const Vector<uint8_t>& fragmentInterfaceSpirv, String& error)
    {
        error.clear();
        if (!RenderAPI::TryGet() || RenderAPI::GetAPI() != RenderAPI::API::Vulkan || !AssetManager::TryGet() ||
            RenderAPI::Get().GetCapabilities().GetFeatureTier() == RenderFeatureTier::Compatibility)
        {
            error = "Procedural material programs require the Vulkan GPU scene renderer and an active asset manager";
            return false;
        }
        Ref<Shader> shader;
        try
        {
            shader = CreateShader(vertexSpirv, fragmentSpirv, fragmentInterfaceSpirv, {}, error);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }
        if (!shader)
            return false;
        shader->GetTechniques().front()->Compile();
        m_Shader = static_asset_cast<Shader>(AssetManager::Get().CreateAssetHandle(shader));
        return true;
    }

    Ref<Shader> ProceduralMaterialProgram::CreateShader(const Vector<uint8_t>& vertexSpirv, const Vector<uint8_t>& fragmentSpirv,
                                                        const Vector<uint8_t>& fragmentInterfaceSpirv, const String& parameters, String& error)
    {
        error.clear();
        const auto vertex = ShaderCompiler::LoadSpirv(vertexSpirv, VERTEX_SHADER, error);
        if (!vertex)
            return nullptr;
        const auto fragment = ShaderCompiler::LoadSpirv(fragmentSpirv, FRAGMENT_SHADER, error);
        if (!fragment)
            return nullptr;
        const auto originalInterface = ShaderCompiler::LoadSpirv(fragmentInterfaceSpirv, FRAGMENT_SHADER, error);
        if (!originalInterface)
            return nullptr;
        fragment->Description = originalInterface->Description;
        const auto block = fragment->Description->Uniforms.find("ProceduralTextureParams");
        if (block == fragment->Description->Uniforms.end() || block->second.Set != 0 || block->second.Slot != 13)
        {
            error = "Missing procedural texture parameter block at set 0, binding 13";
            return nullptr;
        }
        const auto& members = block->second.Members;
        if (members.size() != 3 || members[0].Name != "proceduralUVTransform" || members[0].Offset != 0 ||
            members[0].DataType != ShaderDataType::Float4 || members[1].Name != "proceduralTime" || members[1].Offset != 16 ||
            members[1].DataType != ShaderDataType::Float || members[2].Name != "proceduralAmount" || members[2].Offset != 20 ||
            members[2].DataType != ShaderDataType::Float)
        {
            error = "Incompatible procedural texture parameter layout";
            return nullptr;
        }
        ApplyParameters(*fragment->Description, parameters);
        for (auto& member : block->second.Members)
        {
            const float values[]{ member.Name == "proceduralTime" ? 0.0f : 1.0f, 1.0f, 0.0f, 0.0f };
            member.DefaultValue.resize(member.Name == "proceduralUVTransform" ? 16 : 4);
            std::memcpy(member.DefaultValue.data(), values, member.DefaultValue.size());
        }
        ShaderRenderPassDesc pass{};
        pass.VertexShader = vertex;
        pass.FragmentShader = fragment;
        pass.RasterizationState = CreateRef<RasterizerStateDesc>();
        pass.DepthStencilState = CreateRef<DepthStencilStateDesc>();
        pass.DepthStencilState->DepthCompareFunction = CompareFunction::GREATER_EQUAL;
        pass.BlendState = CreateRef<BlendStateDesc>();
        const auto technique = ShaderTechnique::Create({ "material_model=custom", "procedural_texture" }, {}, { ShaderRenderPass::Create(pass) });
        ShaderDesc description;
        description.Techniques = { technique };
        const auto shader = Shader::Create(description);
        shader->SetName("Procedural PBR");
        return shader;
    }

    Ref<Material> ProceduralMaterialProgram::CreateMaterial() const
    {
        if (!m_Shader)
            return nullptr;
        const auto material = Material::CreatePBR(m_Shader);
        material->SetColor("proceduralUVTransform", glm::vec4(1, 1, 0, 0));
        material->SetFloat("proceduralTime", 0.0f);
        material->SetFloat("proceduralAmount", 1.0f);
        material->SetAlphaMode(AlphaMode::Opaque);
        return material;
    }
} // namespace Crowny
