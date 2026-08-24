#include "cwpch.h"

#include "Crowny/Utils/ShaderCompiler.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Hash.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/ShaderVariation.h"

#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include <shaderc/shaderc.hpp>
#include <tracy/Tracy.hpp>

#include <regex>

namespace Crowny
{
    namespace
    {
        Mutex s_ShaderCacheMutex;
        UnorderedMap<String, Ref<BinaryShaderData>> s_ShaderCache;
        uint64_t s_ShaderCacheHits = 0;
        uint64_t s_ShaderCacheMisses = 0;
        constexpr size_t MAX_SHADER_CACHE_ENTRIES = 1024;

        String BuildCacheKey(const String& source, ShaderType shaderType, ShaderLanguage language, ShaderLanguageFlags flags,
                             const UnorderedMap<String, String>& defines)
        {
            ShaderDefines canonicalDefines;
            for (const auto& [name, value] : defines)
                canonicalDefines.Set(name, value);

            String result;
            result.reserve(source.size() + defines.size() * 24 + 32);
            result += std::to_string(static_cast<uint32_t>(shaderType));
            result.push_back('|');
            result += std::to_string(static_cast<uint32_t>(language));
            result.push_back('|');
            result += std::to_string(static_cast<uint32_t>(flags));
            result.push_back('|');
            result += canonicalDefines.GetCanonicalKey();
            result.push_back('|');
            result += source;
            return result;
        }

        void LogDiagnostic(const ShaderDiagnostic& diagnostic)
        {
            const String location = diagnostic.File.empty()
                                      ? String()
                                      : diagnostic.File.string() + (diagnostic.Line == 0 ? String() : ":" + std::to_string(diagnostic.Line));
            const String message = location.empty() ? diagnostic.Message : location + ": " + diagnostic.Message;
            if (diagnostic.Severity == ShaderDiagnosticSeverity::Error)
                CW_ENGINE_ERROR("{}", message);
            else
                CW_ENGINE_WARN("{}", message);
        }

        bool ExpandShaderIncludes(const Path& path, StringView source, String& output,
                                  Vector<ShaderDiagnostic>& diagnostics, Vector<Path>& includeStack, uint32_t depth)
        {
            constexpr uint32_t MAX_INCLUDE_DEPTH = 32;
            if (depth > MAX_INCLUDE_DEPTH)
            {
                diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, 0, {},
                                        "Shader include depth exceeds " + std::to_string(MAX_INCLUDE_DEPTH) + "." });
                return false;
            }

            const Path normalizedPath = path.lexically_normal();
            if (std::find(includeStack.begin(), includeStack.end(), normalizedPath) != includeStack.end())
            {
                diagnostics.push_back({ ShaderDiagnosticSeverity::Error, normalizedPath, 0, {},
                                        "Shader include cycle detected at '" + normalizedPath.string() + "'." });
                return false;
            }
            includeStack.push_back(normalizedPath);

            static const std::regex INCLUDE_PATTERN(R"(^\s*#\s*include\s*\"([^\"]+)\"\s*(?://.*)?$)");
            std::istringstream stream{ String(source) };
            String line;
            uint32_t lineNumber = 0;
            bool succeeded = true;
            while (std::getline(stream, line))
            {
                lineNumber++;
                std::smatch match;
                if (!std::regex_match(line, match, INCLUDE_PATTERN))
                {
                    output += line;
                    output.push_back('\n');
                    continue;
                }

                const Path includePath = (normalizedPath.parent_path() / Path(match[1].str())).lexically_normal();
                const Ref<DataStream> includeStream = FileSystem::OpenFile(includePath);
                if (includeStream == nullptr)
                {
                    diagnostics.push_back({ ShaderDiagnosticSeverity::Error, normalizedPath, lineNumber, {},
                                            "Cannot open shader include '" + includePath.string() + "'." });
                    succeeded = false;
                    continue;
                }

                const String includeSource = includeStream->GetAsString();
                includeStream->Close();
                output += "#line 1\n";
                succeeded &= ExpandShaderIncludes(includePath, includeSource, output, diagnostics, includeStack, depth + 1u);
                output += "#line " + std::to_string(lineNumber + 1u) + "\n";
            }

            includeStack.pop_back();
            return succeeded;
        }
    } // namespace

    bool ShaderCompileResult::Succeeded() const
    {
        return !Description.Techniques.empty() &&
               std::none_of(Diagnostics.begin(), Diagnostics.end(),
                            [](const ShaderDiagnostic& diagnostic) { return diagnostic.Severity == ShaderDiagnosticSeverity::Error; });
    }

    uint64_t ShaderCompiler::HashSource(StringView source)
    {
        return Hashing::CityHash64(source);
    }

    bool ShaderCompiler::PreprocessIncludes(const Path& path, StringView source, String& output,
                                            Vector<ShaderDiagnostic>& diagnostics)
    {
        output.clear();
        output.reserve(source.size());
        Vector<Path> includeStack;
        includeStack.reserve(8);
        return ExpandShaderIncludes(path, source, output, diagnostics, includeStack, 0);
    }

    void ShaderCompiler::ClearCache()
    {
        ScopedLock lock(s_ShaderCacheMutex);
        s_ShaderCache.clear();
        s_ShaderCacheHits = 0;
        s_ShaderCacheMisses = 0;
    }

    ShaderCompilerCacheStats ShaderCompiler::GetCacheStats()
    {
        ScopedLock lock(s_ShaderCacheMutex);
        return { s_ShaderCacheHits, s_ShaderCacheMisses, s_ShaderCache.size() };
    }
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

    static UniformResourceType SPIRTypeToStorageTextureType(const spirv_cross::SPIRType& type)
    {
        switch (type.image.dim)
        {
        case spv::Dim::Dim1D:
            return RWTEXTURE1D;
        case spv::Dim::Dim2D:
            return RWTEXTURE2D;
        case spv::Dim::Dim3D:
            return RWTEXTURE3D;
        default:
            return TEXTURE_UNKNOWN;
        }
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
        if (attributeName == "cw_PreviousPosition")
            return VertexAttribute::PreviousPosition;
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

    Ref<BlendStateDesc> ShaderCompiler::PreparseBlendState(String& shader)
    {
        Vector<ShaderDiagnostic> diagnostics;
        Ref<BlendStateDesc> result = ParseBlendState({}, shader, diagnostics);
        for (const ShaderDiagnostic& diagnostic : diagnostics)
            LogDiagnostic(diagnostic);
        return result;
    }

    Ref<BlendStateDesc> ShaderCompiler::ParseBlendState(const Path& path, String& shader, Vector<ShaderDiagnostic>& diagnostics)
    {
        static const std::regex blockRegex(R"((^|[\r\n])[\t ]*blend_state\s*\{)", std::regex::icase);
        std::smatch blockMatch;
        if (!std::regex_search(shader, blockMatch, blockRegex))
            return nullptr;

        Ref<BlendStateDesc> result = CreateRef<BlendStateDesc>();
        const size_t start = static_cast<size_t>(blockMatch.position());
        const size_t openBrace = start + static_cast<size_t>(blockMatch.length()) - 1;
        const uint32_t line = 1 + static_cast<uint32_t>(std::count(shader.begin(), shader.begin() + openBrace, '\n'));

        size_t closeBrace = String::npos;
        uint32_t braceDepth = 0;
        bool inLineComment = false;
        bool inBlockComment = false;
        char quote = '\0';
        bool escaped = false;
        for (size_t index = openBrace; index < shader.size(); ++index)
        {
            const char current = shader[index];
            const char next = index + 1 < shader.size() ? shader[index + 1] : '\0';
            if (inLineComment)
            {
                if (current == '\n')
                    inLineComment = false;
                continue;
            }
            if (inBlockComment)
            {
                if (current == '*' && next == '/')
                {
                    inBlockComment = false;
                    ++index;
                }
                continue;
            }
            if (quote != '\0')
            {
                if (escaped)
                    escaped = false;
                else if (current == '\\')
                    escaped = true;
                else if (current == quote)
                    quote = '\0';
                continue;
            }
            if (current == '/' && next == '/')
            {
                inLineComment = true;
                ++index;
                continue;
            }
            if (current == '/' && next == '*')
            {
                inBlockComment = true;
                ++index;
                continue;
            }
            if (current == '"' || current == '\'')
            {
                quote = current;
                continue;
            }
            if (current == '{')
                ++braceDepth;
            else if (current == '}' && --braceDepth == 0)
            {
                closeBrace = index;
                break;
            }
        }

        if (closeBrace == String::npos)
        {
            diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, line, {}, "Unterminated blend_state block." });
            return result;
        }

        size_t end = closeBrace + 1;
        while (end < shader.size() && std::isspace(static_cast<unsigned char>(shader[end])))
            ++end;
        if (end >= shader.size() || shader[end] != ';')
        {
            diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, line, {}, "blend_state block must end with ';'." });
        }
        else
        {
            ++end;
        }

        const String body = shader.substr(openBrace + 1, closeBrace - openBrace - 1);

        auto lower = [](String value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        };
        auto parseFactor = [&](const String& token, BlendFactor& output) {
            const String value = lower(token);
            static const UnorderedMap<String, BlendFactor> FACTORS = {
                { "one", BlendFactor::One },           { "zero", BlendFactor::Zero },
                { "dstrgb", BlendFactor::DestColor }, { "srcrgb", BlendFactor::SourceColor },
                { "dstirgb", BlendFactor::InvDestColor }, { "srcirgb", BlendFactor::InvSourceColor },
                { "dsta", BlendFactor::DestAlpha },   { "srca", BlendFactor::SourceAlpha },
                { "dstia", BlendFactor::InvDestAlpha }, { "srcia", BlendFactor::InvSourceAlpha },
            };
            const auto iter = FACTORS.find(value);
            if (iter == FACTORS.end())
                return false;
            output = iter->second;
            return true;
        };
        auto parseOperation = [&](const String& token, BlendFunction& output) {
            const String value = lower(token);
            static const UnorderedMap<String, BlendFunction> OPERATIONS = {
                { "add", BlendFunction::ADD }, { "sub", BlendFunction::SUBTRACT }, { "rsub", BlendFunction::REVERSE_SUBTRACT },
                { "min", BlendFunction::MIN }, { "max", BlendFunction::MAX },
            };
            const auto iter = OPERATIONS.find(value);
            if (iter == OPERATIONS.end())
                return false;
            output = iter->second;
            return true;
        };

        std::smatch match;
        if (std::regex_search(body, match, std::regex(R"(\benabled\s*=\s*(true|false)\s*;)", std::regex::icase)))
            result->EnableBlending = lower(match[1].str()) == "true";

        auto parseEquation = [&](StringView name, BlendFactor& source, BlendFactor& destination, BlendFunction& operation) {
            const std::regex assignmentRegex("\\b" + String(name) + R"(\s*=)", std::regex::icase);
            if (!std::regex_search(body, assignmentRegex))
                return;
            const std::regex equationRegex("\\b" + String(name) +
                                             R"(\s*=\s*\{\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\}\s*;)",
                                           std::regex::icase);
            std::smatch equation;
            if (!std::regex_search(body, equation, equationRegex) || !parseFactor(equation[1].str(), source) ||
                !parseFactor(equation[2].str(), destination) ||
                !parseOperation(equation[3].str(), operation))
                diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, line, {},
                                        "Invalid " + String(name) + " blend equation in blend_state." });
        };
        parseEquation("color", result->SrcBlend, result->DstBlend, result->BlendOp);
        parseEquation("alpha", result->SrcBlendAlpha, result->DstBlendAlpha, result->BlendOpAlpha);

        for (size_t index = start; index < end; ++index)
        {
            if (shader[index] != '\r' && shader[index] != '\n')
                shader[index] = ' ';
        }
        return result;
    }

    void ShaderCompiler::ParseAnnotations(const String& source, Ref<UniformDesc>& uniformDesc)
    {
        if (!uniformDesc)
            return;

        // Parse lines looking for annotation comments: // @keyword or // @keyword(args)
        // Annotations on a line apply to the variable declared on the next non-empty, non-comment line.
        std::istringstream stream(source);
        String line;
        Vector<String> pendingAnnotationLines;

        while (std::getline(stream, line))
        {
            // Trim leading whitespace
            size_t firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace == String::npos)
                continue;
            String trimmed = line.substr(firstNonSpace);

            // Check if this is an annotation comment line
            if (trimmed.rfind("// @", 0) == 0)
            {
                pendingAnnotationLines.push_back(trimmed);
                continue;
            }

            // If we have pending annotations and this is a non-empty line with a variable, extract the var name
            if (!pendingAnnotationLines.empty())
            {
                // Skip pure comment lines or empty lines
                if (trimmed.empty() || trimmed.rfind("//", 0) == 0)
                    continue;

                // Extract variable name: last identifier before ';' or '=' or ')'
                // Handles patterns like: "vec4 albedo;" or "float roughness;" or "} params;"
                String varName;
                // Find the semicolon
                size_t semiPos = trimmed.find(';');
                if (semiPos != String::npos)
                {
                    // Work backwards from semicolon to find the identifier
                    size_t end = semiPos;
                    // Skip trailing whitespace before semicolon
                    while (end > 0 && (trimmed[end - 1] == ' ' || trimmed[end - 1] == '\t'))
                        end--;
                    // Find the start of the identifier
                    size_t start = end;
                    while (start > 0 && (std::isalnum(trimmed[start - 1]) || trimmed[start - 1] == '_'))
                        start--;
                    if (start < end)
                        varName = trimmed.substr(start, end - start);
                }

                if (!varName.empty())
                {
                    AnnotationSet annotations;
                    // Parse all pending annotation lines
                    static const std::regex annotationRegex(R"(@(\w+)(?:\(([^)]*)\))?)");
                    for (const auto& annoLine : pendingAnnotationLines)
                    {
                        auto begin = std::sregex_iterator(annoLine.begin(), annoLine.end(), annotationRegex);
                        auto end = std::sregex_iterator();
                        for (auto it = begin; it != end; ++it)
                        {
                            const String keyword = (*it)[1].str();
                            String args = (*it)[2].matched ? (*it)[2].str() : "";

                            if (keyword == "color")
                                annotations.IsColor = true;
                            else if (keyword == "hdr")
                                annotations.IsHDR = true;
                            else if (keyword == "hide")
                                annotations.IsHidden = true;
                            else if (keyword == "name" && !args.empty())
                            {
                                // Remove surrounding quotes if present
                                if (args.size() >= 2 && args.front() == '"' && args.back() == '"')
                                    args = args.substr(1, args.size() - 2);
                                annotations.DisplayName = args;
                            }
                            else if (keyword == "range" && !args.empty())
                            {
                                // Parse "min, max"
                                size_t commaPos = args.find(',');
                                if (commaPos != String::npos)
                                {
                                    try
                                    {
                                        annotations.RangeMin = std::stof(args.substr(0, commaPos));
                                        annotations.RangeMax = std::stof(args.substr(commaPos + 1));
                                        annotations.HasRange = true;
                                    }
                                    catch (...)
                                    {
                                        CW_ENGINE_WARN("Failed to parse @range annotation: {}", args);
                                    }
                                }
                            }
                            else if (keyword == "default" && !args.empty())
                            {
                                annotations.DefaultValueStr = args;
                                annotations.HasDefault = true;
                            }
                        }
                    }
                    uniformDesc->Annotations[varName] = annotations;
                }
                pendingAnnotationLines.clear();
            }
        }
    }

    Ref<BinaryShaderData> ShaderCompiler::CompileStage(const String& source, ShaderType shaderType, ShaderLanguage inputLanguage,
                                                       ShaderLanguageFlags outputLanguages, const UnorderedMap<String, String>& defines)
    {
        Vector<ShaderDiagnostic> diagnostics;
        Ref<BinaryShaderData> result = CompileStage({}, source, shaderType, inputLanguage, outputLanguages, defines, diagnostics);
        for (const ShaderDiagnostic& diagnostic : diagnostics)
            LogDiagnostic(diagnostic);
        return result;
    }

    Ref<BinaryShaderData> ShaderCompiler::CompileStage(const Path& path, const String& source, ShaderType shaderType,
                                                       ShaderLanguage inputLanguage, ShaderLanguageFlags outputLanguages,
                                                       const UnorderedMap<String, String>& defines,
                                                       Vector<ShaderDiagnostic>& diagnostics)
    {
        ZoneScopedN("ShaderCompiler::CompileStage");
        const String cacheKey = BuildCacheKey(source, shaderType, inputLanguage, outputLanguages, defines);
        {
            ScopedLock lock(s_ShaderCacheMutex);
            const auto iter = s_ShaderCache.find(cacheKey);
            if (iter != s_ShaderCache.end())
            {
                ++s_ShaderCacheHits;
                return iter->second;
            }
            ++s_ShaderCacheMisses;
        }

        Vector<uint8_t> shaderBinaryData;

        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        for (const auto& kv : defines)
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

        options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                      shaderc_env_version_vulkan_1_3); // TODO: Better versioning
        // SPIR-V 1.6 lowers GLSL discard to OpDemoteToHelperInvocation. That instruction
        // cannot be translated to desktop OpenGL GLSL by SPIRV-Cross, while SPIR-V 1.5
        // retains OpKill and remains valid for the Vulkan 1.3 backend.
        options.SetTargetSpirv(shaderc_spirv_version_1_5);
        // options.SetOptimizationLevel(shaderc_optimization_level_performance); // TODO: if set, can't use uniform
        // names, so maybe compile twice?

        const char* hlslEntryPoints[SHADER_COUNT] = { "vsmain", "fsmain", "gsmain", "dsmain", "hsmain", "csmain", "raygen", "hit", "miss" };
        const char* entryPoint = inputLanguage == ShaderLanguage::HLSL ? hlslEntryPoints[shaderType] : "main";
        const String sourceName = path.empty() ? ShaderTypeToString(shaderType) : path.generic_string();
        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source.c_str(), source.size(), ShaderTypeToShaderC(shaderType),
                                                                         sourceName.c_str(), entryPoint, options);
        if (module.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, 0, ShaderTypeToString(shaderType), module.GetErrorMessage() });
        }
        else
        {
            const Vector<uint32_t> words(module.cbegin(), module.cend());
            shaderBinaryData.resize(words.size() * sizeof(uint32_t));
            std::memcpy(shaderBinaryData.data(), words.data(), shaderBinaryData.size());
            if (module.GetNumWarnings() > 0)
                diagnostics.push_back({ ShaderDiagnosticSeverity::Warning, path, 0, ShaderTypeToString(shaderType), module.GetErrorMessage() });
        }

        // Something went wrong, still thought we need to return a valid shader
        if (shaderBinaryData.empty())
            return CreateRef<BinaryShaderData>(shaderBinaryData, entryPoint, shaderType, nullptr);

        Ref<BinaryShaderData> dataResult = CreateRef<BinaryShaderData>();
        dataResult->Data = shaderBinaryData;
        dataResult->Type = shaderType;
        dataResult->EntryPoint = entryPoint;

        // Initializes the VertexLayout and UniformDesc.
        Reflect(shaderBinaryData, dataResult);

        // Parse annotations from GLSL source comments (before shaderc strips them)
        ParseAnnotations(source, dataResult->Description);

        // Convert @default annotation strings to raw byte defaults on members
        if (dataResult->Description)
        {
            auto& desc = dataResult->Description;
            for (auto& [blockName, block] : desc->Uniforms)
            {
                for (auto& member : block.Members)
                {
                    auto annoIt = desc->Annotations.find(member.Name);
                    if (annoIt == desc->Annotations.end() || !annoIt->second.HasDefault)
                        continue;

                    const String& defStr = annoIt->second.DefaultValueStr;
                    uint32_t byteSize = ShaderDataTypeSize(member.DataType);
                    member.DefaultValue.resize(byteSize, 0);

                    // Parse comma-separated floats/ints from the default string
                    Vector<float> floats;
                    std::istringstream ss(defStr);
                    String token;
                    while (std::getline(ss, token, ','))
                    {
                        try
                        {
                            // Trim whitespace
                            size_t start = token.find_first_not_of(" \t");
                            if (start != String::npos)
                                token = token.substr(start);
                            floats.push_back(std::stof(token));
                        }
                        catch (...)
                        {
                        }
                    }

                    switch (member.DataType)
                    {
                    case ShaderDataType::Float:
                        if (floats.size() >= 1)
                            std::memcpy(member.DefaultValue.data(), &floats[0], sizeof(float));
                        break;
                    case ShaderDataType::Float2:
                    {
                        glm::vec2 v(floats.size() >= 1 ? floats[0] : 0.0f, floats.size() >= 2 ? floats[1] : 0.0f);
                        std::memcpy(member.DefaultValue.data(), &v, sizeof(v));
                        break;
                    }
                    case ShaderDataType::Float3:
                    {
                        glm::vec3 v(floats.size() >= 1 ? floats[0] : 0.0f, floats.size() >= 2 ? floats[1] : 0.0f,
                                    floats.size() >= 3 ? floats[2] : 0.0f);
                        std::memcpy(member.DefaultValue.data(), &v, sizeof(v));
                        break;
                    }
                    case ShaderDataType::Float4:
                    {
                        glm::vec4 v(floats.size() >= 1 ? floats[0] : 0.0f, floats.size() >= 2 ? floats[1] : 0.0f,
                                    floats.size() >= 3 ? floats[2] : 0.0f, floats.size() >= 4 ? floats[3] : 0.0f);
                        std::memcpy(member.DefaultValue.data(), &v, sizeof(v));
                        break;
                    }
                    case ShaderDataType::Int:
                    {
                        int32_t iv = floats.size() >= 1 ? (int32_t)floats[0] : 0;
                        std::memcpy(member.DefaultValue.data(), &iv, sizeof(iv));
                        break;
                    }
                    case ShaderDataType::Bool:
                    {
                        int32_t bv = (floats.size() >= 1 && floats[0] != 0.0f) ? 1 : 0;
                        std::memcpy(member.DefaultValue.data(), &bv, sizeof(bv));
                        break;
                    }
                    default:
                        member.DefaultValue.clear(); // Unsupported type, skip
                        break;
                    }
                }
            }
        }

        {
            ScopedLock lock(s_ShaderCacheMutex);
            if (s_ShaderCache.size() >= MAX_SHADER_CACHE_ENTRIES)
                s_ShaderCache.clear();
            s_ShaderCache.try_emplace(cacheKey, dataResult);
        }
        return dataResult;
    }

    Vector<Ref<ShaderRenderPass>> ShaderCompiler::CompilePasses(const Path& path, const ParsedShaderSource& parsedSource,
                                                                ShaderLanguage inputLanguage, ShaderLanguageFlags shaderLanguage,
                                                                const UnorderedMap<String, String>& defines,
                                                                const Ref<BlendStateDesc>& blendState,
                                                                Vector<ShaderDiagnostic>& diagnostics)
    {
        Vector<Ref<ShaderRenderPass>> renderPasses;
        renderPasses.reserve(parsedSource.Passes.size());
        for (const ShaderSourcePass& sourcePass : parsedSource.Passes)
        {
            ShaderRenderPassDesc passDesc;
            bool passSucceeded = true;
            for (uint32_t typeIndex = 0; typeIndex < SHADER_COUNT; ++typeIndex)
            {
                if (!sourcePass.HasStage[typeIndex])
                    continue;
                const ShaderType type = static_cast<ShaderType>(typeIndex);
                const Ref<BinaryShaderData> shaderData =
                  CompileStage(path, sourcePass.Stages[typeIndex], type, inputLanguage, shaderLanguage, defines, diagnostics);
                passSucceeded &= shaderData != nullptr && !shaderData->Data.empty();
                if (type == VERTEX_SHADER)
                    passDesc.VertexShader = shaderData;
                else if (type == FRAGMENT_SHADER)
                    passDesc.FragmentShader = shaderData;
                else if (type == GEOMETRY_SHADER)
                    passDesc.GeometryShader = shaderData;
                else if (type == HULL_SHADER)
                    passDesc.HullShader = shaderData;
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
            EvaluatePragmaDirectives(parsedSource.GlobalPragmas, sourcePass.Pragmas, passDesc, diagnostics, path);
            passDesc.BlendState = blendState;
            if (passSucceeded)
                renderPasses.push_back(ShaderRenderPass::Create(passDesc));
        }
        return renderPasses;
    }

    ShaderDesc ShaderCompiler::Compile(const Path& path, const String& rawSource, ShaderLanguageFlags shaderLanguage,
                                       const UnorderedMap<String, String>& defines)
    {
        ShaderCompileResult result = CompileWithDiagnostics(path, rawSource, shaderLanguage, defines);
        for (const ShaderDiagnostic& diagnostic : result.Diagnostics)
            LogDiagnostic(diagnostic);
        return std::move(result.Description);
    }

    ShaderCompileResult ShaderCompiler::CompileWithDiagnostics(const Path& path, const String& rawSource,
                                                                ShaderLanguageFlags shaderLanguage,
                                                                const UnorderedMap<String, String>& defines)
    {
        ZoneScopedN("ShaderCompiler::Compile");
        ShaderCompileResult result;
        for (const auto& [name, _] : defines)
        {
            if (!ShaderSourceParser::IsIdentifier(name))
                result.Diagnostics.push_back(
                  { ShaderDiagnosticSeverity::Error, path, 0, {}, "Invalid preprocessor define name '" + name + "'." });
        }

        String source;
        if (!PreprocessIncludes(path, rawSource, source, result.Diagnostics))
            return result;
        const Ref<BlendStateDesc> blendState = ParseBlendState(path, source, result.Diagnostics);
        ParsedShaderSource parsedSource = ShaderSourceParser::Parse(path, source);
        result.Diagnostics.insert(result.Diagnostics.end(), parsedSource.Diagnostics.begin(), parsedSource.Diagnostics.end());
        if (!parsedSource.Succeeded() ||
            std::any_of(result.Diagnostics.begin(), result.Diagnostics.end(),
                        [](const ShaderDiagnostic& diagnostic) { return diagnostic.Severity == ShaderDiagnosticSeverity::Error; }))
            return result;

        const ShaderLanguage inputLanguage = parsedSource.Language == "hlsl" ? ShaderLanguage::HLSL : ShaderLanguage::GLSL;

        // Compile the cartesian product in declaration order, using mixed-radix indexing.
        const uint32_t totalCombinations = parsedSource.VariationCount;
        result.Description.Techniques.reserve(totalCombinations);
        for (uint32_t combination = 0; combination < totalCombinations; ++combination)
        {
            UnorderedMap<String, String> mergedDefines = defines;
            ShaderVariation variation;
            uint32_t mixedRadixIndex = combination;

            for (const ShaderVariationGroup& group : parsedSource.VariationGroups)
            {
                const uint32_t groupSize = static_cast<uint32_t>(group.Options.size());
                const uint32_t selected = mixedRadixIndex % groupSize;
                mixedRadixIndex /= groupSize;
                for (const String& option : group.Options)
                {
                    if (option.empty())
                        continue;
                    mergedDefines.erase(option);
                    variation.Set(option, false);
                }
                const String& selectedOption = group.Options[selected];
                if (!selectedOption.empty())
                {
                    mergedDefines[selectedOption] = "1";
                    variation.Set(selectedOption, true);
                }
            }

            Vector<Ref<ShaderRenderPass>> renderPasses = CompilePasses(path, parsedSource, inputLanguage, shaderLanguage, mergedDefines,
                                                                       blendState, result.Diagnostics);
            if (renderPasses.size() == parsedSource.Passes.size())
                result.Description.Techniques.push_back(ShaderTechnique::Create({}, variation, renderPasses));
        }
        if (!result.Succeeded())
            result.Description.Techniques.clear();
        return result;

    }

    void ShaderCompiler::Reflect(const Vector<uint8_t>& shaderBinaryData, Ref<BinaryShaderData>& outData)
    {
        ZoneScopedN("ShaderCompiler::Reflect");
        if (shaderBinaryData.empty() || shaderBinaryData.size() % sizeof(uint32_t) != 0)
            return;
        Vector<uint32_t> words(shaderBinaryData.size() / sizeof(uint32_t));
        std::memcpy(words.data(), shaderBinaryData.data(), shaderBinaryData.size());
        const spirv_cross::Compiler compiler(words.data(), words.size());
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        const Ref<UniformDesc> uniformDesc = CreateRef<UniformDesc>();
        const auto reflectArray = [&](const spirv_cross::Resource& input, UniformResourceDesc& output) {
            const spirv_cross::SPIRType& type = compiler.get_type(input.type_id);
            if (type.array.empty())
                return;
            output.RuntimeArray = type.array[0] == 0;
            output.ArraySize = output.RuntimeArray ? 1u : std::max(type.array[0], 1u);
        };
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
            reflectArray(sampler, resource);

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
            reflectArray(texture, resource);

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
            reflectArray(sampler, resource);
            // TODO: Fix this
            // resource.ElementType = MapSamplerBasicType(sampler);

            uniformDesc->Samplers[resource.Name] = resource;
        }
        for (const spirv_cross::Resource& storageBuffer : resources.storage_buffers)
        {
            UniformResourceDesc resource;
            resource.Name = compiler.get_name(storageBuffer.id);
            if (resource.Name.empty())
                resource.Name = storageBuffer.name;
            if (resource.Name.empty())
                resource.Name = compiler.get_fallback_name(storageBuffer.id);
            resource.Slot = compiler.get_decoration(storageBuffer.id, spv::DecorationBinding);
            resource.Set = compiler.get_decoration(storageBuffer.id, spv::DecorationDescriptorSet);
            resource.Type = compiler.has_decoration(storageBuffer.id, spv::DecorationNonWritable) ? STRUCTURED_BUFFER : RWSTRUCTURED_BUFFER;
            reflectArray(storageBuffer, resource);
            uniformDesc->Buffers[resource.Name] = resource;
        }

        for (const spirv_cross::Resource& storageImage : resources.storage_images)
        {
            const spirv_cross::SPIRType& imageType = compiler.get_type(storageImage.type_id);
            UniformResourceDesc resource;
            resource.Name = storageImage.name;
            resource.Slot = compiler.get_decoration(storageImage.id, spv::DecorationBinding);
            resource.Set = compiler.get_decoration(storageImage.id, spv::DecorationDescriptorSet);
            resource.Type = SPIRTypeToStorageTextureType(imageType);
            reflectArray(storageImage, resource);
            uniformDesc->LoadStoreTextures[resource.Name] = resource;
        }

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
            struct VertexInput
            {
                uint32_t Location;
                const spirv_cross::Resource* Resource;
            };
            Vector<VertexInput> inputs;
            inputs.reserve(resources.stage_inputs.size());
            for (const auto& vertInput : resources.stage_inputs)
                inputs.push_back({ compiler.get_decoration(vertInput.id, spv::DecorationLocation), &vertInput });
            std::sort(inputs.begin(), inputs.end(), [](const VertexInput& lhs, const VertexInput& rhs) { return lhs.Location < rhs.Location; });

            BufferLayout layout;
            for (const VertexInput& input : inputs)
            {
                const spirv_cross::Resource& vertInput = *input.Resource;
                const auto& bufferType = compiler.get_type(vertInput.base_type_id);
                const VertexAttribute attrSemantic = GetSpecialVertexAttribute(vertInput.name);
                BufferElement element(SprivTypeToShaderType(bufferType), attrSemantic, false);
                element.Name = vertInput.name;
                element.Location = input.Location;
                layout.AddBufferElement(element);
            }
            outData->VertexLayout = std::move(layout);
        }
    }

    void ShaderCompiler::EvaluatePragmaDirectives(const Vector<ShaderPragma>& globalPragmas, const Vector<ShaderPragma>& passPragmas,
                                                   ShaderRenderPassDesc& shaderPassDesc, Vector<ShaderDiagnostic>& diagnostics,
                                                   const Path& path)
    {
        auto addError = [&](const ShaderPragma& pragma, const String& message) {
            diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, pragma.Line, {}, message });
        };
        auto apply = [&](const ShaderPragma& pragma) {
            const String& name = pragma.Name;
            const String& value = pragma.Value;
            if (name == "depth_read" || name == "depth_write")
            {
                if (value != "true" && value != "false")
                {
                    addError(pragma, "#pragma " + name + " expects 'true' or 'false'.");
                    return;
                }
                if (!shaderPassDesc.DepthStencilState)
                    shaderPassDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
                if (name == "depth_read")
                    shaderPassDesc.DepthStencilState->EnableDepthRead = value == "true";
                else
                    shaderPassDesc.DepthStencilState->EnableDepthWrite = value == "true";
            }
            else if (name == "depth_compare")
            {
                if (!shaderPassDesc.DepthStencilState)
                    shaderPassDesc.DepthStencilState = CreateRef<DepthStencilStateDesc>();
                if (value == "never" || value == "always_fail")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::ALWAYS_FAIL;
                else if (value == "always" || value == "always_pass")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::ALWAYS_PASS;
                else if (value == "less")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::LESS;
                else if (value == "less_equal")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::LESS_EQUAL;
                else if (value == "equal")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::EQUAL;
                else if (value == "not_equal")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::NOT_EQUAL;
                else if (value == "greater")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::GREATER;
                else if (value == "greater_equal")
                    shaderPassDesc.DepthStencilState->DepthCompareFunction = CompareFunction::GREATER_EQUAL;
                else
                    addError(pragma, "#pragma depth_compare expects 'never', 'always', 'less', 'less_equal', 'equal', "
                                     "'not_equal', 'greater', or 'greater_equal'.");
            }
            else if (name == "cull")
            {
                if (!shaderPassDesc.RasterizationState)
                    shaderPassDesc.RasterizationState = CreateRef<RasterizerStateDesc>();
                if (value == "false" || value == "none")
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_NONE;
                else if (value == "front")
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_CLOCKWISE;
                else if (value == "back")
                    shaderPassDesc.RasterizationState->CullMode = CullingMode::CULL_COUNTERCLOCKWISE;
                else
                    addError(pragma, "#pragma cull expects 'none', 'false', 'front', or 'back'.");
            }
            else if (name == "polygon_mode")
            {
                if (!shaderPassDesc.RasterizationState)
                    shaderPassDesc.RasterizationState = CreateRef<RasterizerStateDesc>();
                if (value == "wireframe")
                    shaderPassDesc.RasterizationState->PolygonDrawMode = PolygonMode::Wireframe;
                else if (value == "point" || value == "points")
                    shaderPassDesc.RasterizationState->PolygonDrawMode = PolygonMode::Points;
                else if (value == "solid")
                    shaderPassDesc.RasterizationState->PolygonDrawMode = PolygonMode::Solid;
                else
                    addError(pragma, "#pragma polygon_mode expects 'solid', 'wireframe', or 'points'.");
            }
        };

        for (const ShaderPragma& pragma : globalPragmas)
            apply(pragma);
        for (const ShaderPragma& pragma : passPragmas)
            apply(pragma);
    }


} // namespace Crowny
