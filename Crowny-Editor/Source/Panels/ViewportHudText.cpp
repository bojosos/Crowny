#include "cwepch.h"

#include "Panels/ViewportHudText.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <span>

namespace Crowny
{
    ViewportHudStatus FormatViewportHudStatus(StringView primaryName, bool hasPrimary, size_t selectionCount, int32_t viewportWidth,
                                              int32_t viewportHeight, float cameraDistance) noexcept
    {
        ViewportHudStatus status;

        if (selectionCount > 1u)
        {
            std::snprintf(status.Text.data(), status.Text.size(), "%llu entities  |  %d x %d  |  View %.1f m",
                          static_cast<unsigned long long>(selectionCount), viewportWidth, viewportHeight, static_cast<double>(cameraDistance));
        }
        else if (hasPrimary)
        {
            const bool truncateName = primaryName.size() > 28u;
            const size_t nameLength = truncateName ? 25u : primaryName.size();
            const char* name = primaryName.empty() ? "" : primaryName.data();
            std::snprintf(status.Text.data(), status.Text.size(), "%.*s%s  |  %d x %d  |  View %.1f m", static_cast<int>(nameLength), name,
                          truncateName ? "..." : "", viewportWidth, viewportHeight, static_cast<double>(cameraDistance));
        }
        else
        {
            std::snprintf(status.Text.data(), status.Text.size(), "No selection  |  %d x %d  |  View %.1f m", viewportWidth, viewportHeight,
                          static_cast<double>(cameraDistance));
        }

        status.Text.back() = '\0';
        return status;
    }

    namespace
    {
        String NormalizeExtension(StringView extension)
        {
            String normalized;
            normalized.reserve(extension.size());
            for (char character : extension)
            {
                if (character == '.' && normalized.empty())
                    continue;
                normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            return normalized;
        }

        constexpr StringView MeshExtensions[] = { "obj", "gltf", "glb", "fbx", "dae", "3ds", "ply", "stl", "blend" };
        constexpr StringView TextureExtensions[] = { "png", "jpeg", "jpg", "psd", "gif", "tga", "bmp", "hdr", "pic", "ppm", "pgm", "ktx2" };
        constexpr StringView AudioExtensions[] = { "ogg", "wav" };
        constexpr StringView MaterialExtensions[] = { "cwmat", "mat" };

        bool Contains(std::span<const StringView> extensions, StringView normalized)
        {
            return std::find(extensions.begin(), extensions.end(), normalized) != extensions.end();
        }

        void PushUnique(Vector<Path>& paths, const Path& path)
        {
            if (std::find(paths.begin(), paths.end(), path) == paths.end())
                paths.push_back(path);
        }

        bool IsRelativeSidecar(StringView uri)
        {
            if (uri.empty() || uri.starts_with("data:") || uri.find("://") != StringView::npos)
                return false;
            return Path(uri).is_relative();
        }

        StringView TrimWhitespace(StringView text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == '\r' || text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }
    } // namespace

    bool IsViewportMeshExtension(StringView extension) noexcept
    {
        const String normalized = NormalizeExtension(extension);
        return !normalized.empty() && Contains(MeshExtensions, normalized);
    }

    ViewportDropFileKind ClassifyViewportDropFile(const Path& path) noexcept
    {
        const String normalized = NormalizeExtension(path.extension().string());
        if (normalized.empty())
            return ViewportDropFileKind::Unsupported;
        if (Contains(MeshExtensions, normalized))
            return ViewportDropFileKind::Mesh;
        if (Contains(TextureExtensions, normalized))
            return ViewportDropFileKind::Texture;
        if (Contains(AudioExtensions, normalized))
            return ViewportDropFileKind::AudioClip;
        if (Contains(MaterialExtensions, normalized))
            return ViewportDropFileKind::Material;
        if (normalized == "cwprefab")
            return ViewportDropFileKind::Prefab;
        if (normalized == "cwscene")
            return ViewportDropFileKind::Scene;
        return ViewportDropFileKind::Unsupported;
    }

    Vector<Path> CollectMeshSidecarReferences(const Path& sourcePath, StringView contents)
    {
        Vector<Path> references;
        const String normalized = NormalizeExtension(sourcePath.extension().string());
        if (normalized == "gltf")
        {
            // Minimal scan for "uri": "<value>" pairs; buffers and images are the only glTF properties that use `uri`.
            size_t cursor = 0;
            while ((cursor = contents.find("\"uri\"", cursor)) != StringView::npos)
            {
                cursor += 5;
                const size_t colon = contents.find(':', cursor);
                if (colon == StringView::npos)
                    break;
                const size_t open = contents.find('"', colon + 1);
                if (open == StringView::npos)
                    break;
                const size_t close = contents.find('"', open + 1);
                if (close == StringView::npos)
                    break;
                const StringView uri = contents.substr(open + 1, close - open - 1);
                if (IsRelativeSidecar(uri))
                    PushUnique(references, Path(uri));
                cursor = close + 1;
            }
        }
        else if (normalized == "obj")
        {
            size_t lineStart = 0;
            while (lineStart < contents.size())
            {
                size_t lineEnd = contents.find('\n', lineStart);
                if (lineEnd == StringView::npos)
                    lineEnd = contents.size();
                StringView line = TrimWhitespace(contents.substr(lineStart, lineEnd - lineStart));
                lineStart = lineEnd + 1;
                if (!line.starts_with("mtllib"))
                    continue;
                line.remove_prefix(6);
                line = TrimWhitespace(line);
                if (IsRelativeSidecar(line))
                    PushUnique(references, Path(line));
            }
        }
        return references;
    }
} // namespace Crowny
