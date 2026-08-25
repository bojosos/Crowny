#include "cwpch.h"

#include "Crowny/Build/ContentGraph.h"
#include "Crowny/Common/Yaml.h"

#include <cctype>

namespace Crowny
{
    namespace
    {
        String NormalizeLogicalPath(const Path& input) { return NormalizePortableBuildPath(input); }

        String Lowercase(String value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        bool IsUnderFolder(StringView logicalPath, StringView folder)
        {
            if (logicalPath == folder)
                return true;
            return logicalPath.size() > folder.size() && logicalPath.starts_with(folder) && logicalPath[folder.size()] == '/';
        }

        String WriteDatabaseYaml(const Path& path, const YAML::Emitter& output)
        {
            if (!output.good())
                return output.GetLastError();
            std::error_code error;
            if (!path.parent_path().empty())
                fs::create_directories(path.parent_path(), error);
            if (error)
                return "Cannot create content database directory: " + error.message();
            const Path temporary = path.string() + ".tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                if (!stream)
                    return "Cannot write content database '" + temporary.string() + "'.";
                stream.write(output.c_str(), static_cast<std::streamsize>(output.size()));
                stream.flush();
                if (!stream)
                {
                    stream.close();
                    fs::remove(temporary, error);
                    return "Writing content database failed for '" + temporary.string() + "'.";
                }
            }

            const Path backup = path.string() + ".previous";
            fs::remove(backup, error);
            error.clear();
            const bool hadDestination = fs::exists(path, error);
            if (error)
                return "Cannot inspect content database destination: " + error.message();
            if (hadDestination)
            {
                fs::rename(path, backup, error);
                if (error)
                    return "Cannot preserve existing content database: " + error.message();
            }
            fs::rename(temporary, path, error);
            if (error)
            {
                const String message = "Cannot publish content database '" + path.string() + "': " + error.message();
                if (hadDestination)
                {
                    error.clear();
                    fs::rename(backup, path, error);
                }
                return message;
            }
            if (hadDestination)
                fs::remove(backup, error);
            return {};
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
    } // namespace

    String ContentDatabaseStore::Save(const Path& path, const ContentDatabase& database)
    {
        const BuildValidation validation = ValidateContentDatabase(database);
        if (!validation.IsValid())
            return ValidationError(validation);

        Vector<ContentAssetRecord> assets = database.Assets;
        std::sort(assets.begin(), assets.end(), [](const ContentAssetRecord& left, const ContentAssetRecord& right) {
            return NormalizeLogicalPath(left.LogicalPath) < NormalizeLogicalPath(right.LogicalPath);
        });

        YAML::Emitter output;
        output << YAML::BeginMap;
        output << YAML::Key << "Schema" << YAML::Value << database.Schema;
        output << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;
        for (const ContentAssetRecord& asset : assets)
        {
            output << YAML::BeginMap;
            output << YAML::Key << "Id" << YAML::Value << asset.Id;
            output << YAML::Key << "Path" << YAML::Value << NormalizeLogicalPath(asset.LogicalPath);
            output << YAML::Key << "CookedPath" << YAML::Value << asset.CookedPath.generic_string();
            output << YAML::Key << "Type" << YAML::Value << asset.Type;
            output << YAML::Key << "SourceHash" << YAML::Value << asset.SourceHash;
            output << YAML::Key << "Dependencies" << YAML::Value << YAML::BeginSeq;
            Vector<UUID> dependencies = asset.Dependencies;
            std::sort(dependencies.begin(), dependencies.end());
            for (const UUID& dependency : dependencies)
                output << dependency;
            output << YAML::EndSeq << YAML::EndMap;
        }
        output << YAML::EndSeq << YAML::EndMap;
        return WriteDatabaseYaml(path, output);
    }

    String ContentDatabaseStore::Load(const Path& path, ContentDatabase& database)
    {
        try
        {
            const YAML::Node root = YAML::LoadFile(path.string());
            if (!root.IsMap())
                return "Content database root must be a map.";
            ContentDatabase loaded;
            loaded.Schema = root["Schema"].as<uint32_t>(0);
            if (loaded.Schema > CONTENT_DATABASE_SCHEMA)
                return "Content database schema is newer than this builder supports.";
            const YAML::Node assets = root["Assets"];
            if (assets && !assets.IsSequence())
                return "Content database Assets value must be a sequence.";
            if (assets)
            {
                for (const YAML::Node node : assets)
                {
                    ContentAssetRecord asset;
                    asset.Id = node["Id"].as<UUID>();
                    asset.LogicalPath = node["Path"].as<String>();
                    asset.CookedPath = node["CookedPath"].as<String>();
                    asset.Type = node["Type"].as<String>("");
                    asset.SourceHash = node["SourceHash"].as<String>("");
                    if (const YAML::Node dependencies = node["Dependencies"])
                    {
                        for (const YAML::Node dependency : dependencies)
                            asset.Dependencies.push_back(dependency.as<UUID>());
                    }
                    loaded.Assets.push_back(std::move(asset));
                }
            }
            const BuildValidation validation = ValidateContentDatabase(loaded);
            if (!validation.IsValid())
                return ValidationError(validation);
            database = std::move(loaded);
            return {};
        }
        catch (const std::exception& error)
        {
            return "Cannot read content database '" + path.string() + "': " + error.what();
        }
    }

    ContentResolveRequest CreateContentResolveRequest(const BuildProfile& profile)
    {
        ContentResolveRequest request;
        request.SceneRoots = profile.SceneOrder;
        request.ExcludedAssets = profile.ExcludedAssets;
        for (const ContentRoot& root : profile.ContentRoots)
        {
            if (root.Kind == ContentRootKind::Asset)
                request.AssetRoots.push_back(root.AssetId);
            else
                request.FolderRoots.push_back(root.PathValue);
        }
        return request;
    }

    BuildValidation ValidateContentDatabase(const ContentDatabase& database)
    {
        BuildValidation validation;
        if (database.Schema != CONTENT_DATABASE_SCHEMA)
            validation.Error("content.schema.unsupported", "The content database schema is not supported.");

        UnorderedMap<UUID, const ContentAssetRecord*> byId;
        Set<String> paths;
        Set<String> lowercasePaths;
        for (const ContentAssetRecord& asset : database.Assets)
        {
            const String logicalPath = NormalizeLogicalPath(asset.LogicalPath);
            if (asset.Id.Empty())
                validation.Error("content.asset.id.empty", "Content database entry has an empty asset ID.", logicalPath);
            else if (!byId.emplace(asset.Id, &asset).second)
                validation.Error("content.asset.id.duplicate", "Content database contains a duplicate asset ID.", asset.Id.ToString());
            if (!IsSafeRelativeBuildPath(asset.LogicalPath))
                validation.Error("content.asset.path.unsafe", "Runtime asset paths must be project-relative and cannot contain '..'.", logicalPath);
            else if (!paths.insert(logicalPath).second)
                validation.Error("content.asset.path.duplicate", "Content database contains a duplicate runtime path.", logicalPath);
            else if (!lowercasePaths.insert(Lowercase(logicalPath)).second)
                validation.Error("content.asset.path.case_collision", "Runtime asset paths differ only by letter case.", logicalPath);
            if (!IsSafeRelativeBuildPath(asset.CookedPath))
                validation.Error("content.asset.cooked_path.unsafe", "Cooked asset paths must stay inside the project.",
                                 asset.CookedPath.generic_string());

            Set<UUID> dependencies;
            for (const UUID& dependency : asset.Dependencies)
            {
                if (dependency.Empty())
                    validation.Error("content.dependency.empty", "An asset dependency has an empty ID.", logicalPath);
                else if (!dependencies.insert(dependency).second)
                    validation.Error("content.dependency.duplicate", "An asset dependency appears more than once.", dependency.ToString());
            }
        }

        for (const ContentAssetRecord& asset : database.Assets)
        {
            for (const UUID& dependency : asset.Dependencies)
            {
                if (!dependency.Empty() && !byId.contains(dependency))
                    validation.Error("content.dependency.missing", "A dependency is missing from the content database.", dependency.ToString());
            }
        }

        enum class VisitState
        {
            Visiting,
            Visited
        };
        UnorderedMap<UUID, VisitState> states;
        std::function<void(const UUID&)> visit = [&](const UUID& id) {
            const auto state = states.find(id);
            if (state != states.end())
            {
                if (state->second == VisitState::Visiting)
                    validation.Error("content.dependency.cycle", "The content database contains a dependency cycle.", id.ToString());
                return;
            }
            const auto asset = byId.find(id);
            if (asset == byId.end())
                return;
            states[id] = VisitState::Visiting;
            for (const UUID& dependency : asset->second->Dependencies)
                visit(dependency);
            states[id] = VisitState::Visited;
        };
        for (const auto& entry : byId)
            visit(entry.first);
        return validation;
    }

    ContentResolveResult ResolveContent(const ContentDatabase& database, const ContentResolveRequest& request)
    {
        ContentResolveResult result;
        result.Validation = ValidateContentDatabase(database);
        if (!result.Validation.IsValid())
            return result;

        UnorderedMap<UUID, const ContentAssetRecord*> byId;
        for (const ContentAssetRecord& asset : database.Assets)
            byId.emplace(asset.Id, &asset);

        struct PendingRoot
        {
            UUID Id;
            String Kind;
        };
        Vector<PendingRoot> roots;
        for (const UUID& scene : request.SceneRoots)
            roots.push_back({ scene, "Scene" });
        for (const UUID& asset : request.AssetRoots)
            roots.push_back({ asset, "Asset" });
        for (const Path& folderPath : request.FolderRoots)
        {
            const String folder = NormalizeLogicalPath(folderPath);
            if (!IsSafeRelativeBuildPath(folderPath))
            {
                result.Validation.Error("content.folder_root.path_unsafe", "Folder roots must be project-relative and cannot contain '..'.", folder);
                continue;
            }
            bool matched = false;
            for (const ContentAssetRecord& asset : database.Assets)
            {
                if (IsUnderFolder(NormalizeLogicalPath(asset.LogicalPath), folder))
                {
                    roots.push_back({ asset.Id, "Folder" });
                    matched = true;
                }
            }
            if (!matched)
                result.Validation.Warn("content.folder_root.empty", "Folder root contains no imported assets.", folder);
        }

        std::sort(roots.begin(), roots.end(), [&](const PendingRoot& left, const PendingRoot& right) {
            const auto leftIter = byId.find(left.Id);
            const auto rightIter = byId.find(right.Id);
            const String leftPath = leftIter == byId.end() ? left.Id.ToString() : NormalizeLogicalPath(leftIter->second->LogicalPath);
            const String rightPath = rightIter == byId.end() ? right.Id.ToString() : NormalizeLogicalPath(rightIter->second->LogicalPath);
            return leftPath < rightPath;
        });

        const UnorderedSet<UUID> exclusions(request.ExcludedAssets.begin(), request.ExcludedAssets.end());
        UnorderedMap<UUID, ResolvedContentAsset> included;
        enum class VisitState
        {
            Visiting,
            Visited
        };
        UnorderedMap<UUID, VisitState> states;

        std::function<void(const UUID&, const Vector<UUID>&, const String&)> visit;
        visit = [&](const UUID& id, const Vector<UUID>& parentChain, const String& rootKind) {
            const auto assetIter = byId.find(id);
            if (assetIter == byId.end())
            {
                result.Validation.Error(parentChain.empty() ? "content.root.missing" : "content.dependency.missing",
                                        parentChain.empty() ? "A content root is missing from the content database."
                                                            : "A required asset dependency is missing from the content database.",
                                        id.ToString());
                return;
            }

            Vector<UUID> chain = parentChain;
            chain.push_back(id);
            if (exclusions.contains(id))
            {
                String message = "Required asset '" + NormalizeLogicalPath(assetIter->second->LogicalPath) + "' is excluded. Dependency chain: ";
                for (size_t index = 0; index < chain.size(); index++)
                {
                    if (index > 0)
                        message += " -> ";
                    const auto chainIter = byId.find(chain[index]);
                    message += chainIter == byId.end() ? chain[index].ToString() : NormalizeLogicalPath(chainIter->second->LogicalPath);
                }
                result.Validation.Error("content.required_asset_excluded", std::move(message), id.ToString());
                return;
            }

            const auto stateIter = states.find(id);
            if (stateIter != states.end() && stateIter->second == VisitState::Visiting)
            {
                result.Validation.Error("content.dependency.cycle",
                                        "Content dependency cycle reaches '" + NormalizeLogicalPath(assetIter->second->LogicalPath) + "'.",
                                        id.ToString());
                return;
            }
            if (stateIter != states.end() && stateIter->second == VisitState::Visited)
                return;

            states[id] = VisitState::Visiting;
            included.emplace(id, ResolvedContentAsset{ *assetIter->second, chain, rootKind });
            Vector<UUID> dependencies = assetIter->second->Dependencies;
            std::sort(dependencies.begin(), dependencies.end());
            for (const UUID& dependency : dependencies)
                visit(dependency, chain, rootKind);
            states[id] = VisitState::Visited;
        };

        for (const PendingRoot& root : roots)
        {
            states.clear();
            visit(root.Id, {}, root.Kind);
        }

        result.Assets.reserve(included.size());
        for (auto& [id, asset] : included)
            result.Assets.push_back(std::move(asset));
        std::sort(result.Assets.begin(), result.Assets.end(), [](const ResolvedContentAsset& left, const ResolvedContentAsset& right) {
            return NormalizeLogicalPath(left.Asset.LogicalPath) < NormalizeLogicalPath(right.Asset.LogicalPath);
        });
        return result;
    }
} // namespace Crowny
