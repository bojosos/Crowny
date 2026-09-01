#pragma once

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    class ScriptInspectorPath
    {
    public:
        static StringView ResolveSetting(StringView setting, const ScriptValue& root);
        static Path ResolveParentFolder(const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot);
        static Path ResolveStoredPath(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot);
        static Path InitialDirectory(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot);
        static String Normalize(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot);
        static bool Exists(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot);
        static Vector<DialogFilter> Filters(const ScriptPathSettings& settings, const ScriptValue& root);
    };
} // namespace Crowny
