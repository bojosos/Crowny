#include "cwpch.h"

#include "Crowny/Build/BuildManifest.h"
#include "Crowny/Common/Version.h"
#include "Crowny/Common/Yaml.h"

#include <cctype>
#include <fstream>

namespace Crowny
{
    namespace
    {
        String Lowercase(String value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        template <class T> void ReadOptional(const YAML::Node& node, const char* key, T& value)
        {
            if (const YAML::Node child = node[key])
                value = child.as<T>();
        }

        String ValidationError(const BuildValidation& validation)
        {
            String result;
            for (const BuildIssue& issue : validation.Issues)
            {
                if (issue.Severity != BuildIssueSeverity::Error)
                    continue;
                if (!result.empty())
                    result += " ";
                result += issue.Code + ": " + issue.Message;
            }
            return result;
        }

        String ReplaceFile(const Path& temporary, const Path& destination)
        {
            std::error_code error;
            const Path backup = destination.string() + ".previous";
            fs::remove(backup, error);
            error.clear();

            const bool hadDestination = fs::exists(destination, error);
            if (error)
                return "Cannot inspect build manifest destination '" + destination.string() + "': " + error.message();
            if (hadDestination)
            {
                fs::rename(destination, backup, error);
                if (error)
                    return "Cannot preserve existing build manifest '" + destination.string() + "': " + error.message();
            }

            fs::rename(temporary, destination, error);
            if (error)
            {
                const String message = "Cannot publish build manifest '" + destination.string() + "': " + error.message();
                if (hadDestination)
                {
                    error.clear();
                    fs::rename(backup, destination, error);
                }
                return message;
            }

            if (hadDestination)
                fs::remove(backup, error);
            return {};
        }

        String WriteYaml(const Path& path, const YAML::Emitter& output)
        {
            if (!output.good())
                return output.GetLastError();

            std::error_code error;
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path(), error);
            if (error)
                return "Cannot create build manifest directory '" + path.parent_path().string() + "': " + error.message();

            const Path temporary = path.string() + ".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                if (!stream)
                    return "Cannot write build manifest '" + temporary.string() + "'.";
                stream.write(output.c_str(), static_cast<std::streamsize>(output.size()));
                stream.flush();
                if (!stream)
                {
                    stream.close();
                    fs::remove(temporary, error);
                    return "Writing build manifest failed for '" + temporary.string() + "'.";
                }
            }
            return ReplaceFile(temporary, path);
        }

        String ReadYaml(const Path& path, YAML::Node& node)
        {
            try
            {
                node = YAML::LoadFile(path.string());
                return node.IsMap() ? String() : "Build manifest must contain a map.";
            }
            catch (const std::exception& error)
            {
                return "Cannot read build manifest '" + path.string() + "': " + error.what();
            }
        }

        bool IsKnown(BuildPlatform value) { return value == BuildPlatform::WindowsX64 || value == BuildPlatform::LinuxX64; }

        bool IsKnown(BuildConfiguration value) { return value == BuildConfiguration::Development || value == BuildConfiguration::Shipping; }

        bool IsKnown(RendererPolicy value)
        {
            return value == RendererPolicy::VulkanThenOpenGL || value == RendererPolicy::VulkanOnly || value == RendererPolicy::OpenGLOnly;
        }

        bool IsKnown(QualityTier value)
        {
            return value == QualityTier::Low || value == QualityTier::Medium || value == QualityTier::High || value == QualityTier::Ultra;
        }
    } // namespace

    String BuildManifestStore::Save(const Path& path, const BuildManifest& manifest)
    {
        const BuildValidation validation = ValidateBuildManifest(manifest);
        if (!validation.IsValid())
            return ValidationError(validation);

        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Schema" << YAML::Value << manifest.Schema;
        output << YAML::Key << "PlayerAbi" << YAML::Value << manifest.PlayerAbi;
        output << YAML::Key << "ContentSchema" << YAML::Value << manifest.ContentSchema;
        output << YAML::Key << "ProductName" << YAML::Value << manifest.ProductName;
        output << YAML::Key << "ArtifactName" << YAML::Value << manifest.ArtifactName;
        output << YAML::Key << "ProductVersion" << YAML::Value << manifest.ProductVersion;
        output << YAML::Key << "Company" << YAML::Value << manifest.Company;
        output << YAML::Key << "EngineVersion" << YAML::Value << manifest.EngineVersion;
        output << YAML::Key << "MonoVersion" << YAML::Value << manifest.MonoVersion;
        output << YAML::Key << "Platform" << YAML::Value << ToString(manifest.Platform);
        output << YAML::Key << "Configuration" << YAML::Value << ToString(manifest.Configuration);
        output << YAML::Key << "Renderers" << YAML::Value << ToString(manifest.Renderers);
        output << YAML::Key << "Quality" << YAML::Value << YAML::BeginMap;
        output << YAML::Key << "Default" << YAML::Value << ToString(manifest.DefaultQuality);
        output << YAML::Key << "Allowed" << YAML::Value << YAML::BeginSeq;
        for (QualityTier tier : manifest.AllowedQuality)
            output << ToString(tier);
        output << YAML::EndSeq << YAML::EndMap;
        output << YAML::Key << "StartupScene" << YAML::Value << manifest.StartupScene;
        output << YAML::Key << "Scenes" << YAML::Value << YAML::BeginSeq;
        Vector<BuildManifestScene> scenes = manifest.Scenes;
        std::sort(scenes.begin(), scenes.end(),
                  [](const BuildManifestScene& left, const BuildManifestScene& right) { return left.Order < right.Order; });
        for (const BuildManifestScene& scene : scenes)
        {
            output << YAML::BeginMap;
            output << YAML::Key << "Order" << YAML::Value << scene.Order;
            output << YAML::Key << "Id" << YAML::Value << scene.Id;
            output << YAML::Key << "Path" << YAML::Value << NormalizePortableBuildPath(scene.LogicalPath);
            output << YAML::EndMap;
        }
        output << YAML::EndSeq;
        output << YAML::Key << "Paths" << YAML::Value << YAML::BeginMap;
        output << YAML::Key << "ContentPack" << YAML::Value << NormalizePortableBuildPath(manifest.Paths.ContentPack);
        output << YAML::Key << "ManagedAssembly" << YAML::Value << NormalizePortableBuildPath(manifest.Paths.ManagedAssembly);
        output << YAML::Key << "MonoRoot" << YAML::Value << NormalizePortableBuildPath(manifest.Paths.MonoRoot);
        output << YAML::EndMap << YAML::EndMap;
        return WriteYaml(path, output);
    }

    String BuildManifestStore::Load(const Path& path, BuildManifest& manifest)
    {
        YAML::Node node;
        if (String error = ReadYaml(path, node); !error.empty())
            return error;

        try
        {
            BuildManifest loaded;
            ReadOptional(node, "Schema", loaded.Schema);
            ReadOptional(node, "PlayerAbi", loaded.PlayerAbi);
            ReadOptional(node, "ContentSchema", loaded.ContentSchema);
            ReadOptional(node, "ProductName", loaded.ProductName);
            ReadOptional(node, "ArtifactName", loaded.ArtifactName);
            ReadOptional(node, "ProductVersion", loaded.ProductVersion);
            ReadOptional(node, "Company", loaded.Company);
            ReadOptional(node, "EngineVersion", loaded.EngineVersion);
            ReadOptional(node, "MonoVersion", loaded.MonoVersion);

            if (!TryParseBuildPlatform(node["Platform"].as<String>(""), loaded.Platform))
                return "Build manifest has an unknown platform.";
            if (!TryParseBuildConfiguration(node["Configuration"].as<String>(""), loaded.Configuration))
                return "Build manifest has an unknown build configuration.";
            if (!TryParseRendererPolicy(node["Renderers"].as<String>(""), loaded.Renderers))
                return "Build manifest has an unknown renderer policy.";

            const YAML::Node quality = node["Quality"];
            if (!quality || !quality.IsMap())
                return "Build manifest has no quality policy.";
            if (!TryParseQualityTier(quality["Default"].as<String>(""), loaded.DefaultQuality))
                return "Build manifest has an unknown default quality tier.";
            loaded.AllowedQuality.clear();
            if (const YAML::Node allowed = quality["Allowed"])
            {
                for (const YAML::Node tierNode : allowed)
                {
                    QualityTier tier;
                    if (!TryParseQualityTier(tierNode.as<String>(), tier))
                        return "Build manifest has an unknown allowed quality tier.";
                    loaded.AllowedQuality.push_back(tier);
                }
            }

            ReadOptional(node, "StartupScene", loaded.StartupScene);
            if (const YAML::Node scenes = node["Scenes"])
            {
                for (const YAML::Node sceneNode : scenes)
                {
                    BuildManifestScene scene;
                    ReadOptional(sceneNode, "Order", scene.Order);
                    ReadOptional(sceneNode, "Id", scene.Id);
                    scene.LogicalPath = sceneNode["Path"].as<String>("");
                    loaded.Scenes.push_back(std::move(scene));
                }
            }

            if (const YAML::Node paths = node["Paths"])
            {
                loaded.Paths.ContentPack = paths["ContentPack"].as<String>("");
                loaded.Paths.ManagedAssembly = paths["ManagedAssembly"].as<String>("");
                loaded.Paths.MonoRoot = paths["MonoRoot"].as<String>("");
            }

            std::sort(loaded.Scenes.begin(), loaded.Scenes.end(),
                      [](const BuildManifestScene& left, const BuildManifestScene& right) { return left.Order < right.Order; });

            const BuildValidation validation = ValidateBuildManifest(loaded);
            if (!validation.IsValid())
                return ValidationError(validation);
            manifest = std::move(loaded);
            return {};
        }
        catch (const std::exception& error)
        {
            return "Invalid build manifest in '" + path.string() + "': " + error.what();
        }
    }

    BuildValidation ValidateBuildManifest(const BuildManifest& manifest)
    {
        BuildValidation validation;
        if (manifest.Schema != BUILD_MANIFEST_SCHEMA)
            validation.Error("manifest.schema.unsupported", "The build manifest schema is not supported.");
        if (manifest.PlayerAbi != PLAYER_ABI_VERSION)
            validation.Error("manifest.player_abi.incompatible", "The player ABI does not match this runtime.");
        if (manifest.ContentSchema != CONTENT_SCHEMA_VERSION)
            validation.Error("manifest.content_schema.incompatible", "The content schema does not match this runtime.");
        if (manifest.EngineVersion != CROWNY_VERSION_STRING)
            validation.Error("manifest.engine_version.incompatible", "The manifest was built for a different Crowny version.");

        if (manifest.ProductName.empty())
            validation.Error("manifest.product_name.empty", "The product name is empty.");
        if (manifest.ArtifactName.empty() || SanitizeArtifactName(manifest.ArtifactName) != manifest.ArtifactName)
            validation.Error("manifest.artifact_name.invalid", "The artifact name is empty or requires sanitization.");
        if (manifest.ProductVersion.empty())
            validation.Error("manifest.product_version.empty", "The product version is empty.");
        if (!IsKnown(manifest.Platform))
            validation.Error("manifest.platform.invalid", "The build platform is invalid.");
        if (!IsKnown(manifest.Configuration))
            validation.Error("manifest.configuration.invalid", "The build configuration is invalid.");
        if (!IsKnown(manifest.Renderers))
            validation.Error("manifest.renderers.invalid", "The renderer policy is invalid.");

        if (manifest.Paths.ContentPack.empty() || !IsSafeRelativeBuildPath(manifest.Paths.ContentPack))
            validation.Error("manifest.path.unsafe", "The content pack path must be a safe relative path.",
                             manifest.Paths.ContentPack.generic_string());
        if (!manifest.Paths.ManagedAssembly.empty() && !IsSafeRelativeBuildPath(manifest.Paths.ManagedAssembly))
            validation.Error("manifest.path.unsafe", "The managed assembly path must be a safe relative path.",
                             manifest.Paths.ManagedAssembly.generic_string());
        if (!manifest.Paths.MonoRoot.empty() && !IsSafeRelativeBuildPath(manifest.Paths.MonoRoot))
            validation.Error("manifest.path.unsafe", "The Mono root must be a safe relative path.", manifest.Paths.MonoRoot.generic_string());

        if (manifest.Scenes.empty())
            validation.Error("manifest.scenes.empty", "The build manifest contains no scenes.");
        Set<UUID> sceneIds;
        Set<uint32_t> sceneOrders;
        Set<String> scenePaths;
        Set<String> lowercaseScenePaths;
        bool startupFound = false;
        for (const BuildManifestScene& scene : manifest.Scenes)
        {
            const String logicalPath = NormalizePortableBuildPath(scene.LogicalPath);
            if (scene.Id.Empty())
                validation.Error("manifest.scene.id_empty", "A scene has an empty asset ID.", logicalPath);
            else if (!sceneIds.insert(scene.Id).second)
                validation.Error("manifest.scene.id_duplicate", "A scene asset ID appears more than once.", scene.Id.ToString());
            if (!sceneOrders.insert(scene.Order).second)
                validation.Error("manifest.scene.order_duplicate", "A scene order appears more than once.", std::to_string(scene.Order));
            if (!IsSafeRelativeBuildPath(scene.LogicalPath))
                validation.Error("manifest.scene.path_unsafe", "A scene path must be a safe relative path.", logicalPath);
            else if (!scenePaths.insert(logicalPath).second || !lowercaseScenePaths.insert(Lowercase(logicalPath)).second)
                validation.Error("manifest.scene.path_duplicate", "Scene paths must be unique without case-only collisions.", logicalPath);
            startupFound = startupFound || scene.Id == manifest.StartupScene;
        }
        for (uint32_t order = 0; order < manifest.Scenes.size(); order++)
        {
            if (!sceneOrders.contains(order))
                validation.Error("manifest.scene.order_gap", "Scene order values must form a contiguous zero-based sequence.");
        }
        if (manifest.StartupScene.Empty() || !startupFound)
            validation.Error("manifest.startup_scene.missing", "The startup scene is not present in the scene list.");

        if (manifest.AllowedQuality.empty())
            validation.Error("manifest.quality.empty", "At least one quality tier must be allowed.");
        Set<QualityTier> allowedQuality;
        for (QualityTier tier : manifest.AllowedQuality)
        {
            if (!IsKnown(tier))
                validation.Error("manifest.quality.invalid", "An allowed quality tier is invalid.");
            else if (!allowedQuality.insert(tier).second)
                validation.Error("manifest.quality.duplicate", "An allowed quality tier appears more than once.");
        }
        if (!IsKnown(manifest.DefaultQuality) || !allowedQuality.contains(manifest.DefaultQuality))
            validation.Error("manifest.quality.default_not_allowed", "The default quality tier is not allowed.");
        return validation;
    }
} // namespace Crowny
