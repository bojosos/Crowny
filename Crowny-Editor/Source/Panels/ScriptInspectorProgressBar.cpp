#include "cwepch.h"

#include "Panels/ScriptInspectorProgressBar.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Crowny
{
    namespace
    {
        const ScriptValue* FindMember(const String& name, const ScriptValue& root)
        {
            if (name.empty() || root.Kind != ScriptValueKind::Object)
                return nullptr;
            const auto member = root.Members.find(name);
            return member != root.Members.end() ? &member->second : nullptr;
        }

        StringView FormatNumber(const ScriptValue& value, char* buffer, size_t bufferSize)
        {
            if (buffer == nullptr || bufferSize == 0)
                return {};
            char* const end = buffer + bufferSize;
            std::to_chars_result result{ buffer, std::errc{} };
            switch (value.Kind)
            {
            case ScriptValueKind::SignedInteger:
                result = std::to_chars(buffer, end, value.SignedValue);
                break;
            case ScriptValueKind::UnsignedInteger:
                result = std::to_chars(buffer, end, value.UnsignedValue);
                break;
            case ScriptValueKind::Decimal:
                return value.StringValue;
            case ScriptValueKind::Float:
                result = std::to_chars(buffer, end, value.FloatingValue, std::chars_format::general, 6);
                break;
            default:
                return {};
            }
            return result.ec == std::errc{} ? StringView(buffer, static_cast<size_t>(result.ptr - buffer)) : StringView();
        }
    } // namespace

    bool ScriptInspectorProgressBar::TryReadNumber(const ScriptValue& value, double& output)
    {
        switch (value.Kind)
        {
        case ScriptValueKind::SignedInteger:
            output = static_cast<double>(value.SignedValue);
            break;
        case ScriptValueKind::UnsignedInteger:
            output = static_cast<double>(value.UnsignedValue);
            break;
        case ScriptValueKind::Float:
            output = value.FloatingValue;
            break;
        case ScriptValueKind::Decimal: {
            if (value.StringValue.empty())
            {
                output = 0.0;
                break;
            }
            const char* begin = value.StringValue.c_str();
            char* end = nullptr;
            output = std::strtod(begin, &end);
            if (end != begin + value.StringValue.size())
                return false;
            break;
        }
        default:
            return false;
        }
        return std::isfinite(output);
    }

    bool ScriptInspectorProgressBar::TryWriteNumber(ScriptValue& value, double input)
    {
        if (!std::isfinite(input))
            return false;

        switch (value.Kind)
        {
        case ScriptValueKind::SignedInteger: {
            const double rounded = std::round(input);
            const int64_t result = rounded <= static_cast<double>(std::numeric_limits<int64_t>::min())   ? std::numeric_limits<int64_t>::min()
                                   : rounded >= static_cast<double>(std::numeric_limits<int64_t>::max()) ? std::numeric_limits<int64_t>::max()
                                                                                                         : static_cast<int64_t>(rounded);
            if (result == value.SignedValue)
                return false;
            value.SignedValue = result;
            return true;
        }
        case ScriptValueKind::UnsignedInteger: {
            const double rounded = std::round(input);
            const uint64_t result = rounded <= 0.0                                                         ? 0
                                    : rounded >= static_cast<double>(std::numeric_limits<uint64_t>::max()) ? std::numeric_limits<uint64_t>::max()
                                                                                                           : static_cast<uint64_t>(rounded);
            if (result == value.UnsignedValue)
                return false;
            value.UnsignedValue = result;
            return true;
        }
        case ScriptValueKind::Float:
            if (input == value.FloatingValue)
                return false;
            value.FloatingValue = input;
            return true;
        case ScriptValueKind::Decimal: {
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<double>::max_digits10) << input;
            const String result = stream.str();
            if (result == value.StringValue)
                return false;
            value.StringValue = result;
            return true;
        }
        default:
            return false;
        }
    }

    void ScriptInspectorProgressBar::ResolveBounds(const ScriptProgressBarSettings& settings, const ScriptValue& root, double& min, double& max)
    {
        min = settings.Min;
        max = settings.Max;
        if (const ScriptValue* value = FindMember(settings.MinGetter, root))
            TryReadNumber(*value, min);
        if (const ScriptValue* value = FindMember(settings.MaxGetter, root))
            TryReadNumber(*value, max);
    }

    glm::vec4 ScriptInspectorProgressBar::ResolveColor(const String& getter, const ScriptValue& root, const glm::vec4& fallback)
    {
        const ScriptValue* value = FindMember(getter, root);
        return value != nullptr && value->Kind == ScriptValueKind::Color ? value->VectorValue : fallback;
    }

    StringView ScriptInspectorProgressBar::ResolveLabelView(const ScriptProgressBarSettings& settings, const ScriptValue& root,
                                                            const ScriptValue& value, char* buffer, size_t bufferSize)
    {
        if (const ScriptValue* label = FindMember(settings.CustomValueStringGetter, root); label != nullptr && label->Kind == ScriptValueKind::String)
            return label->StringValue;
        return FormatNumber(value, buffer, bufferSize);
    }

    String ScriptInspectorProgressBar::ResolveLabel(const ScriptProgressBarSettings& settings, const ScriptValue& root, const ScriptValue& value)
    {
        char buffer[64];
        return String(ResolveLabelView(settings, root, value, buffer, sizeof(buffer)));
    }

    float ScriptInspectorProgressBar::Fraction(double value, double min, double max)
    {
        if (!std::isfinite(value) || !std::isfinite(min) || !std::isfinite(max) || max <= min)
            return 0.0f;
        return static_cast<float>(std::clamp((value - min) / (max - min), 0.0, 1.0));
    }
} // namespace Crowny
