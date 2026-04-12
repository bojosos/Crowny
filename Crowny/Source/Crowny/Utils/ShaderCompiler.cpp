#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Shader.h"

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <shaderc/shaderc.hpp>
#include <tracy/Tracy.hpp>

#include <regex>

namespace Crowny
{
    // TODO: Switch to pragma shader_stage!!!! Although given how I write the shader in a single file we
    // will still have to split the file in two.
    static shaderc_shader_kind ShaderTypeToShaderC(ShaderType shaderType)
    {
        switch (shaderType)
        {
        case VERTEX_SHADER:
            return shaderc_glsl_vertex_shader;
        case FRAGMENT_SHADER:
            return shaderc_glsl_fragment_shader;
        case GEOMETRY_SHADER:
            return shaderc_glsl_geometry_shader;
        case DOMAIN_SHADER:
            return shaderc_glsl_tess_control_shader;
        case HULL_SHADER:
            return shaderc_glsl_tess_evaluation_shader;
        case COMPUTE_SHADER:
            return shaderc_glsl_compute_shader;
        case RAYGEN_SHADER:
            return shaderc_glsl_raygen_shader;
        case HIT_SHADER:
            return shaderc_glsl_closesthit_shader;
        case MISS_SHADER:
            return shaderc_glsl_miss_shader;
        default:
            return shaderc_glsl_vertex_shader;
        }
    }

    static String ShaderTypeToString(ShaderType shaderType)
    {
        switch (shaderType)
        {
        case VERTEX_SHADER:
            return "vertex";
        case FRAGMENT_SHADER:
            return "fragment";
        case GEOMETRY_SHADER:
            return "geometry";
        case HULL_SHADER:
            return "hull";
        case DOMAIN_SHADER:
            return "domain";
        case COMPUTE_SHADER:
            return "compute";
        case RAYGEN_SHADER:
            return "raygen";
        case MISS_SHADER:
            return "miss";
        case HIT_SHADER:
            return "hit";
        default:
            return String();
        }
    }

    static UniformResourceType SPIRTypeToResourceType(const spirv_cross::SPIRType& type)
    {
        if (type.basetype == spirv_cross::SPIRType::SampledImage)
        {
            switch (type.image.dim)
            {
            case spv::Dim::Dim1D:
                return SAMPLER1D;
            case spv::Dim::Dim2D:
                return SAMPLER2D;
            case spv::Dim::Dim3D:
                return SAMPLER3D;
            case spv::Dim::DimCube:
                return SAMPLERCUBE;
            default:
                return TEXTURE_UNKNOWN;
            }
        }

        if (type.basetype == spirv_cross::SPIRType::Image)
        {
            switch (type.image.dim)
            {
            case spv::Dim::Dim1D:
                return TEXTURE1D;
            case spv::Dim::Dim2D:
                return TEXTURE2D;
            case spv::Dim::Dim3D:
                return TEXTURE3D;
            case spv::Dim::DimCube:
                return TEXTURECUBE;
            default:
                return TEXTURE_UNKNOWN;
            }
        }

        return TEXTURE_UNKNOWN;
    }

    static VertexAttribute GetSpecialVertexAttribute(const StringView& attributeName)
    {
        if (attributeName == "cw_Position")
            return VertexAttribute::Position;
        if (attributeName == "cw_Normal")
            return VertexAttribute::Normal;
        if (attributeName == "cw_Tangent")
            return VertexAttribute::Tangent;
        if (attributeName == "cw_Bitangent")
            return VertexAttribute::Bitangent;
        if (attributeName == "cw_Color")
            return VertexAttribute::Color;
        if (attributeName == "cw_TexCoord0")
            return VertexAttribute::TexCoord0;
        if (attributeName == "cw_TexCoord1")
            return VertexAttribute::TexCoord1;
        if (attributeName == "cw_TexCoord2")
            return VertexAttribute::TexCoord2;
        if (attributeName == "cw_TexCoord3")
            return VertexAttribute::TexCoord3;
        if (attributeName == "cw_TexCoord4")
            return VertexAttribute::TexCoord4;
        if (attributeName == "cw_TexCoord5")
            return VertexAttribute::TexCoord5;
        if (attributeName == "cw_TexCoord6")
            return VertexAttribute::TexCoord6;
        if (attributeName == "cw_TexCoord7")
            return VertexAttribute::TexCoord7;
        if (attributeName == "cw_BlendWeights")
            return VertexAttribute::BlendWeights;
        if (attributeName == "cw_BlendIndices")
            return VertexAttribute::BlendIndices;
        // CW_ENGINE_ASSERT(false);
        return VertexAttribute::None;
    }

    static ShaderDataType SprivTypeToShaderType(const spirv_cross::SPIRType& type)
    {
        switch (type.basetype)
        {
        case spirv_cross::SPIRType::Boolean:
            return ShaderDataType::Bool;
        case spirv_cross::SPIRType::Int: // TODO: Width, vecsize, columns?
            switch (type.vecsize)
            {
            case 1:
                return ShaderDataType::Int;
            case 2:
                return ShaderDataType::Int2;
            case 3:
                return ShaderDataType::Int3;
            case 4:
                return ShaderDataType::Int4;
            default:
                CW_ENGINE_ASSERT(false);
                return ShaderDataType::Int;
            }
        case spirv_cross::SPIRType::Float: // TODO: Width, vecsize, columns?
            switch (type.vecsize)
            {
            case 1:
                return ShaderDataType::Float;
            case 2:
                return ShaderDataType::Float2;
            case 3:
                CW_ENGINE_ASSERT(type.columns == type.vecsize || type.columns == 1);
                return type.columns == 3 ? ShaderDataType::Mat3 : ShaderDataType::Float3;
            case 4:
                CW_ENGINE_ASSERT(type.columns == type.vecsize || type.columns == 1);
                return type.columns == 4 ? ShaderDataType::Mat4 : ShaderDataType::Float4;
            default:
                CW_ENGINE_ASSERT(false);
                return ShaderDataType::Float;
            }
        }

        return ShaderDataType::None;
    }

    static bool GetShaderTypeFromString(const String& type, ShaderType& outShaderType)
    {
        if (type == "vertex")
            outShaderType = VERTEX_SHADER;
        else if (type == "fragment" || type == "pixel")
            outShaderType = FRAGMENT_SHADER;
        else if (type == "geometry")
            outShaderType = GEOMETRY_SHADER;
        else if (type == "domain")
            outShaderType = DOMAIN_SHADER;
        else if (type == "hull")
            outShaderType = HULL_SHADER;
        else if (type == "compute")
            outShaderType = COMPUTE_SHADER;
        else if (type == "raygen")
            outShaderType = RAYGEN_SHADER;
        else if (type == "raymiss")
            outShaderType = MISS_SHADER;
        else if (type == "rayhit")
            outShaderType = HIT_SHADER;
        else
            return false;
        return true;
    }

    static bool GetShaderLanguage(const String& lang, ShaderLanguage& outShaderLanguage)
    {
        if (lang == "hlsl")
            outShaderLanguage = ShaderLanguage::HLSL;
        else if (lang == "glsl")
            outShaderLanguage = ShaderLanguage::GLSL;
        else
            return false;
        return true;
    }

    Ref<BlendStateDesc> ShaderCompiler::PreparseBlendState(String& shader)
    {
        Ref<BlendStateDesc> result;
        auto strPosIter = shader.find("blend_state");
        const auto strPos = strPosIter;

        auto skipWhiteSpace = [&strPosIter, &shader]() {
            while (strPosIter < shader.size() && std::isspace(shader[strPosIter]))
                strPosIter++;
        };
        auto expect = [&strPosIter, &shader](char value) {
            CW_ENGINE_ASSERT(shader[strPosIter] == value, "Bad programmer");

            strPosIter++;
        };
        auto isNext = [&strPosIter, &shader](const String& value) {
            for (int i = 0; i < value.size(); i++)
                if (std::tolower(shader[strPosIter + i]) != std::tolower(value[i]))
                    return false;
            strPosIter += value.size();
            return true;
        };
        auto parseBool = [&]() {
            if (isNext("true"))
                return true;
            else if (isNext("false"))
                return false;
            else
                CW_ENGINE_ERROR("Bad");
            return false;
        };
        auto parseBlendFactor = [&]() {
            if (isNext("one"))
                return BlendFactor::One;
            else if (isNext("zero"))
                return BlendFactor::Zero;
            else if (isNext("dstrgb"))
                return BlendFactor::DestColor;
            else if (isNext("srcrgb"))
                return BlendFactor::SourceColor;
            else if (isNext("dstirgb"))
                return BlendFactor::InvDestColor;
            else if (isNext("srcirgb"))
                return BlendFactor::InvSourceColor;
            else if (isNext("dsta"))
                return BlendFactor::DestAlpha;
            else if (isNext("srca"))
                return BlendFactor::SourceAlpha;
            else if (isNext("dstia"))
                return BlendFactor::InvDestAlpha;
            else if (isNext("srcia"))
                return BlendFactor::InvSourceAlpha;
            CW_ENGINE_ASSERT(false);
            return BlendFactor::One;
        };
        auto parseBlendOp = [&]() {
            if (isNext("add"))
                return BlendFunction::ADD;
            else if (isNext("sub"))
                return BlendFunction::SUBTRACT;
            else if (isNext("rsub"))
                return BlendFunction::REVERSE_SUBTRACT;
            else if (isNext("min"))
                return BlendFunction::MIN;
            else if (isNext("max"))
                return BlendFunction::MAX;
            CW_ENGINE_ASSERT(false);
            return BlendFunction::ADD;
        };
        auto parseColorBlendOp = [&](BlendFactor& srcBlend, BlendFactor& dstBlend, BlendFunction& blendOp) {
            skipWhiteSpace();
            expect('{');
            skipWhiteSpace();
            srcBlend = parseBlendFactor();
            skipWhiteSpace();
            expect(',');
            skipWhiteSpace();
            dstBlend = parseBlendFactor();
            skipWhiteSpace();
            expect(',');
            skipWhiteSpace();
            blendOp = parseBlendOp();
            skipWhiteSpace();
            expect('}');
            skipWhiteSpace();
            expect(';');
        };
        if (strPosIter != String::npos)
        {
            result = CreateRef<BlendStateDesc>();
            strPosIter += strlen("blend_state");
            skipWhiteSpace();
            expect('{');
            while (shader[strPosIter] != '}')
            {
                skipWhiteSpace();
                if (isNext("enabled"))
                {
                    skipWhiteSpace();
                    expect('=');
                    skipWhiteSpace();
                    result->EnableBlending = parseBool();
                    skipWhiteSpace();
                    expect(';');
                }
                else if (isNext("color"))
                {
                    skipWhiteSpace();
                    expect('=');
                    skipWhiteSpace();
                    parseColorBlendOp(result->SrcBlend, result->DstBlend, result->BlendOp);
                }
                else if (isNext("alpha"))
                {
                    skipWhiteSpace();
                    expect('=');
                    skipWhiteSpace();
                    parseColorBlendOp(result->SrcBlendAlpha, result->DstBlendAlpha, result->BlendOpAlpha);
                }
                else if (isNext("writemask"))
                {
                    CW_ENGINE_ASSERT(false);
                }
                skipWhiteSpace();
            }
            expect('}');
            skipWhiteSpace();
            expect(';');
            shader = shader.substr(0, strPos) + shader.substr(strPosIter);
        }
        return result;
    }

    Ref<BinaryShaderData> ShaderCompiler::CompileStage(const String& source, ShaderType shaderType, ShaderLanguage inputLanguage,
                                                       ShaderLanguageFlags outputLanguages, const UnorderedMap<String, String>& defines)
    {
        ZoneScopedN("ShaderCompiler::CompileStage");
        Vector<uint8_t> shaderBinaryData;

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        for (auto& kv : defines)
            options.AddMacroDefinition(kv.first, kv.second);
        switch (inputLanguage)
        {
        case (ShaderLanguage::GLSL):
            options.SetSourceLanguage(shaderc_source_language_glsl);
            break;
        case (ShaderLanguage::HLSL):
            options.SetSourceLanguage(shaderc_source_language_hlsl);
            break;
        }

        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                     shaderc_env_version_vulkan_1_3); // TODO: Better versioning
        // options.SetOptimizationLevel(shaderc_optimization_level_performance); // TODO: if set, can't use uniform
        // names, so maybe compile twice?

        const char* entryPoints[SHADER_COUNT] = { "vsmain", "fsmain", "gsmain", "dsmain", "hsmain", "csmain", "raygen", "hit", "miss" };
        const char* entryPoint = entryPoints[shaderType];
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.c_str(), source.size(), ShaderTypeToShaderC(shaderType),
                                                                         ShaderTypeToString(shaderType).c_str(), entryPoint, options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            String shaderTypeString = ShaderTypeToString(shaderType);
            shaderTypeString[0] = std::toupper(shaderTypeString[0]);
            CW_ENGINE_ERROR("{0} shader compilation error: {1}", shaderTypeString, module.GetErrorMessage());
        }
        else
            shaderBinaryData = Vector<uint8_t>((uint8_t*)module.cbegin(), (uint8_t*)module.cend());

        // Something went wrong, still thought we need to return a valid shader
        if (shaderBinaryData.empty())
            return CreateRef<BinaryShaderData>(shaderBinaryData, entryPoint, shaderType, nullptr);

        Ref<BinaryShaderData> dataResult = CreateRef<BinaryShaderData>();
        dataResult->Data = shaderBinaryData;
        dataResult->Type = shaderType;
        dataResult->EntryPoint = entryPoint;

        // Initializes the VertexLayout and UniformDesc.
        Reflect(shaderBinaryData, dataResult);

        return dataResult;
    }

    ShaderDesc ShaderCompiler::Compile(const Path& path, const String& rawSource, ShaderLanguageFlags shaderLanguage,
                                       const UnorderedMap<String, String>& defines)
    {
        ZoneScopedN("ShaderCompiler::Compile");
        const char* langToken = "#lang";
        const size_t langTokenLength = strlen(langToken);
        const size_t pos = rawSource.find(langToken, 0);
        ShaderLanguage inputLanguage = ShaderLanguage::GLSL;
        if (pos != String::npos)
        {
            const size_t eol = rawSource.find_first_of("\n\r", pos);
            if (eol != String::npos)
            {
                const size_t begin = pos + langTokenLength + 1;
                const String langString = rawSource.substr(begin, eol - begin);
                if (!GetShaderLanguage(langString, inputLanguage))
                    CW_ENGINE_ERROR("Shader language string {0} not recognized. Assuming shader is in GLSL.", langString);
            }
        }
        else
            CW_ENGINE_WARN("#lang directive not found in {0}, assuming shader is in GLSL.", path.string());

        String source = rawSource;
        auto blendState = PreparseBlendState(source);
        const auto parsedPasses = Parse(source);
        Vector<Ref<ShaderRenderPass>> renderPasses;

        for (const auto& sourceShaders : parsedPasses)
        {
            ShaderRenderPassDesc passDesc;
            String passSourceCombined;
            for (const auto& [type, stageSource] : sourceShaders)
            {
                passSourceCombined += stageSource;
                const Ref<BinaryShaderData> shaderData = CompileStage(stageSource, type, inputLanguage, shaderLanguage, defines);
                if (type == VERTEX_SHADER)
                    passDesc.VertexShader = shaderData;
                else if (type == FRAGMENT_SHADER)
                    passDesc.FragmentShader = shaderData;
                else if (type == GEOMETRY_SHADER)
                    passDesc.GeometryShader = shaderData;
                else if (type == HULL_SHADER)
                    passDesc.DomainShader = shaderData;
                else if (type == DOMAIN_SHADER)
                    passDesc.DomainShader = shaderData;
                else if (type == COMPUTE_SHADER)
                    passDesc.ComputeShader = shaderData;
                else if (type == RAYGEN_SHADER)
                    passDesc.RaygenShader = shaderData;
                else if (type == HIT_SHADER)
                    passDesc.HitShader = shaderData;
                else if (type == MISS_SHADER)
                    passDesc.MissShader = shaderData;
                else
                    CW_ENGINE_ASSERT(false);
            }
            EvaluatePragmaDirectives(passSourceCombined, passDesc);
            passDesc.BlendState = blendState;
            renderPasses.push_back(ShaderRenderPass::Create(passDesc));
        }

        Ref<ShaderTechnique> technique = ShaderTechnique::Create({}, ShaderVariation(), renderPasses);
        ShaderDesc shaderDesc;
        shaderDesc.Techniques = { technique };
        return shaderDesc;
    }

    void ShaderCompiler::Reflect(const Vector<uint8_t>& shaderBinaryData, Ref<BinaryShaderData>& outData)
    {
        ZoneScopedN("ShaderCompiler::Reflect");
        const spirv_cross::Compiler compiler((uint32_t*)shaderBinaryData.data(), shaderBinaryData.size() / sizeof(uint32_t));
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        Ref<UniformDesc> uniformDesc = CreateRef<UniformDesc>();
        // Read all uniform buffers in the current stage.
        for (const spirv_cross::Resource& uniform : resources.uniform_buffers)
        {
            const spirv_cross::SPIRType bufferType = compiler.get_type(uniform.base_type_id);
            const uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(bufferType);
            const uint32_t binding = compiler.get_decoration(uniform.id, spv::DecorationBinding);
            const uint32_t set = compiler.get_decoration(uniform.id, spv::DecorationDescriptorSet);

            UniformBufferBlockDesc buffer;
            buffer.Name = uniform.name;
            buffer.BlockSize = bufferSize;
            buffer.Slot = binding;
            buffer.Set = set;

            Vector<UniformBufferBlockMember> members;
            for (uint32_t i = 0; i < bufferType.member_types.size(); i++)
            {
                UniformBufferBlockMember& newMember = members.emplace_back();
                const spirv_cross::TypeID member = bufferType.member_types[i];
                const spirv_cross::SPIRType memberType = compiler.get_type(member);
                newMember.Name = compiler.get_member_name(bufferType.self, i);
                newMember.Offset = compiler.type_struct_member_offset(bufferType, i);
                newMember.DataType = SprivTypeToShaderType(memberType);
            }
            buffer.Members = std::move(members);
            uniformDesc->Uniforms[uniform.name] = buffer;
        }

        for (const spirv_cross::Resource& sampler : resources.sampled_images)
        {
            const spirv_cross::SPIRType& bufferType = compiler.get_type(sampler.base_type_id);
            const uint32_t binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            const uint32_t set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);

            UniformResourceDesc resource;
            resource.Name = sampler.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;

            uniformDesc->Samplers[resource.Name] = resource;
            uniformDesc->Textures[resource.Name] = resource;
        }

        for (const spirv_cross::Resource& texture : resources.separate_images)
        {
            const spirv_cross::SPIRType& bufferType = compiler.get_type(texture.base_type_id);
            const uint32_t binding = compiler.get_decoration(texture.id, spv::DecorationBinding);
            const uint32_t set = compiler.get_decoration(texture.id, spv::DecorationDescriptorSet);

            UniformResourceDesc resource;
            resource.Name = texture.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;

            uniformDesc->Textures[resource.Name] = resource;
        }

        for (const spirv_cross::Resource& sampler : resources.separate_samplers)
        {
            const spirv_cross::SPIRType& bufferType = compiler.get_type(sampler.base_type_id);
            const uint32_t binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            const uint32_t set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);

            UniformResourceDesc resource;
            resource.Name = sampler.name;
            resource.Type = SPIRTypeToResourceType(bufferType);
            resource.Slot = binding;
            resource.Set = set;
            // TODO: Fix this
            // resource.ElementType = MapSamplerBasicType(sampler);

            uniformDesc->Samplers[resource.Name] = resource;
        }
        // TODO: Buffers and loadstore textures

        for (const spirv_cross::Resource& accelStruct : resources.acceleration_structures)
        {
            const uint32_t binding = compiler.get_decoration(accelStruct.id, spv::DecorationBinding);
            const uint32_t set = compiler.get_decoration(accelStruct.id, spv::DecorationDescriptorSet);

            AccelerationStructDesc resource;
            resource.Name = accelStruct.name;
            resource.Slot = binding;
            resource.Set = set;

            uniformDesc->AccelerationStructures[resource.Name] = resource;
        }

        outData->Description = uniformDesc;

        // Retrieve the vertex shader input layout
        if (outData->Type == ShaderType::VERTEX_SHADER)
        {
            BufferLayout layout;
            for (const auto& vertInput : resources.stage_inputs)
            {
                const auto& bufferType = compiler.get_type(vertInput.base_type_id);
                const uint32_t location = compiler.get_decoration(vertInput.id, spv::DecorationLocation);
                const VertexAttribute attrSemantic = GetSpecialVertexAttribute(vertInput.name);
                BufferElement element(SprivTypeToShaderType(bufferType), attrSemantic, false);
                element.Name = vertInput.name;
                layout.AddBufferElement(element);
            }
            outData->VertexLayout = std::move(layout);
        }
    }

    void ShaderCompiler::EvaluatePragmaDirectives(const String& source, ShaderRenderPassDesc& shaderPassDesc)
    {
        std::regex pragma_regex(R"(#pragma\s+(\w+)\s+(\w+))");
        std::vector<std::string> pragma_names;
        std::vector<std::string> pragma_values;
        std::sregex_iterator iter(source.begin(), source.end(), pragma_regex);
        std::sregex_iterator end;
        while (iter != end)
        {
            std::smatch match = *iter;
            const String name = match[1].str();
            const String value = match[2].str();
            // CW_ENGINE_INFO("#pragma directive: {} = {}", name, value);
            if (name == "depth_read")
            {
                if (!shaderPassDesc.DepthStencilState)
                    shaderPassDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
                shaderPassDesc.DepthStencilState->EnableDepthRead = (value == "false" ? false : true);
            }
            else if (name == "depth_write")
            {
                if (!shaderPassDesc.DepthStencilState)
                    shaderPassDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
                shaderPassDesc.DepthStencilState->EnableDepthWrite = (value == "false" ? false : true);
            }
            else if (name == "cull")
            {
                if (!shaderPassDesc.RasterizationState)
                    shaderPassDesc.RasterizationState = CreateRef<RasterizerStateDesc>();
                if (value == "false")
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_NONE;
                else if (value == "front")
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_CLOCKWISE;
                else
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_COUNTERCLOCKWISE;
            }
            else
                CW_ENGINE_WARN("Unrecognized #pragma {}={}", name, value);
            iter++;
        }
    }

    Vector<UnorderedMap<ShaderType, String>> ShaderCompiler::Parse(const String& source)
    {
        Vector<UnorderedMap<ShaderType, String>> passes;

        // Split source into passes by #pass directives
        Vector<String> passSources;
        const char* passToken = "#pass";
        size_t passPos = source.find(passToken, 0);
        if (passPos == String::npos)
        {
            // No #pass directive — single pass (backward compatible)
            passSources.push_back(source);
        }
        else
        {
            while (passPos != String::npos)
            {
                // Skip past "#pass N" line
                size_t eol = source.find_first_of("\r\n", passPos);
                size_t contentStart = (eol != String::npos) ? source.find_first_not_of("\r\n", eol) : String::npos;
                size_t nextPass = source.find(passToken, contentStart != String::npos ? contentStart : passPos + 1);
                if (contentStart != String::npos)
                {
                    String passSource =
                      (nextPass == String::npos) ? source.substr(contentStart) : source.substr(contentStart, nextPass - contentStart);
                    passSources.push_back(passSource);
                }
                passPos = nextPass;
            }
        }

        // Parse each pass for #type directives
        const char* typeToken = "#type";
        const size_t typeTokenLength = strlen(typeToken);

        for (const String& passSource : passSources)
        {
            UnorderedMap<ShaderType, String> shaderSources;
            size_t pos = passSource.find(typeToken, 0);
            while (pos != String::npos)
            {
                size_t eol = passSource.find_first_of("\r\n", pos);
                CW_ENGINE_ASSERT(eol != String::npos, "Syntax error");
                size_t begin = pos + typeTokenLength + 1;
                String typeString = passSource.substr(begin, eol - begin);
                ShaderType shaderType;
                if (!GetShaderTypeFromString(typeString, shaderType))
                {
                    CW_ENGINE_ERROR("Shader type string {0} not recognized.", typeString);
                    break;
                }

                if (typeString == "compute")
                {
                    shaderSources[shaderType] = passSource.substr(begin + typeString.size());
                    CW_ENGINE_ASSERT(shaderSources.size() == 1);
                    passes.push_back(shaderSources);
                    return passes;
                }

                size_t nextLinePos = passSource.find_first_not_of("\r\n", eol);
                CW_ENGINE_ASSERT(nextLinePos != String::npos, "Syntax error");
                pos = passSource.find(typeToken, nextLinePos);

                shaderSources[shaderType] =
                  (pos == String::npos) ? passSource.substr(nextLinePos) : passSource.substr(nextLinePos, pos - nextLinePos);
            }
            if (shaderSources.size() < 2)
                CW_ENGINE_ERROR("You are required to provide at least a vertex and a fragment shader.");
            passes.push_back(shaderSources);
        }

        return passes;
    }

} // namespace Crowny
