#include "cwepch.h"

#include "Panels/ScriptInspectorPath.h"

#include <algorithm>
#include <system_error>

namespace Crowny
{
    namespace
    {
        Path ParsePath(StringView value)
        {
            String normalized(value);
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            return Path(normalized);
        }

        String DisplayPath(const Path& value, bool useBackslashes)
        {
            String result = value.generic_string();
            if (useBackslashes)
                std::replace(result.begin(), result.end(), '/', '\\');
            return result;
        }

        bool IsDirectory(const Path& value)
        {
            std::error_code error;
            return fs::is_directory(value, error);
        }

        bool IsFile(const Path& value)
        {
            std::error_code error;
            return fs::is_regular_file(value, error);
        }
    } // namespace

    StringView ScriptInspectorPath::ResolveSetting(StringView setting, const ScriptValue& root)
    {
        if (setting.empty() || setting.front() != '$')
            return setting;
        if (root.Kind != ScriptValueKind::Object || setting.size() == 1)
            return {};
        const StringView memberName = setting.substr(1);
        for (const auto& [name, value] : root.Members)
        {
            if (name == memberName)
                return value.Kind == ScriptValueKind::String ? StringView(value.StringValue) : StringView();
        }
        return {};
    }

    Path ScriptInspectorPath::ResolveParentFolder(const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot)
    {
        const StringView configured = ResolveSetting(settings.ParentFolder, root);
        if (configured.empty())
            return projectRoot.lexically_normal();
        const Path parent = ParsePath(configured);
        return (parent.is_absolute() ? parent : projectRoot / parent).lexically_normal();
    }

    Path ScriptInspectorPath::ResolveStoredPath(StringView value, const ScriptPathSettings& settings, const ScriptValue& root,
                                                const Path& projectRoot)
    {
        const Path path = ParsePath(value);
        if (path.empty())
            return {};
        return (path.is_absolute() ? path : ResolveParentFolder(settings, root, projectRoot) / path).lexically_normal();
    }

    Path ScriptInspectorPath::InitialDirectory(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot)
    {
        if (value.empty())
            return ResolveParentFolder(settings, root, projectRoot);
        Path initial = ResolveStoredPath(value, settings, root, projectRoot);
        if (settings.Kind == ScriptPathKind::File || !IsDirectory(initial))
            initial = initial.parent_path();
        return initial.empty() ? projectRoot : initial;
    }

    String ScriptInspectorPath::Normalize(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot)
    {
        if (value.empty())
            return {};
        Path path = ResolveStoredPath(value, settings, root, projectRoot);
        if (settings.Kind == ScriptPathKind::File && !settings.IncludeFileExtension)
            path.replace_extension();
        if (!settings.AbsolutePath)
        {
            const Path relative = path.lexically_relative(ResolveParentFolder(settings, root, projectRoot));
            if (!relative.empty())
                path = relative;
        }
        return DisplayPath(path.lexically_normal(), settings.UseBackslashes);
    }

    bool ScriptInspectorPath::Exists(StringView value, const ScriptPathSettings& settings, const ScriptValue& root, const Path& projectRoot)
    {
        if (value.empty())
            return false;
        const Path path = ResolveStoredPath(value, settings, root, projectRoot);
        if (settings.Kind == ScriptPathKind::Folder)
            return IsDirectory(path);
        if (IsFile(path))
            return true;
        if (settings.IncludeFileExtension || path.has_extension())
            return false;
        for (const DialogFilter& filter : Filters(settings, root))
        {
            Path candidate = path;
            candidate.replace_extension(filter.Name);
            if (IsFile(candidate))
                return true;
        }
        return false;
    }

    Vector<DialogFilter> ScriptInspectorPath::Filters(const ScriptPathSettings& settings, const ScriptValue& root)
    {
        return FileSystem::ParseDialogFilters(ResolveSetting(settings.Extensions, root));
    }
} // namespace Crowny
