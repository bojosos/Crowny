#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    using BuildCancellationCheck = std::function<bool()>;

    enum class BuildPlatform
    {
        WindowsX64,
        LinuxX64
    };

    enum class BuildConfiguration
    {
        Development,
        Shipping
    };

    enum class QualityTier
    {
        Low,
        Medium,
        High,
        Ultra
    };

    enum class RendererBackend
    {
        Vulkan,
        OpenGL
    };

    enum class RendererPolicy
    {
        VulkanThenOpenGL,
        VulkanOnly,
        OpenGLOnly
    };

    enum class CompatibilityPolicy
    {
        Exact,
        DeclaredCompatible
    };

    enum class BuildIssueSeverity
    {
        Warning,
        Error
    };

    struct BuildIssue
    {
        BuildIssueSeverity Severity = BuildIssueSeverity::Error;
        String Code;
        String Message;
        String Subject;
    };

    struct BuildValidation
    {
        Vector<BuildIssue> Issues;

        void Error(String code, String message, String subject = {});
        void Warn(String code, String message, String subject = {});
        bool IsValid() const;
        bool ContainsCode(StringView code) const;
        Vector<String> GetErrors() const;
        Vector<String> GetWarnings() const;
        void Append(const BuildValidation& other);
    };

    const char* ToString(BuildPlatform value);
    const char* ToString(BuildConfiguration value);
    const char* ToString(QualityTier value);
    const char* ToString(RendererBackend value);
    const char* ToString(RendererPolicy value);
    const char* ToString(CompatibilityPolicy value);

    bool TryParseBuildPlatform(StringView value, BuildPlatform& output);
    bool TryParseBuildConfiguration(StringView value, BuildConfiguration& output);
    bool TryParseQualityTier(StringView value, QualityTier& output);
    bool TryParseRendererBackend(StringView value, RendererBackend& output);
    bool TryParseRendererPolicy(StringView value, RendererPolicy& output);
    bool TryParseCompatibilityPolicy(StringView value, CompatibilityPolicy& output);

    bool IsSafeRelativeBuildPath(const Path& path);
    String NormalizePortableBuildPath(const Path& path);
    String SanitizeArtifactName(StringView value);
} // namespace Crowny
