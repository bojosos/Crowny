#include "cwepch.h"

#include "Editor/ProjectLibrary.h"
#include "Editor/ViewportAssetDrop.h"

namespace Crowny
{
    ViewportAssetDrop CreateProjectViewportAssetDrop()
    {
        return ViewportAssetDrop({
          [](const Path& path) -> Ref<FileEntry> {
              if (!ProjectLibrary::TryGet())
                  return nullptr;
              const Ref<LibraryEntry> entry = ProjectLibrary::Get().FindEntry(path);
              return entry && entry->Type == LibraryEntryType::File ? StaticRefCast<FileEntry>(entry) : nullptr;
          },
          [](const Path& source) -> Path {
              if (!ProjectLibrary::TryGet())
                  return {};
              auto& library = ProjectLibrary::Get();
              const Path path = ImportExternalDropFile(source, library.GetAssetFolder());
              if (path.empty())
                  return {};
              if (library.FindEntry(path))
                  library.Reimport(path);
              else
                  library.RefreshAsync(library.GetAssetFolder());
              return path;
          },
          [](const FileEntry& file) { return ProjectLibrary::Get().Load(&file); },
          []() { return ProjectLibrary::TryGet() && ProjectLibrary::Get().IsImporting(); },
        });
    }
} // namespace Crowny
