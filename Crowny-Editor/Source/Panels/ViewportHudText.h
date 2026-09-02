#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    struct ViewportHudStatus
    {
        static constexpr size_t Capacity = 128u;

        Array<char, Capacity> Text{};
    };

    ViewportHudStatus FormatViewportHudStatus(StringView primaryName, bool hasPrimary, size_t selectionCount, int32_t viewportWidth,
                                              int32_t viewportHeight, float cameraDistance) noexcept;

    /// What the viewport does with a file dropped onto it from the OS shell.
    enum class ViewportDropFileKind
    {
        Unsupported = 0,
        Mesh,    // .obj/.gltf/.glb/.fbx/... -> import, then spawn a mesh entity
        Texture, // import only
        Material,
        AudioClip, // import, then spawn an audio source
        Prefab,    // import, then instantiate
        Scene      // import, then open
    };

    /// Classifies a dropped file by extension (case-insensitive). Pure; does not touch the filesystem.
    ViewportDropFileKind ClassifyViewportDropFile(const Path& path) noexcept;

    /// True when the extension (with or without the leading dot, any case) is a mesh format the viewport accepts.
    bool IsViewportMeshExtension(StringView extension) noexcept;

    /// Returns the relative sidecar files a mesh source references so they can be copied alongside it
    /// (glTF buffer/image `uri`s, OBJ `mtllib`). Only relative, non-data URIs are returned; duplicates are removed.
    Vector<Path> CollectMeshSidecarReferences(const Path& sourcePath, StringView contents);
} // namespace Crowny
