#pragma once

#include "Crowny/Build/BuildProfile.h"
#include "Crowny/Build/BuildTypes.h"

namespace Crowny
{
    constexpr uint32_t CONTENT_DATABASE_SCHEMA = 1;

    struct ContentAssetRecord
    {
        UUID Id;
        Path LogicalPath;
        Path CookedPath;
        Vector<UUID> Dependencies;
        String Type;
        String SourceHash;
    };

    struct ContentDatabase
    {
        uint32_t Schema = CONTENT_DATABASE_SCHEMA;
        Vector<ContentAssetRecord> Assets;
    };

    struct ContentResolveRequest
    {
        Vector<UUID> SceneRoots;
        Vector<UUID> AssetRoots;
        Vector<Path> FolderRoots;
        Vector<UUID> ExcludedAssets;
    };

    struct ResolvedContentAsset
    {
        ContentAssetRecord Asset;
        Vector<UUID> ReasonChain;
        String RootKind;
    };

    struct ContentResolveResult
    {
        BuildValidation Validation;
        Vector<ResolvedContentAsset> Assets;
    };

    class ContentDatabaseStore
    {
    public:
        static String Save(const Path& path, const ContentDatabase& database);
        static String Load(const Path& path, ContentDatabase& database);
    };

    ContentResolveRequest CreateContentResolveRequest(const BuildProfile& profile);
    BuildValidation ValidateContentDatabase(const ContentDatabase& database);
    ContentResolveResult ResolveContent(const ContentDatabase& database, const ContentResolveRequest& request);
} // namespace Crowny
