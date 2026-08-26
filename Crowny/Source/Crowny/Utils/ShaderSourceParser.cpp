#include "cwpch.h"

#include "Crowny/Common/HashedString.h"
#include "Crowny/Utils/ShaderSourceParser.h"

#include <cctype>
#include <limits>

namespace Crowny
{
    using namespace Literals;

    namespace
    {
        StringView Trim(StringView value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                value.remove_prefix(1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
                value.remove_suffix(1);
            return value;
        }

        bool ConsumeDirective(StringView line, StringView directive, StringView& value)
        {
            line = Trim(line);
            if (!line.starts_with(directive))
                return false;
            if (line.size() > directive.size() && !std::isspace(static_cast<unsigned char>(line[directive.size()])))
                return false;
            value = Trim(line.substr(directive.size()));
            return true;
        }

        String Lower(StringView value)
        {
            String result(value);
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        bool GetShaderType(StringView value, ShaderType& type)
        {
            const String lower = Lower(Trim(value));
            const HashedString stage{ StringView(lower) };
            if (stage == "vertex"_hstr)
                type = VERTEX_SHADER;
            else if (stage == "fragment"_hstr || stage == "pixel"_hstr)
                type = FRAGMENT_SHADER;
            else if (stage == "geometry"_hstr)
                type = GEOMETRY_SHADER;
            else if (stage == "domain"_hstr || stage == "tess_control"_hstr)
                type = DOMAIN_SHADER;
            else if (stage == "hull"_hstr || stage == "tess_evaluation"_hstr)
                type = HULL_SHADER;
            else if (stage == "compute"_hstr)
                type = COMPUTE_SHADER;
            else if (stage == "raygen"_hstr)
                type = RAYGEN_SHADER;
            else if (stage == "rayhit"_hstr || stage == "closesthit"_hstr)
                type = HIT_SHADER;
            else if (stage == "raymiss"_hstr || stage == "miss"_hstr)
                type = MISS_SHADER;
            else
                return false;
            return true;
        }

        String ShaderTypeName(ShaderType type)
        {
            static constexpr const char* NAMES[SHADER_COUNT] = { "vertex", "fragment", "geometry", "domain", "hull",
                                                                 "compute", "raygen",   "rayhit",  "raymiss" };
            return NAMES[static_cast<uint32_t>(type)];
        }

        void AddError(ParsedShaderSource& result, const Path& path, uint32_t line, const String& message)
        {
            result.Diagnostics.push_back({ ShaderDiagnosticSeverity::Error, path, line, {}, message });
        }

        Vector<String> SplitTokens(StringView value)
        {
            Vector<String> result;
            std::istringstream stream{ String(value) };
            String token;
            while (stream >> token)
                result.push_back(std::move(token));
            return result;
        }

        bool IsEnginePragma(StringView name)
        {
            const HashedString pragma(name);
            return pragma == "variation"_hstr || pragma == "variation_multi"_hstr || pragma == "depth_read"_hstr ||
                   pragma == "depth_write"_hstr || pragma == "depth_compare"_hstr || pragma == "cull"_hstr ||
                   pragma == "polygon_mode"_hstr || pragma == "material_model"_hstr;
        }

        void ValidatePass(ParsedShaderSource& result, const Path& path, const ShaderSourcePass& pass, uint32_t line)
        {
            const bool hasGraphics = pass.HasStage[VERTEX_SHADER] || pass.HasStage[FRAGMENT_SHADER] || pass.HasStage[GEOMETRY_SHADER] ||
                                     pass.HasStage[DOMAIN_SHADER] || pass.HasStage[HULL_SHADER];
            const bool hasCompute = pass.HasStage[COMPUTE_SHADER];
            const bool hasRayTracing = pass.HasStage[RAYGEN_SHADER] || pass.HasStage[HIT_SHADER] || pass.HasStage[MISS_SHADER];
            const uint32_t pipelineKinds = static_cast<uint32_t>(hasGraphics) + static_cast<uint32_t>(hasCompute) + static_cast<uint32_t>(hasRayTracing);

            if (pipelineKinds == 0)
                AddError(result, path, line, "Shader pass has no stages.");
            else if (pipelineKinds > 1)
                AddError(result, path, line, "A shader pass cannot mix graphics, compute, and ray-tracing stages.");
            else if (hasGraphics && (!pass.HasStage[VERTEX_SHADER] || !pass.HasStage[FRAGMENT_SHADER]))
                AddError(result, path, line, "A graphics pass requires both vertex and fragment stages.");
            else if (hasGraphics && pass.HasStage[DOMAIN_SHADER] != pass.HasStage[HULL_SHADER])
                AddError(result, path, line, "Tessellation control and evaluation stages must be declared together.");
            else if (hasRayTracing && !pass.HasStage[RAYGEN_SHADER])
                AddError(result, path, line, "A ray-tracing pass requires a raygen stage.");
        }
    } // namespace

    bool ParsedShaderSource::Succeeded() const
    {
        return std::none_of(Diagnostics.begin(), Diagnostics.end(),
                            [](const ShaderDiagnostic& diagnostic) { return diagnostic.Severity == ShaderDiagnosticSeverity::Error; });
    }

    bool ShaderSourceParser::IsIdentifier(StringView value)
    {
        if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_'))
            return false;
        return std::all_of(value.begin() + 1, value.end(),
                           [](unsigned char character) { return std::isalnum(character) || character == '_'; });
    }

    ParsedShaderSource ShaderSourceParser::Parse(const Path& path, const String& source)
    {
        ParsedShaderSource result;
        Vector<String> lines;
        std::istringstream stream(source);
        String line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(std::move(line));
        }

        if (lines.empty())
        {
            AddError(result, path, 0, "Shader source is empty.");
            return result;
        }

        Vector<bool> engineDirective(lines.size(), false);
        Vector<bool> globalLine(lines.size(), false);
        Vector<int32_t> linePass(lines.size(), -1);
        Vector<int32_t> lineStage(lines.size(), -1);

        ShaderSourcePass* currentPass = nullptr;
        int32_t currentPassIndex = -1;
        ShaderType currentStage = SHADER_COUNT;
        bool sawStage = false;
        Set<String> variationNames;
        Set<String> passNames;
        bool materialModelDeclared = false;

        auto ensurePass = [&]() -> ShaderSourcePass& {
            if (currentPass == nullptr)
            {
                result.Passes.emplace_back();
                result.Passes.back().Name = std::to_string(result.Passes.size() - 1);
                passNames.insert(result.Passes.back().Name);
                currentPass = &result.Passes.back();
                currentPassIndex = static_cast<int32_t>(result.Passes.size() - 1);
            }
            return *currentPass;
        };

        for (size_t index = 0; index < lines.size(); ++index)
        {
            const uint32_t lineNumber = static_cast<uint32_t>(index + 1);
            const StringView trimmed = Trim(lines[index]);
            StringView value;

            if (ConsumeDirective(trimmed, "#lang", value))
            {
                engineDirective[index] = true;
                const String language = Lower(value);
                if (language != "glsl" && language != "hlsl")
                    AddError(result, path, lineNumber, "Unknown shader language '" + String(value) + "'.");
                else
                    result.Language = language;
                continue;
            }

            if (ConsumeDirective(trimmed, "#pass", value))
            {
                engineDirective[index] = true;
                result.Passes.emplace_back();
                currentPass = &result.Passes.back();
                currentPassIndex = static_cast<int32_t>(result.Passes.size() - 1);
                currentPass->Name = value.empty() ? std::to_string(result.Passes.size() - 1) : String(value);
                currentPass->Line = lineNumber;
                if (!passNames.insert(currentPass->Name).second)
                    AddError(result, path, lineNumber, "Shader pass '" + currentPass->Name + "' is declared more than once.");
                currentStage = SHADER_COUNT;
                continue;
            }

            if (ConsumeDirective(trimmed, "#type", value))
            {
                engineDirective[index] = true;
                ShaderType type;
                if (!GetShaderType(value, type))
                {
                    AddError(result, path, lineNumber, "Unknown shader stage '" + String(value) + "'.");
                    currentStage = SHADER_COUNT;
                    continue;
                }

                ShaderSourcePass& pass = ensurePass();
                if (pass.Line == 0)
                    pass.Line = lineNumber;
                if (pass.HasStage[type])
                    AddError(result, path, lineNumber, "Shader pass declares the " + ShaderTypeName(type) + " stage more than once.");
                pass.HasStage[type] = true;
                currentStage = type;
                sawStage = true;
                continue;
            }

            if (ConsumeDirective(trimmed, "#pragma", value))
            {
                Vector<String> tokens = SplitTokens(value);
                if (!tokens.empty() && IsEnginePragma(tokens[0]))
                {
                    engineDirective[index] = true;
                    const String pragmaValue = value.size() > tokens[0].size() ? String(Trim(value.substr(tokens[0].size()))) : String();
                    ShaderPragma pragma{ tokens[0], pragmaValue, lineNumber };
                    if (tokens[0] == "material_model")
                    {
                        if (currentPass != nullptr)
                            AddError(result, path, lineNumber,
                                     "#pragma material_model is global and must appear before the first pass or stage.");
                        if (materialModelDeclared)
                            AddError(result, path, lineNumber, "#pragma material_model is declared more than once.");
                        materialModelDeclared = true;
                        if (tokens.size() != 2)
                            AddError(result, path, lineNumber,
                                     "#pragma material_model expects one of: standard, unlit, toon, custom.");
                        else
                        {
                            const String model = Lower(tokens[1]);
                            if (model != "standard" && model != "unlit" && model != "toon" && model != "custom")
                                AddError(result, path, lineNumber,
                                         "Unknown material model '" + tokens[1] + "'. Expected standard, unlit, toon, or custom.");
                            else
                                pragma.Value = model;
                        }
                    }
                    if (currentPass != nullptr)
                        currentPass->Pragmas.push_back(pragma);
                    else
                        result.GlobalPragmas.push_back(pragma);

                    if (tokens[0] == "variation")
                    {
                        if (tokens.size() != 2 || !IsIdentifier(tokens[1]))
                        {
                            AddError(result, path, lineNumber, "#pragma variation expects one valid preprocessor identifier.");
                            continue;
                        }
                        if (!variationNames.insert(tokens[1]).second)
                        {
                            AddError(result, path, lineNumber, "Variation identifier '" + tokens[1] + "' appears in more than one group.");
                            continue;
                        }
                        result.VariationGroups.push_back({ tokens[1], { String(), tokens[1] }, true, lineNumber });
                    }
                    else if (tokens[0] == "variation_multi")
                    {
                        if (tokens.size() < 3)
                        {
                            AddError(result, path, lineNumber, "#pragma variation_multi expects at least two options.");
                            continue;
                        }

                        ShaderVariationGroup group;
                        group.Name = "group_" + std::to_string(result.VariationGroups.size());
                        group.Line = lineNumber;
                        Set<String> localNames;
                        for (size_t tokenIndex = 1; tokenIndex < tokens.size(); ++tokenIndex)
                        {
                            const String& option = tokens[tokenIndex];
                            if (option == "_")
                            {
                                if (!localNames.insert(String()).second)
                                    AddError(result, path, lineNumber, "A variation group cannot contain the '_' option more than once.");
                                else
                                    group.Options.emplace_back();
                            }
                            else if (!IsIdentifier(option))
                                AddError(result, path, lineNumber, "Invalid variation identifier '" + option + "'.");
                            else if (!localNames.insert(option).second || !variationNames.insert(option).second)
                                AddError(result, path, lineNumber, "Variation identifier '" + option + "' appears more than once.");
                            else
                                group.Options.push_back(option);
                        }
                        if (group.Options.size() >= 2)
                            result.VariationGroups.push_back(std::move(group));
                    }
                    continue;
                }
            }

            if (currentPassIndex < 0 && !sawStage)
                globalLine[index] = true;
            else if (currentPassIndex >= 0)
            {
                linePass[index] = currentPassIndex;
                if (currentStage != SHADER_COUNT)
                    lineStage[index] = static_cast<int32_t>(currentStage);
            }
        }

        if (result.Passes.empty())
            AddError(result, path, 0, "Shader source does not contain a #type directive.");

        for (size_t passIndex = 0; passIndex < result.Passes.size(); ++passIndex)
        {
            ShaderSourcePass& pass = result.Passes[passIndex];
            for (uint32_t type = 0; type < SHADER_COUNT; ++type)
            {
                if (!pass.HasStage[type])
                    continue;

                String& stageSource = pass.Stages[type];
                stageSource.reserve(source.size());
                for (size_t index = 0; index < lines.size(); ++index)
                {
                    const bool passGlobal = linePass[index] == static_cast<int32_t>(passIndex) && lineStage[index] < 0;
                    const bool selectedStage = linePass[index] == static_cast<int32_t>(passIndex) && lineStage[index] == static_cast<int32_t>(type);
                    if (!engineDirective[index] && (globalLine[index] || passGlobal || selectedStage))
                        stageSource += lines[index];
                    stageSource.push_back('\n');
                }
            }
            ValidatePass(result, path, pass, pass.Line);
        }

        uint64_t combinations = 1;
        for (const ShaderVariationGroup& group : result.VariationGroups)
        {
            if (group.Options.empty() || combinations > std::numeric_limits<uint32_t>::max() / group.Options.size())
            {
                AddError(result, path, group.Line, "Shader variation count overflows a 32-bit value.");
                combinations = 0;
                break;
            }
            combinations *= group.Options.size();
            if (combinations > MAX_VARIATIONS)
            {
                AddError(result, path, group.Line,
                         "Shader declares " + std::to_string(combinations) + " variations. The limit is " +
                           std::to_string(MAX_VARIATIONS) + ".");
                break;
            }
        }
        result.VariationCount = static_cast<uint32_t>(combinations);
        return result;
    }
} // namespace Crowny
