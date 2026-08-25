#include "cwpch.h"

#include "Crowny/Build/BuildTypes.h"

#include <cctype>

namespace Crowny
{
    void BuildValidation::Error(String code, String message, String subject)
    {
        Issues.push_back({ BuildIssueSeverity::Error, std::move(code), std::move(message), std::move(subject) });
    }

    void BuildValidation::Warn(String code, String message, String subject)
    {
        Issues.push_back({ BuildIssueSeverity::Warning, std::move(code), std::move(message), std::move(subject) });
    }

    bool BuildValidation::IsValid() const
    {
        return std::none_of(Issues.begin(), Issues.end(), [](const BuildIssue& issue) { return issue.Severity == BuildIssueSeverity::Error; });
    }

    bool BuildValidation::ContainsCode(StringView code) const
    {
        return std::any_of(Issues.begin(), Issues.end(), [&](const BuildIssue& issue) { return issue.Code == code; });
    }

    Vector<String> BuildValidation::GetErrors() const
    {
        Vector<String> errors;
        for (const BuildIssue& issue : Issues)
        {
            if (issue.Severity == BuildIssueSeverity::Error)
                errors.push_back(issue.Message);
        }
        return errors;
    }

    Vector<String> BuildValidation::GetWarnings() const
    {
        Vector<String> warnings;
        for (const BuildIssue& issue : Issues)
        {
            if (issue.Severity == BuildIssueSeverity::Warning)
                warnings.push_back(issue.Message);
        }
        return warnings;
    }

    void BuildValidation::Append(const BuildValidation& other) { Issues.insert(Issues.end(), other.Issues.begin(), other.Issues.end()); }

    const char* ToString(BuildPlatform value) { return value == BuildPlatform::WindowsX64 ? "WindowsX64" : "LinuxX64"; }

    const char* ToString(BuildConfiguration value) { return value == BuildConfiguration::Development ? "Development" : "Shipping"; }

    const char* ToString(QualityTier value)
    {
        switch (value)
        {
        case QualityTier::Low:
            return "Low";
        case QualityTier::Medium:
            return "Medium";
        case QualityTier::High:
            return "High";
        case QualityTier::Ultra:
            return "Ultra";
        }
        return "High";
    }

    const char* ToString(RendererBackend value) { return value == RendererBackend::Vulkan ? "Vulkan" : "OpenGL"; }

    const char* ToString(RendererPolicy value)
    {
        switch (value)
        {
        case RendererPolicy::VulkanThenOpenGL:
            return "VulkanThenOpenGL";
        case RendererPolicy::VulkanOnly:
            return "VulkanOnly";
        case RendererPolicy::OpenGLOnly:
            return "OpenGLOnly";
        }
        return "VulkanThenOpenGL";
    }

    const char* ToString(CompatibilityPolicy value) { return value == CompatibilityPolicy::Exact ? "Exact" : "DeclaredCompatible"; }

    namespace
    {
        template <class T> bool ParsePair(StringView value, StringView first, T firstValue, StringView second, T secondValue, T& output)
        {
            if (value == first)
            {
                output = firstValue;
                return true;
            }
            if (value == second)
            {
                output = secondValue;
                return true;
            }
            return false;
        }
    } // namespace

    bool TryParseBuildPlatform(StringView value, BuildPlatform& output)
    {
        return ParsePair(value, "WindowsX64", BuildPlatform::WindowsX64, "LinuxX64", BuildPlatform::LinuxX64, output);
    }

    bool TryParseBuildConfiguration(StringView value, BuildConfiguration& output)
    {
        return ParsePair(value, "Development", BuildConfiguration::Development, "Shipping", BuildConfiguration::Shipping, output);
    }

    bool TryParseQualityTier(StringView value, QualityTier& output)
    {
        if (value == "Low")
            output = QualityTier::Low;
        else if (value == "Medium")
            output = QualityTier::Medium;
        else if (value == "High")
            output = QualityTier::High;
        else if (value == "Ultra")
            output = QualityTier::Ultra;
        else
            return false;
        return true;
    }

    bool TryParseRendererBackend(StringView value, RendererBackend& output)
    {
        return ParsePair(value, "Vulkan", RendererBackend::Vulkan, "OpenGL", RendererBackend::OpenGL, output);
    }

    bool TryParseRendererPolicy(StringView value, RendererPolicy& output)
    {
        if (value == "VulkanThenOpenGL")
            output = RendererPolicy::VulkanThenOpenGL;
        else if (value == "VulkanOnly")
            output = RendererPolicy::VulkanOnly;
        else if (value == "OpenGLOnly")
            output = RendererPolicy::OpenGLOnly;
        else
            return false;
        return true;
    }

    bool TryParseCompatibilityPolicy(StringView value, CompatibilityPolicy& output)
    {
        return ParsePair(value, "Exact", CompatibilityPolicy::Exact, "DeclaredCompatible", CompatibilityPolicy::DeclaredCompatible, output);
    }

    bool IsSafeRelativeBuildPath(const Path& path)
    {
        if (path.empty() || path.is_absolute() || path.has_root_directory() || path.has_root_name())
            return false;

        const String portable = path.generic_string();
        if (portable.front() == '/' || portable.front() == '\\' ||
            (portable.size() >= 2 && std::isalpha(static_cast<unsigned char>(portable[0])) && portable[1] == ':'))
            return false;

        size_t componentStart = 0;
        for (size_t index = 0; index <= portable.size(); index++)
        {
            if (index != portable.size() && portable[index] != '/' && portable[index] != '\\')
                continue;
            if (StringView(portable).substr(componentStart, index - componentStart) == "..")
                return false;
            componentStart = index + 1;
        }

        for (const Path& part : path)
        {
            if (part == "..")
                return false;
        }

        const Path normalized = path.lexically_normal();
        for (const Path& part : normalized)
        {
            if (part == ".." || part == ".")
                return false;
        }
        return normalized != Path(".");
    }

    String NormalizePortableBuildPath(const Path& path)
    {
        String portable = path.generic_string();
        std::replace(portable.begin(), portable.end(), '\\', '/');
        portable = Path(portable).lexically_normal().generic_string();
        while (portable.starts_with("./"))
            portable.erase(0, 2);
        return portable;
    }

    String SanitizeArtifactName(StringView value)
    {
        String result;
        result.reserve(value.size());
        bool pendingSeparator = false;
        for (unsigned char character : value)
        {
            if (std::isalnum(character) || character == '_' || character == '-')
            {
                if (pendingSeparator && !result.empty())
                    result.push_back('-');
                result.push_back(static_cast<char>(character));
                pendingSeparator = false;
            }
            else if (!result.empty())
                pendingSeparator = true;
        }
        while (!result.empty() && (result.back() == '-' || result.back() == '.'))
            result.pop_back();
        return result;
    }
} // namespace Crowny
