#include "cwpch.h"

#include "Crowny/Build/BuildProfile.h"
#include "Crowny/Common/Yaml.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        String WriteYaml(const Path& path, const YAML::Emitter& emitter)
        {
            if (!emitter.good())
                return emitter.GetLastError();

            std::error_code error;
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path(), error);
            if (error)
                return "Cannot create settings directory '" + path.parent_path().string() + "': " + error.message();

            const Path temporary = path.string() + ".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                if (!stream)
                    return "Cannot write settings file '" + temporary.string() + "'.";
                stream.write(emitter.c_str(), static_cast<std::streamsize>(emitter.size()));
                stream.flush();
                if (!stream)
                    return "Writing settings file failed for '" + temporary.string() + "'.";
            }

            fs::remove(path, error);
            error.clear();
            fs::rename(temporary, path, error);
            if (error)
            {
                fs::remove(temporary);
                return "Cannot publish settings file '" + path.string() + "': " + error.message();
            }
            return {};
        }

        String ReadYaml(const Path& path, YAML::Node& node)
        {
            try
            {
                node = YAML::LoadFile(path.string());
                if (!node.IsMap())
                    return "Settings file '" + path.string() + "' must contain a map.";
                return {};
            }
            catch (const std::exception& error)
            {
                return "Cannot read settings file '" + path.string() + "': " + error.what();
            }
        }

        template <class T> void ReadOptional(const YAML::Node& node, const char* key, T& value)
        {
            if (const YAML::Node child = node[key])
                value = child.as<T>();
        }

        void EmitUuidSequence(YAML::Emitter& output, const char* key, const Vector<UUID>& values)
        {
            output << YAML::Key << key << YAML::Value << YAML::BeginSeq;
            for (const UUID& value : values)
                output << value;
            output << YAML::EndSeq;
        }

        void EmitStringSequence(YAML::Emitter& output, const char* key, const Vector<String>& values)
        {
            output << YAML::Key << key << YAML::Value << YAML::BeginSeq;
            for (const String& value : values)
                output << value;
            output << YAML::EndSeq;
        }

        bool HasDuplicates(const Vector<String>& values)
        {
            Set<String> unique;
            for (const String& value : values)
            {
                if (!unique.insert(value).second)
                    return true;
            }
            return false;
        }

        bool IsKnown(BuildPlatform value) { return value == BuildPlatform::WindowsX64 || value == BuildPlatform::LinuxX64; }

        bool IsKnown(BuildConfiguration value) { return value == BuildConfiguration::Development || value == BuildConfiguration::Shipping; }

        bool IsKnown(QualityTier value)
        {
            return value == QualityTier::Low || value == QualityTier::Medium || value == QualityTier::High || value == QualityTier::Ultra;
        }

        bool IsKnown(RendererPolicy value)
        {
            return value == RendererPolicy::VulkanThenOpenGL || value == RendererPolicy::VulkanOnly || value == RendererPolicy::OpenGLOnly;
        }

        bool IsKnown(CompatibilityPolicy value) { return value == CompatibilityPolicy::Exact || value == CompatibilityPolicy::DeclaredCompatible; }
    } // namespace

    String BuildProfileStore::SaveGameSettings(const Path& path, const GameSettings& settings)
    {
        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Schema" << YAML::Value << settings.Schema;
        output << YAML::Key << "ProductName" << YAML::Value << settings.ProductName;
        output << YAML::Key << "ArtifactName" << YAML::Value << settings.ArtifactName;
        output << YAML::Key << "ProductVersion" << YAML::Value << settings.ProductVersion;
        output << YAML::Key << "Company" << YAML::Value << settings.Company;
        output << YAML::Key << "Icons" << YAML::Value << YAML::BeginMap;
        output << YAML::Key << "Windows" << YAML::Value << settings.WindowsIcon;
        output << YAML::Key << "Linux" << YAML::Value << settings.LinuxIcon;
        output << YAML::EndMap;
        output << YAML::Key << "Window" << YAML::Value << YAML::BeginMap;
        output << YAML::Key << "Width" << YAML::Value << settings.Window.Width;
        output << YAML::Key << "Height" << YAML::Value << settings.Window.Height;
        output << YAML::Key << "Resizable" << YAML::Value << settings.Window.Resizable;
        output << YAML::Key << "Fullscreen" << YAML::Value << settings.Window.Fullscreen;
        output << YAML::EndMap << YAML::EndMap;
        return WriteYaml(path, output);
    }

    String BuildProfileStore::LoadGameSettings(const Path& path, GameSettings& settings)
    {
        YAML::Node node;
        if (String error = ReadYaml(path, node); !error.empty())
            return error;
        try
        {
            GameSettings loaded;
            ReadOptional(node, "Schema", loaded.Schema);
            if (loaded.Schema > GAME_SETTINGS_SCHEMA)
                return "Game settings schema " + std::to_string(loaded.Schema) + " is newer than this editor supports.";
            ReadOptional(node, "ProductName", loaded.ProductName);
            ReadOptional(node, "ArtifactName", loaded.ArtifactName);
            ReadOptional(node, "ProductVersion", loaded.ProductVersion);
            ReadOptional(node, "Company", loaded.Company);
            if (const YAML::Node icons = node["Icons"])
            {
                if (const YAML::Node windows = icons["Windows"])
                    loaded.WindowsIcon = windows.as<UUID>();
                if (const YAML::Node linux = icons["Linux"])
                    loaded.LinuxIcon = linux.as<UUID>();
            }
            if (const YAML::Node window = node["Window"])
            {
                ReadOptional(window, "Width", loaded.Window.Width);
                ReadOptional(window, "Height", loaded.Window.Height);
                ReadOptional(window, "Resizable", loaded.Window.Resizable);
                ReadOptional(window, "Fullscreen", loaded.Window.Fullscreen);
            }
            settings = std::move(loaded);
            return {};
        }
        catch (const std::exception& error)
        {
            return "Invalid game settings in '" + path.string() + "': " + error.what();
        }
    }

    String BuildProfileStore::SaveProfile(const Path& path, const BuildProfile& profile)
    {
        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Schema" << YAML::Value << profile.Schema;
        output << YAML::Key << "Id" << YAML::Value << profile.Id;
        output << YAML::Key << "Name" << YAML::Value << profile.Name;
        EmitUuidSequence(output, "SceneOrder", profile.SceneOrder);
        output << YAML::Key << "StartupScene" << YAML::Value << profile.StartupScene;
        output << YAML::Key << "ContentRoots" << YAML::Value << YAML::BeginSeq;
        for (const ContentRoot& root : profile.ContentRoots)
        {
            if (root.Kind != ContentRootKind::Asset && root.Kind != ContentRootKind::Folder)
                return "Build profile '" + profile.Name + "' contains an invalid content root kind.";
            output << YAML::BeginMap;
            output << YAML::Key << "Kind" << YAML::Value << (root.Kind == ContentRootKind::Asset ? "Asset" : "Folder");
            output << YAML::Key << "Path" << YAML::Value << NormalizePortableBuildPath(root.PathValue);
            if (!root.AssetId.Empty())
                output << YAML::Key << "AssetId" << YAML::Value << root.AssetId;
            output << YAML::EndMap;
        }
        output << YAML::EndSeq;
        output << YAML::Key << "ExcludedAssets" << YAML::Value << YAML::BeginSeq;
        for (const UUID& excluded : profile.ExcludedAssets)
            output << excluded;
        output << YAML::EndSeq;
        EmitStringSequence(output, "Symbols", profile.Symbols);
        output << YAML::Key << "Quality" << YAML::Value << YAML::BeginMap;
        output << YAML::Key << "Default" << YAML::Value << ToString(profile.DefaultQuality);
        output << YAML::Key << "Allowed" << YAML::Value << YAML::BeginSeq;
        for (QualityTier tier : profile.AllowedQuality)
            output << ToString(tier);
        output << YAML::EndSeq << YAML::EndMap;
        output << YAML::Key << "Targets" << YAML::Value << YAML::BeginSeq;
        for (const BuildTarget& target : profile.Targets)
        {
            output << YAML::BeginMap;
            output << YAML::Key << "Id" << YAML::Value << target.Id;
            output << YAML::Key << "Platform" << YAML::Value << ToString(target.Platform);
            output << YAML::Key << "Configuration" << YAML::Value << ToString(target.Configuration);
            EmitStringSequence(output, "Symbols", target.Symbols);
            output << YAML::Key << "DefaultQuality" << YAML::Value << ToString(target.DefaultQuality);
            output << YAML::Key << "Renderers" << YAML::Value << ToString(target.Renderers);
            output << YAML::Key << "Compatibility" << YAML::Value << ToString(target.Compatibility);
            output << YAML::Key << "Archive" << YAML::Value << target.Archive;
            output << YAML::Key << "IncludeSymbols" << YAML::Value << target.IncludeSymbols;
            output << YAML::EndMap;
        }
        output << YAML::EndSeq << YAML::EndMap;
        return WriteYaml(path, output);
    }

    String BuildProfileStore::LoadProfile(const Path& path, BuildProfile& profile)
    {
        YAML::Node node;
        if (String error = ReadYaml(path, node); !error.empty())
            return error;
        try
        {
            BuildProfile loaded;
            ReadOptional(node, "Schema", loaded.Schema);
            if (loaded.Schema > BUILD_PROFILE_SCHEMA)
                return "Build profile schema " + std::to_string(loaded.Schema) + " is newer than this editor supports.";
            ReadOptional(node, "Id", loaded.Id);
            ReadOptional(node, "Name", loaded.Name);
            if (const YAML::Node scenes = node["SceneOrder"])
            {
                for (const YAML::Node scene : scenes)
                    loaded.SceneOrder.push_back(scene.as<UUID>());
            }
            ReadOptional(node, "StartupScene", loaded.StartupScene);
            if (const YAML::Node roots = node["ContentRoots"])
            {
                for (const YAML::Node rootNode : roots)
                {
                    ContentRoot root;
                    root.Kind = rootNode["Kind"].as<String>("Asset") == "Folder" ? ContentRootKind::Folder : ContentRootKind::Asset;
                    root.PathValue = rootNode["Path"].as<String>("");
                    ReadOptional(rootNode, "AssetId", root.AssetId);
                    loaded.ContentRoots.push_back(std::move(root));
                }
            }
            if (const YAML::Node exclusions = node["ExcludedAssets"])
            {
                for (const YAML::Node exclusion : exclusions)
                    loaded.ExcludedAssets.push_back(exclusion.as<UUID>());
            }
            if (const YAML::Node symbols = node["Symbols"])
            {
                for (const YAML::Node symbol : symbols)
                    loaded.Symbols.push_back(symbol.as<String>());
            }
            if (const YAML::Node quality = node["Quality"])
            {
                QualityTier parsed;
                if (!TryParseQualityTier(quality["Default"].as<String>("High"), parsed))
                    return "Build profile has an unknown default quality tier.";
                loaded.DefaultQuality = parsed;
                loaded.AllowedQuality.clear();
                if (const YAML::Node allowed = quality["Allowed"])
                {
                    for (const YAML::Node tier : allowed)
                    {
                        if (!TryParseQualityTier(tier.as<String>(), parsed))
                            return "Build profile has an unknown allowed quality tier.";
                        loaded.AllowedQuality.push_back(parsed);
                    }
                }
            }
            if (const YAML::Node targets = node["Targets"])
            {
                for (const YAML::Node targetNode : targets)
                {
                    BuildTarget target;
                    ReadOptional(targetNode, "Id", target.Id);
                    if (!TryParseBuildPlatform(targetNode["Platform"].as<String>(""), target.Platform))
                        return "Build profile has an unknown target platform.";
                    if (!TryParseBuildConfiguration(targetNode["Configuration"].as<String>(""), target.Configuration))
                        return "Build profile has an unknown build configuration.";
                    if (const YAML::Node symbols = targetNode["Symbols"])
                    {
                        for (const YAML::Node symbol : symbols)
                            target.Symbols.push_back(symbol.as<String>());
                    }
                    QualityTier tier;
                    if (!TryParseQualityTier(targetNode["DefaultQuality"].as<String>(ToString(loaded.DefaultQuality)), tier))
                        return "Build target has an unknown default quality tier.";
                    target.DefaultQuality = tier;
                    if (!TryParseRendererPolicy(targetNode["Renderers"].as<String>("VulkanThenOpenGL"), target.Renderers))
                        return "Build target has an unknown renderer policy.";
                    if (!TryParseCompatibilityPolicy(targetNode["Compatibility"].as<String>("Exact"), target.Compatibility))
                        return "Build target has an unknown compatibility policy.";
                    ReadOptional(targetNode, "Archive", target.Archive);
                    ReadOptional(targetNode, "IncludeSymbols", target.IncludeSymbols);
                    loaded.Targets.push_back(std::move(target));
                }
            }
            profile = std::move(loaded);
            return {};
        }
        catch (const std::exception& error)
        {
            return "Invalid build profile in '" + path.string() + "': " + error.what();
        }
    }

    BuildProfile BuildProfileStore::CreateDefault(BuildPlatform hostPlatform)
    {
        BuildProfile profile;
        profile.Id = UuidGenerator::Generate();
        BuildTarget target;
        target.Id = UuidGenerator::Generate();
        target.Platform = hostPlatform;
        target.Configuration = BuildConfiguration::Development;
        profile.Targets.push_back(target);
        return profile;
    }

    BuildValidation ValidateBuildProfile(const GameSettings& game, const BuildProfile& profile)
    {
        BuildValidation result;
        if (game.Schema != GAME_SETTINGS_SCHEMA)
            result.Error("game.schema.unsupported", "The game settings schema is not supported.");
        if (profile.Schema != BUILD_PROFILE_SCHEMA)
            result.Error("profile.schema.unsupported", "The build profile schema is not supported.");
        if (game.ProductName.empty())
            result.Error("game.product_name.empty", "Enter a product name.", "ProductName");
        if (game.ProductVersion.empty())
            result.Error("game.product_version.empty", "Enter a product version.", "ProductVersion");
        if (game.Window.Width == 0 || game.Window.Height == 0)
            result.Error("game.window.size.invalid", "The initial window dimensions must be greater than zero.", "Window");
        const String sanitized = SanitizeArtifactName(game.ArtifactName);
        if (game.ArtifactName.empty() || sanitized != game.ArtifactName || !IsSafeRelativeBuildPath(game.ArtifactName))
            result.Error("game.artifact_name.invalid", "Artifact name must contain only letters, numbers, '-' or '_'.", "ArtifactName");
        if (profile.Id.Empty())
            result.Error("profile.id.empty", "The build profile has no stable ID.", profile.Name);
        if (profile.Name.empty())
            result.Error("profile.name.empty", "Enter a build profile name.");
        if (profile.SceneOrder.empty())
            result.Error("profile.scenes.empty", "Add at least one scene to the build profile.", profile.Name);
        if (profile.StartupScene.Empty())
            result.Error("profile.startup_scene.empty", "Choose a startup scene.", profile.Name);
        else if (std::find(profile.SceneOrder.begin(), profile.SceneOrder.end(), profile.StartupScene) == profile.SceneOrder.end())
            result.Error("profile.startup_scene.not_in_scene_list", "The startup scene must be enabled in the ordered scene list.",
                         profile.StartupScene.ToString());
        Set<UUID> scenes;
        for (const UUID& scene : profile.SceneOrder)
        {
            if (scene.Empty())
                result.Error("profile.scene.empty", "The scene list contains an empty asset ID.", profile.Name);
            else if (!scenes.insert(scene).second)
                result.Error("profile.scene.duplicate", "The scene list contains the same scene more than once.", scene.ToString());
        }
        if (profile.Targets.empty())
            result.Error("profile.targets.empty", "Add at least one Windows x64 or Linux x64 target.", profile.Name);
        Set<UUID> targetIds;
        for (const BuildTarget& target : profile.Targets)
        {
            if (target.Id.Empty() || !targetIds.insert(target.Id).second)
                result.Error("profile.target.id.invalid", "Each build target needs a unique stable ID.", profile.Name);
            if (!IsKnown(target.Platform))
                result.Error("profile.target.platform.invalid", "A build target has an invalid platform.", profile.Name);
            if (!IsKnown(target.Configuration))
                result.Error("profile.target.configuration.invalid", "A build target has an invalid configuration.", profile.Name);
            if (!IsKnown(target.DefaultQuality))
                result.Error("profile.target.quality.invalid", "A build target has an invalid default quality tier.", profile.Name);
            if (!IsKnown(target.Renderers))
                result.Error("profile.target.renderers.invalid", "A build target has an invalid renderer policy.", profile.Name);
            if (!IsKnown(target.Compatibility))
                result.Error("profile.target.compatibility.invalid", "A build target has an invalid compatibility policy.", profile.Name);
            if (HasDuplicates(target.Symbols))
                result.Error("profile.target.symbol.duplicate", "C# symbols must be unique within a build target.", profile.Name);
            if (target.DefaultQuality != profile.DefaultQuality &&
                std::find(profile.AllowedQuality.begin(), profile.AllowedQuality.end(), target.DefaultQuality) == profile.AllowedQuality.end())
                result.Error("profile.target.quality.not_allowed", "A target's default quality tier must be allowed by its profile.",
                             ToString(target.DefaultQuality));
        }
        if (profile.AllowedQuality.empty() ||
            std::find(profile.AllowedQuality.begin(), profile.AllowedQuality.end(), profile.DefaultQuality) == profile.AllowedQuality.end())
            result.Error("profile.quality.default_not_allowed", "The profile's default quality tier must be in its allowed tier list.",
                         ToString(profile.DefaultQuality));
        Set<QualityTier> allowedQuality;
        for (QualityTier tier : profile.AllowedQuality)
        {
            if (!IsKnown(tier))
                result.Error("profile.quality.invalid", "The profile contains an invalid quality tier.", profile.Name);
            else if (!allowedQuality.insert(tier).second)
                result.Error("profile.quality.duplicate", "The profile contains a duplicate quality tier.", ToString(tier));
        }
        if (!IsKnown(profile.DefaultQuality))
            result.Error("profile.quality.invalid", "The profile has an invalid default quality tier.", profile.Name);
        if (HasDuplicates(profile.Symbols))
            result.Error("profile.symbol.duplicate", "C# symbols must be unique within a build profile.", profile.Name);
        Set<UUID> assetRoots;
        Set<String> folderRoots;
        for (const ContentRoot& root : profile.ContentRoots)
        {
            if (root.Kind != ContentRootKind::Asset && root.Kind != ContentRootKind::Folder)
            {
                result.Error("profile.content_root.kind_invalid", "A content root has an invalid kind.", profile.Name);
                continue;
            }
            if (!root.PathValue.empty() && !IsSafeRelativeBuildPath(root.PathValue))
                result.Error("profile.content_root.path_unsafe", "Content roots must be project-relative and cannot contain '..'.",
                             root.PathValue.generic_string());
            if (root.Kind == ContentRootKind::Asset && root.AssetId.Empty())
                result.Error("profile.content_root.empty", "An asset root needs an asset ID.", profile.Name);
            else if (root.Kind == ContentRootKind::Asset && !assetRoots.insert(root.AssetId).second)
                result.Error("profile.content_root.duplicate", "The same asset root appears more than once.", root.AssetId.ToString());
            if (root.Kind == ContentRootKind::Asset && !root.PathValue.empty())
                result.Error("profile.content_root.ambiguous", "An asset root cannot also contain a folder path.", root.PathValue.generic_string());
            if (root.Kind == ContentRootKind::Folder && root.PathValue.empty())
                result.Error("profile.content_root.empty", "A folder root needs a project-relative logical path.", profile.Name);
            else if (root.Kind == ContentRootKind::Folder)
            {
                String folder = NormalizePortableBuildPath(root.PathValue);
                std::transform(folder.begin(), folder.end(), folder.begin(),
                               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                if (!folderRoots.insert(std::move(folder)).second)
                    result.Error("profile.content_root.duplicate", "The same folder root appears more than once.", root.PathValue.generic_string());
            }
            if (root.Kind == ContentRootKind::Folder && !root.AssetId.Empty())
                result.Error("profile.content_root.ambiguous", "A folder root cannot also contain an asset ID.", root.AssetId.ToString());
        }
        Set<UUID> exclusions;
        for (const UUID& excluded : profile.ExcludedAssets)
        {
            if (excluded.Empty())
                result.Error("profile.exclusion.empty", "The excluded asset list contains an empty ID.", profile.Name);
            else if (!exclusions.insert(excluded).second)
                result.Error("profile.exclusion.duplicate", "The excluded asset list contains a duplicate ID.", excluded.ToString());
        }
        return result;
    }
} // namespace Crowny
