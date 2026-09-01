#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class ScriptInspectorProgressBar
    {
    public:
        static bool TryReadNumber(const ScriptValue& value, double& output);
        static bool TryWriteNumber(ScriptValue& value, double input);
        static void ResolveBounds(const ScriptProgressBarSettings& settings, const ScriptValue& root, double& min, double& max);
        static glm::vec4 ResolveColor(const String& getter, const ScriptValue& root, const glm::vec4& fallback);
        static StringView ResolveLabelView(const ScriptProgressBarSettings& settings, const ScriptValue& root, const ScriptValue& value,
                                           char* buffer, size_t bufferSize);
        static String ResolveLabel(const ScriptProgressBarSettings& settings, const ScriptValue& root, const ScriptValue& value);
        static float Fraction(double value, double min, double max);
    };
} // namespace Crowny
