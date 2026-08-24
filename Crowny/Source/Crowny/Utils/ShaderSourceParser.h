#pragma once

#include "Crowny/Common/Types.h"

namespace Crowny
{
    enum class ShaderDiagnosticSeverity
    {
        Warning,
        Error
    };

    struct ShaderDiagnostic
    {
        ShaderDiagnosticSeverity Severity = ShaderDiagnosticSeverity::Error;
        Path File;
        uint32_t Line = 0;
        String Stage;
        String Message;
    };

    struct ShaderPragma
    {
        String Name;
        String Value;
        uint32_t Line = 0;
    };

    struct ShaderVariationGroup
    {
        String Name;
        Vector<String> Options;
        bool IsToggle = false;
        uint32_t Line = 0;
    };

    struct ShaderSourcePass
    {
        String Name;
        uint32_t Line = 0;
        Array<String, SHADER_COUNT> Stages;
        Array<bool, SHADER_COUNT> HasStage{};
        Vector<ShaderPragma> Pragmas;
    };

    struct ParsedShaderSource
    {
        String Language = "glsl";
        Vector<ShaderSourcePass> Passes;
        Vector<ShaderPragma> GlobalPragmas;
        Vector<ShaderVariationGroup> VariationGroups;
        Vector<ShaderDiagnostic> Diagnostics;
        uint32_t VariationCount = 1;

        bool Succeeded() const;
    };

    class ShaderSourceParser
    {
    public:
        static constexpr uint32_t MAX_VARIATIONS = 256;

        static ParsedShaderSource Parse(const Path& path, const String& source);
        static bool IsIdentifier(StringView value);
    };
} // namespace Crowny
