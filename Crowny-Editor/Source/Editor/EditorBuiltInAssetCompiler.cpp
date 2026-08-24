#include "cwepch.h"

#include "Editor/EditorBuiltInAssetCompiler.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Hash.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/Import/ImportOptions.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"

namespace Crowny
{
    namespace
    {
        constexpr Array<const char*, 17> TEXTURE_SOURCES = {
            "Resources/Icons/Play.png",         "Resources/Icons/Pause.png",       "Resources/Icons/Stop.png",
            "Resources/Icons/File.png",         "Resources/Icons/Folder.png",      "Resources/Icons/ArrowPointerIcon.png",
            "Resources/Icons/ArrowsIcon.png",   "Resources/Icons/RotateIcon.png",  "Resources/Icons/MaximizeIcon.png",
            "Resources/Icons/GlobeIcon.png",    "Resources/Icons/SearchIcon.png",  "Resources/Icons/ConsoleInfo.png",
            "Resources/Icons/ConsoleWarn.png",  "Resources/Icons/ConsoleError.png", "Resources/Icons/AlignLeft.png",
            "Resources/Icons/AlignCenter.png",  "Resources/Icons/AlignRight.png"
        };

        Path GetEditorRoot()
        {
            const Path workingDirectory = Application::TryGet()->GetWorkingDirectory();
            if (fs::is_directory(workingDirectory / "Crowny-Editor/Resources"))
                return workingDirectory / "Crowny-Editor";
            return workingDirectory;
        }

        int64_t GetTimestamp(const Path& path)
        {
            std::error_code error;
            const fs::file_time_type writeTime = fs::last_write_time(path, error);
            if (error)
                return 0;
            return std::chrono::duration_cast<std::chrono::seconds>(writeTime.time_since_epoch()).count();
        }

        uint64_t HashFile(const Path& path)
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            if (!stream)
                return 0;
            const Vector<uint8_t> bytes = stream->ReadAll();
            return Hashing::CityHash64(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        bool ReadEmbeddedAssetHeader(const Path& path, AssetFileHeader& header)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return false;

            Array<uint8_t, 512> prefix{};
            stream.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
            const size_t size = static_cast<size_t>(stream.gcount());
            constexpr size_t serializedHeaderSize = sizeof(uint32_t) * 3 + sizeof(int64_t) * 2 + sizeof(uint64_t);
            for (size_t index = 0; index + serializedHeaderSize <= size; index++)
            {
                size_t cursor = index;
                std::memcpy(&header.Magic, prefix.data() + cursor, sizeof(header.Magic));
                if (header.Magic != ASSET_FILE_MAGIC)
                    continue;
                cursor += sizeof(header.Magic);
                std::memcpy(&header.Version, prefix.data() + cursor, sizeof(header.Version));
                cursor += sizeof(header.Version);
                std::memcpy(&header.Type, prefix.data() + cursor, sizeof(header.Type));
                cursor += sizeof(header.Type);
                std::memcpy(&header.SourceTimestamp, prefix.data() + cursor, sizeof(header.SourceTimestamp));
                cursor += sizeof(header.SourceTimestamp);
                std::memcpy(&header.CompileTimestamp, prefix.data() + cursor, sizeof(header.CompileTimestamp));
                cursor += sizeof(header.CompileTimestamp);
                std::memcpy(&header.SourceContentHash, prefix.data() + cursor, sizeof(header.SourceContentHash));
                return true;
            }
            return false;
        }
    } // namespace

    void EditorBuiltInAssetCompiler::CompileChangedAssets()
    {
        const Path editorRoot = GetEditorRoot();
        if (!fs::is_directory(editorRoot / "Resources/Icons"))
            return;
        uint32_t cooked = 0;
        uint32_t failed = 0;

        for (const char* sourceName : TEXTURE_SOURCES)
        {
            const Path sourcePath = editorRoot / sourceName;
            Path assetPath = sourcePath;
            assetPath.replace_extension(".asset");

            const uint64_t sourceHash = HashFile(sourcePath);
            AssetFileHeader header;
            const bool isCurrent = ReadEmbeddedAssetHeader(assetPath, header) && header.Version == TEXTURE_FORMAT_VERSION &&
                                   header.Type == AssetType::Texture && header.SourceContentHash == sourceHash;
            if (isCurrent)
                continue;

            Ref<Texture> texture = Importer::Get().Import<Texture>(sourcePath);
            if (!texture)
            {
                failed++;
                continue;
            }

            texture->SetSourceTimestamp(GetTimestamp(sourcePath));
            texture->SetSourceContentHash(sourceHash);
            AssetManager::TryGet()->Save(texture, assetPath);
            cooked++;
        }

        const Path fontSourcePath = editorRoot / "Resources/Fonts/Roboto/roboto-thin.ttf";
        Path fontAssetPath = fontSourcePath;
        fontAssetPath += ".asset";
        const uint64_t fontSourceHash = HashFile(fontSourcePath);
        AssetFileHeader fontHeader;
        const bool fontIsCurrent = ReadEmbeddedAssetHeader(fontAssetPath, fontHeader) && fontHeader.Version == FONT_FORMAT_VERSION &&
                                   fontHeader.Type == AssetType::Font && fontHeader.SourceContentHash == fontSourceHash;
        if (!fontIsCurrent)
        {
            const Ref<FontImportOptions> options = CreateRef<FontImportOptions>();
            options->AutomaticFontSampling = true;
            options->AutoSizeAtlas = true;
            const Ref<Font> font = Importer::Get().Import<Font>(fontSourcePath, options);
            if (font)
            {
                font->SetSourceTimestamp(GetTimestamp(fontSourcePath));
                font->SetSourceContentHash(fontSourceHash);
                AssetManager::TryGet()->Save(font, fontAssetPath);
                cooked++;
            }
            else
                failed++;
        }

        if (cooked > 0 || failed > 0)
            CW_ENGINE_INFO("Editor built-in assets: {} cooked, {} failed.", cooked, failed);
    }
} // namespace Crowny
