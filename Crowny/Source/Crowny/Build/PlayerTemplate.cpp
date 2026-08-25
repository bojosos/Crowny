#include "cwpch.h"

#include "Crowny/Build/ContentPack.h"
#include "Crowny/Build/PlayerTemplate.h"
#include "Crowny/Common/Yaml.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        String WriteManifest(const Path& path, const YAML::Emitter& output)
        {
            if (!output.good())
                return output.GetLastError();
            std::error_code error;
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path(), error);
            if (error)
                return "Cannot create template manifest directory: " + error.message();
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream)
                return "Cannot write template manifest '" + path.string() + "'.";
            stream.write(output.c_str(), static_cast<std::streamsize>(output.size()));
            return stream ? String() : "Writing template manifest failed for '" + path.string() + "'.";
        }

        bool ContainsRenderer(const Vector<RendererBackend>& renderers, RendererBackend renderer)
        {
            return std::find(renderers.begin(), renderers.end(), renderer) != renderers.end();
        }

        bool IsPathWithin(const Path& root, const Path& candidate)
        {
            std::error_code error;
            const Path normalizedRoot = fs::weakly_canonical(root, error);
            if (error)
                return false;
            const Path normalizedCandidate = fs::weakly_canonical(candidate, error);
            if (error)
                return false;
            const Path relative = normalizedCandidate.lexically_relative(normalizedRoot);
            if (relative.empty())
                return normalizedCandidate == normalizedRoot;
            return !relative.is_absolute() && *relative.begin() != "..";
        }

        String FoldPortablePath(String value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        bool IsSha256(StringView value)
        {
            return value.size() == 64 &&
                   std::all_of(value.begin(), value.end(), [](unsigned char character) { return std::isxdigit(character) != 0; });
        }

        bool HashesMatch(StringView left, StringView right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                                                             [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
        }

        struct ScopedDirectoryRemoval
        {
            explicit ScopedDirectoryRemoval(Path path) : Directory(std::move(path)) {}

            ~ScopedDirectoryRemoval()
            {
                if (Enabled)
                {
                    std::error_code error;
                    fs::remove_all(Directory, error);
                }
            }

            Path Directory;
            bool Enabled = true;
        };

        Path UniqueSibling(const Path& directory, StringView suffix)
        {
            const Path parent = directory.parent_path();
            const String base = directory.filename().string();
            return parent / (base + String(suffix) + UuidGenerator::Generate().ToString());
        }

        String PublishStagedDirectory(const Path& temporary, const Path& destination)
        {
            std::error_code error;
            const bool destinationExists = fs::exists(destination, error);
            if (error)
                return "Cannot inspect existing staging directory: " + error.message();

            const Path backup = UniqueSibling(destination, ".previous-");
            if (destinationExists)
            {
                fs::rename(destination, backup, error);
                if (error)
                    return "Cannot preserve existing staging directory: " + error.message();
            }

            fs::rename(temporary, destination, error);
            if (error)
            {
                String message = "Cannot publish staging directory: " + error.message();
                if (destinationExists)
                {
                    std::error_code restoreError;
                    fs::rename(backup, destination, restoreError);
                    if (restoreError)
                        message += "; restoring the previous directory also failed: " + restoreError.message();
                }
                return message;
            }

            if (destinationExists)
            {
                fs::remove_all(backup, error);
                if (error)
                    return "Player template was staged, but its previous directory could not be removed: " + error.message();
            }
            return {};
        }
    } // namespace

    String PlayerTemplateStore::Save(const Path& path, const PlayerTemplateManifest& manifest)
    {
        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Schema" << YAML::Value << manifest.Schema;
        output << YAML::Key << "EngineVersion" << YAML::Value << manifest.EngineVersion;
        output << YAML::Key << "CompatibleEngineVersions" << YAML::Value << YAML::BeginSeq;
        for (const String& version : manifest.CompatibleEngineVersions)
            output << version;
        output << YAML::EndSeq;
        output << YAML::Key << "PlayerAbi" << YAML::Value << manifest.PlayerAbi;
        output << YAML::Key << "ContentSchema" << YAML::Value << YAML::Flow << YAML::BeginSeq << manifest.ContentSchemaMin
               << manifest.ContentSchemaMax << YAML::EndSeq;
        output << YAML::Key << "Platform" << YAML::Value << ToString(manifest.Platform);
        output << YAML::Key << "Configuration" << YAML::Value << ToString(manifest.Configuration);
        output << YAML::Key << "Renderers" << YAML::Value << YAML::BeginSeq;
        for (RendererBackend renderer : manifest.Renderers)
            output << ToString(renderer);
        output << YAML::EndSeq;
        Vector<PlayerTemplateFile> files = manifest.Files;
        std::sort(files.begin(), files.end(), [](const PlayerTemplateFile& left, const PlayerTemplateFile& right) {
            return NormalizePortableBuildPath(left.RelativePath) < NormalizePortableBuildPath(right.RelativePath);
        });
        output << YAML::Key << "Files" << YAML::Value << YAML::BeginSeq;
        for (const PlayerTemplateFile& file : files)
        {
            output << YAML::BeginMap;
            output << YAML::Key << "Path" << YAML::Value << NormalizePortableBuildPath(file.RelativePath);
            output << YAML::Key << "Sha256" << YAML::Value << file.Sha256;
            output << YAML::Key << "Executable" << YAML::Value << file.Executable;
            output << YAML::EndMap;
        }
        output << YAML::EndSeq << YAML::EndMap;
        return WriteManifest(path, output);
    }

    String PlayerTemplateStore::Load(const Path& path, PlayerTemplateManifest& manifest)
    {
        try
        {
            const YAML::Node root = YAML::LoadFile(path.string());
            if (!root.IsMap())
                return "Player template manifest root must be a map.";
            PlayerTemplateManifest loaded;
            loaded.Schema = root["Schema"].as<uint32_t>(0);
            if (loaded.Schema > PLAYER_TEMPLATE_MANIFEST_SCHEMA)
                return "Player template manifest schema is newer than this builder supports.";
            loaded.EngineVersion = root["EngineVersion"].as<String>("");
            if (const YAML::Node versions = root["CompatibleEngineVersions"])
            {
                for (const YAML::Node version : versions)
                    loaded.CompatibleEngineVersions.push_back(version.as<String>());
            }
            loaded.PlayerAbi = root["PlayerAbi"].as<uint32_t>(0);
            const YAML::Node contentSchema = root["ContentSchema"];
            if (!contentSchema || !contentSchema.IsSequence() || contentSchema.size() != 2)
                return "Player template ContentSchema must contain its minimum and maximum readable schema.";
            loaded.ContentSchemaMin = contentSchema[0].as<uint32_t>();
            loaded.ContentSchemaMax = contentSchema[1].as<uint32_t>();
            if (!TryParseBuildPlatform(root["Platform"].as<String>(""), loaded.Platform))
                return "Player template has an unknown platform.";
            if (!TryParseBuildConfiguration(root["Configuration"].as<String>(""), loaded.Configuration))
                return "Player template has an unknown build configuration.";
            if (const YAML::Node renderers = root["Renderers"])
            {
                for (const YAML::Node rendererNode : renderers)
                {
                    RendererBackend renderer;
                    if (!TryParseRendererBackend(rendererNode.as<String>(), renderer))
                        return "Player template has an unknown renderer.";
                    loaded.Renderers.push_back(renderer);
                }
            }
            const YAML::Node files = root["Files"];
            if (!files || !files.IsSequence())
                return "Player template Files value must be a sequence.";
            for (const YAML::Node fileNode : files)
            {
                PlayerTemplateFile file;
                file.RelativePath = fileNode["Path"].as<String>("");
                file.Sha256 = fileNode["Sha256"].as<String>("");
                file.Executable = fileNode["Executable"].as<bool>(false);
                loaded.Files.push_back(std::move(file));
            }
            manifest = std::move(loaded);
            return {};
        }
        catch (const std::exception& error)
        {
            return "Cannot read player template manifest '" + path.string() + "': " + error.what();
        }
    }

    String PlayerTemplateStore::CreateManifest(const Path& root, PlayerTemplateManifest manifest, const Vector<Path>& executableFiles,
                                               PlayerTemplateManifest& output)
    {
        if (!fs::is_directory(root))
            return "Player template root is not a directory: '" + root.string() + "'.";
        Set<String> executablePaths;
        for (const Path& path : executableFiles)
        {
            const Path relative = path.is_absolute() ? path.lexically_relative(root) : path;
            if (!IsSafeRelativeBuildPath(relative))
                return "Executable path is outside the player template: '" + path.string() + "'.";
            executablePaths.insert(NormalizePortableBuildPath(relative));
        }

        manifest.Files.clear();
        std::error_code error;
        fs::recursive_directory_iterator iterator(root, error);
        const fs::recursive_directory_iterator end;
        if (error)
            return "Cannot enumerate player template root: " + error.message();
        for (; iterator != end; iterator.increment(error))
        {
            if (error)
                return "Cannot enumerate player template root: " + error.message();
            const fs::directory_entry& entry = *iterator;
            const bool regularFile = entry.is_regular_file(error);
            if (error)
                return "Cannot inspect template file '" + entry.path().string() + "': " + error.message();
            if (!regularFile)
                continue;
            const Path relative = entry.path().lexically_relative(root);
            if (relative == "template.yaml")
                continue;
            if (!IsSafeRelativeBuildPath(relative) || !IsPathWithin(root, entry.path()))
                return "Template file resolves outside its root: '" + entry.path().string() + "'.";
            const String hash = ComputeFileSha256(entry.path());
            if (hash.empty())
                return "Cannot read or hash template file '" + entry.path().string() + "'.";
            manifest.Files.push_back({ NormalizePortableBuildPath(relative), hash, executablePaths.contains(NormalizePortableBuildPath(relative)) });
        }
        if (error)
            return "Cannot enumerate player template root: " + error.message();
        std::sort(manifest.Files.begin(), manifest.Files.end(), [](const PlayerTemplateFile& left, const PlayerTemplateFile& right) {
            return NormalizePortableBuildPath(left.RelativePath) < NormalizePortableBuildPath(right.RelativePath);
        });
        output = std::move(manifest);
        return {};
    }

    BuildValidation ValidatePlayerTemplate(const Path& root, const PlayerTemplateManifest& manifest, const PlayerTemplateRequest& request)
    {
        BuildValidation validation;
        if (!fs::is_directory(root))
        {
            validation.Error("template.root.missing", "Player template directory does not exist.", root.string());
            return validation;
        }
        if (manifest.Schema != PLAYER_TEMPLATE_MANIFEST_SCHEMA)
            validation.Error("template.schema.unsupported", "Player template manifest schema is unsupported.", std::to_string(manifest.Schema));
        const bool exactEngine = manifest.EngineVersion == request.EngineVersion;
        const bool declaredEngine = std::find(manifest.CompatibleEngineVersions.begin(), manifest.CompatibleEngineVersions.end(),
                                              request.EngineVersion) != manifest.CompatibleEngineVersions.end();
        if (!exactEngine && (request.Compatibility == CompatibilityPolicy::Exact || !declaredEngine))
            validation.Error("template.engine_version.incompatible",
                             "Player template does not declare compatibility with engine " + request.EngineVersion + ".", manifest.EngineVersion);
        else if (!exactEngine)
            validation.Warn("template.engine_version.compatible",
                            "Using a player template that declares compatibility with engine " + request.EngineVersion + ".", manifest.EngineVersion);
        if (manifest.PlayerAbi != request.PlayerAbi)
            validation.Error("template.player_abi.incompatible", "Player ABI does not match the builder.", std::to_string(manifest.PlayerAbi));
        if (request.ContentSchema < manifest.ContentSchemaMin || request.ContentSchema > manifest.ContentSchemaMax)
            validation.Error("template.content_schema.incompatible",
                             "Player template cannot read content schema " + std::to_string(request.ContentSchema) + ".");
        if (manifest.Platform != request.Platform)
            validation.Error("template.platform.mismatch", "Player template target does not match the build target.", ToString(manifest.Platform));
        if (manifest.Configuration != request.Configuration)
            validation.Error("template.configuration.mismatch", "Player template configuration does not match the build target.",
                             ToString(manifest.Configuration));
        for (RendererBackend renderer : request.RequiredRenderers)
        {
            if (!ContainsRenderer(manifest.Renderers, renderer))
                validation.Error("template.renderer.missing", String("Player template is missing the ") + ToString(renderer) + " renderer.",
                                 ToString(renderer));
        }

        Set<String> paths;
        Set<String> foldedPaths;
        for (const PlayerTemplateFile& file : manifest.Files)
        {
            const String relative = NormalizePortableBuildPath(file.RelativePath);
            if (!IsSafeRelativeBuildPath(file.RelativePath))
            {
                validation.Error("template.file.path_unsafe", "Template file path is absolute or escapes its root.", relative);
                continue;
            }
            if (!paths.insert(relative).second)
            {
                validation.Error("template.file.path_duplicate", "Template manifest contains a duplicate file path.", relative);
                continue;
            }
            if (!foldedPaths.insert(FoldPortablePath(relative)).second)
            {
                validation.Error("template.file.path_case_collision", "Template manifest contains file paths that differ only by letter case.",
                                 relative);
                continue;
            }
            const bool hashPresent = !file.Sha256.empty();
            const bool hashValid = IsSha256(file.Sha256);
            if (!hashPresent)
                validation.Error("template.file.hash_missing", "Template file has no declared SHA-256 hash.", relative);
            else if (!hashValid)
                validation.Error("template.file.hash_invalid", "Template file has an invalid SHA-256 hash.", relative);
            const Path absolute = root / file.RelativePath;
            if (!fs::is_regular_file(absolute) || !IsPathWithin(root, absolute))
            {
                validation.Error("template.file.missing", "Template file is missing or resolves outside the template.", relative);
                continue;
            }
            const String actualHash = ComputeFileSha256(absolute);
            if (actualHash.empty())
                validation.Error("template.file.hash_unreadable", "Template file cannot be read or hashed.", relative);
            else if (hashValid && !HashesMatch(actualHash, file.Sha256))
                validation.Error("template.file.hash_mismatch", "Template file hash does not match its manifest.", relative);
        }
        if (manifest.Files.empty())
            validation.Error("template.files.empty", "Player template manifest contains no files.", root.string());
        return validation;
    }

    String StagePlayerTemplate(const Path& root, const PlayerTemplateManifest& manifest, const Path& stageDirectory)
    {
        if (stageDirectory.empty() || stageDirectory.filename().empty())
            return "Player template staging directory must name a directory.";
        if (manifest.Files.empty())
            return "Player template manifest contains no files.";
        if (IsPathWithin(root, stageDirectory) || IsPathWithin(stageDirectory, root))
            return "Player template source and staging directories must not overlap.";

        struct PreparedFile
        {
            const PlayerTemplateFile* ManifestFile;
            Path Source;
            String Relative;
        };
        Vector<PreparedFile> preparedFiles;
        preparedFiles.reserve(manifest.Files.size());
        Set<String> paths;
        Set<String> foldedPaths;
        for (const PlayerTemplateFile& file : manifest.Files)
        {
            if (!IsSafeRelativeBuildPath(file.RelativePath))
                return "Template manifest contains unsafe path '" + file.RelativePath.string() + "'.";
            const String relative = NormalizePortableBuildPath(file.RelativePath);
            if (!paths.insert(relative).second)
                return "Template manifest contains duplicate path '" + relative + "'.";
            if (!foldedPaths.insert(FoldPortablePath(relative)).second)
                return "Template manifest contains paths that differ only by letter case: '" + relative + "'.";
            if (!IsSha256(file.Sha256))
                return "Template file has a missing or invalid SHA-256 hash: '" + relative + "'.";
            const Path source = root / file.RelativePath;
            if (!IsPathWithin(root, source) || !fs::is_regular_file(source))
                return "Template file is missing or resolves outside its root: '" + relative + "'.";
            preparedFiles.push_back({ &file, source, relative });
        }

        std::error_code error;
        const Path parent = stageDirectory.parent_path();
        if (!parent.empty())
            fs::create_directories(parent, error);
        if (error)
            return "Cannot create staging parent directory: " + error.message();

        const Path temporary = UniqueSibling(stageDirectory, ".tmp-");
        fs::create_directory(temporary, error);
        if (error)
            return "Cannot create temporary staging directory: " + error.message();
        ScopedDirectoryRemoval temporaryCleanup(temporary);

        for (const PreparedFile& prepared : preparedFiles)
        {
            const PlayerTemplateFile& file = *prepared.ManifestFile;
            const String sourceHash = ComputeFileSha256(prepared.Source);
            if (sourceHash.empty())
                return "Cannot read or hash template file '" + prepared.Relative + "'.";
            if (!HashesMatch(sourceHash, file.Sha256))
                return "Template file hash changed before staging: '" + prepared.Relative + "'.";

            const Path destination = temporary / file.RelativePath;
            fs::create_directories(destination.parent_path(), error);
            if (error)
                return "Cannot create staging subdirectory: " + error.message();
            fs::copy_file(prepared.Source, destination, fs::copy_options::none, error);
            if (error)
                return "Cannot stage template file '" + prepared.Relative + "': " + error.message();
            const String stagedHash = ComputeFileSha256(destination);
            if (stagedHash.empty() || !HashesMatch(stagedHash, file.Sha256))
                return "Staged template file hash does not match its manifest: '" + prepared.Relative + "'.";
#ifndef CW_PLATFORM_WIN32
            if (file.Executable)
            {
                fs::permissions(destination, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, error);
                if (error)
                    return "Cannot set executable permissions on '" + file.RelativePath.string() + "': " + error.message();
            }
#endif
        }

        const String publishError = PublishStagedDirectory(temporary, stageDirectory);
        if (publishError.empty())
            temporaryCleanup.Enabled = false;
        return publishError;
    }
} // namespace Crowny
