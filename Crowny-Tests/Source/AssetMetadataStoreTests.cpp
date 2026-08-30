#include <catch2/catch_test_macros.hpp>

#include "Editor/AssetLibraryServices.h"

#include "Crowny/Common/FileSystem.h"

#include <chrono>
#include <fstream>

using namespace Crowny;

namespace
{
    class TemporaryMetadataDirectory
    {
    public:
        TemporaryMetadataDirectory()
        {
            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            Root = fs::temp_directory_path() / ("crowny-asset-metadata-" + std::to_string(unique));
            fs::create_directories(Root);
        }

        ~TemporaryMetadataDirectory()
        {
            std::error_code error;
            fs::remove_all(Root, error);
        }

        Path Root;
    };

    class NamedAsset final : public Asset
    {
    public:
        NamedAsset(AssetType type, String name) : m_Type(type) { SetName(name); }

        AssetType GetAssetType() const override { return m_Type; }

    private:
        AssetType m_Type;
    };

    Ref<AssetMetadata> MakeMetadata(const UUID& uuid, AssetType type, String key = {})
    {
        Ref<AssetMetadata> metadata = CreateRef<AssetMetadata>();
        metadata->Uuid = uuid;
        metadata->Type = type;
        metadata->SubassetKey = std::move(key);
        metadata->ImportOptions = CreateRef<ImportOptions>();
        return metadata;
    }

    Path BackupPath(const Path& path) { return Path(path.string() + ".bak"); }

    void WriteText(const Path& path, StringView text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        REQUIRE(output.good());
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        REQUIRE(output.good());
    }
} // namespace

TEST_CASE("Asset metadata keeps a last-good identity document", "[Assets][Metadata]")
{
    TemporaryMetadataDirectory temporary;
    const Path path = temporary.Root / "character.fbx.meta";
    AssetMetadataStore store;

    const UUID primaryId(1, 2, 3, 4);
    const UUID walkId(5, 6, 7, 8);
    Ref<AssetMetadata> primary = MakeMetadata(primaryId, AssetType::Mesh);
    Ref<AssetMetadata> walk = MakeMetadata(walkId, AssetType::AnimationClip, "18:Walk#0");

    String error;
    REQUIRE(store.Save(path, primary, { walk }, &error));
    CHECK(error.empty());
    CHECK_FALSE(fs::exists(BackupPath(path)));

    primary->IncludeInBuild = true;
    REQUIRE(store.Save(path, primary, { walk }, &error));
    REQUIRE(fs::is_regular_file(BackupPath(path)));

    AssetMetadataLoadResult current = store.Load(path);
    REQUIRE(current.Status == AssetMetadataLoadStatus::Loaded);
    REQUIRE(current.Metadata != nullptr);
    CHECK(current.Version == 2);
    CHECK(current.Metadata->Uuid == primaryId);
    CHECK(current.Metadata->IncludeInBuild);
    REQUIRE(current.Dependents.size() == 1);
    CHECK(current.Dependents.front()->Uuid == walkId);
    CHECK(current.Dependents.front()->SubassetKey == "18:Walk#0");

    WriteText(path, "Version: 2\nUuid:");
    AssetMetadataLoadResult recovered = store.Load(path);
    REQUIRE(recovered.Status == AssetMetadataLoadStatus::Recovered);
    REQUIRE(recovered.Metadata != nullptr);
    CHECK(recovered.Metadata->Uuid == primaryId);
    CHECK_FALSE(recovered.Metadata->IncludeInBuild);
    REQUIRE(recovered.Dependents.size() == 1);
    CHECK(recovered.Dependents.front()->Uuid == walkId);
}

TEST_CASE("Asset metadata reports corruption without manufacturing UUIDs", "[Assets][Metadata]")
{
    TemporaryMetadataDirectory temporary;
    const Path path = temporary.Root / "broken.png.meta";
    AssetMetadataStore store;

    SECTION("required identity fields are missing")
    {
        WriteText(path, "Version: 2\nIncludeInBuild: false\nTypeId: 2\n");
        const AssetMetadataLoadResult result = store.Load(path);
        CHECK(result.Status == AssetMetadataLoadStatus::Corrupt);
        CHECK(result.Metadata == nullptr);
        CHECK(result.Dependents.empty());
    }

    SECTION("both copies are corrupt")
    {
        WriteText(path, "Version: 2\nUuid:");
        WriteText(BackupPath(path), "Version: 999\n");
        const AssetMetadataLoadResult result = store.Load(path);
        CHECK(result.Status == AssetMetadataLoadStatus::Corrupt);
        CHECK(result.Metadata == nullptr);
        CHECK_FALSE(result.Error.empty());
    }

    SECTION("dependent keys must be unique")
    {
        const UUID primaryId(11, 12, 13, 14);
        const UUID firstId(21, 22, 23, 24);
        const UUID secondId(31, 32, 33, 34);
        const String duplicateKeys = "Version: 2\nUuid: " + primaryId.ToString() + "\nIncludeInBuild: false\nTypeId: 5\nDependents:\n" +
                                     "  - Key: 18:Walk#0\n    Uuid: " + firstId.ToString() + "\n    TypeId: 18\n" +
                                     "  - Key: 18:Walk#0\n    Uuid: " + secondId.ToString() + "\n    TypeId: 18\n";
        WriteText(path, duplicateKeys);
        const AssetMetadataLoadResult result = store.Load(path);
        CHECK(result.Status == AssetMetadataLoadStatus::Corrupt);
        CHECK(result.Metadata == nullptr);
    }
}

TEST_CASE("Asset metadata save leaves the primary untouched when backup publication fails", "[Assets][Metadata][AtomicWrite]")
{
    TemporaryMetadataDirectory temporary;
    const Path path = temporary.Root / "material.mat.meta";
    AssetMetadataStore store;
    Ref<AssetMetadata> metadata = MakeMetadata(UUID(10, 20, 30, 40), AssetType::Material);

    String error;
    REQUIRE(store.Save(path, metadata, {}, &error));
    const String original = FileSystem::ReadTextFile(path);

    REQUIRE(fs::create_directory(BackupPath(path)));
    metadata->IncludeInBuild = true;
    CHECK_FALSE(store.Save(path, metadata, {}, &error));
    CHECK_FALSE(error.empty());
    CHECK(FileSystem::ReadTextFile(path) == original);
}

TEST_CASE("Atomic text publication accepts an empty document", "[Assets][Metadata][AtomicWrite]")
{
    TemporaryMetadataDirectory temporary;
    const Path path = temporary.Root / "empty.txt";
    String error;
    REQUIRE(FileSystem::WriteTextFileAtomic(path, {}, &error));
    CHECK(error.empty());
    CHECK(fs::is_regular_file(path));
    CHECK(fs::file_size(path) == 0);
}

TEST_CASE("Atomic asset copy preserves its destination on failure", "[Assets][Metadata][AtomicWrite]")
{
    TemporaryMetadataDirectory temporary;
    const Path source = temporary.Root / "scene.cw";
    const Path destination = temporary.Root / "scene.asset";
    WriteText(source, "new scene");
    WriteText(destination, "old scene");

    AssetFilesystemOperations files;
    String error;
    CHECK_FALSE(files.CopyFileAtomic(temporary.Root / "missing.cw", destination, true, &error));
    CHECK_FALSE(error.empty());
    CHECK(FileSystem::ReadTextFile(destination) == "old scene");

    REQUIRE(files.CopyFileAtomic(source, destination, true, &error));
    CHECK(error.empty());
    CHECK(FileSystem::ReadTextFile(destination) == "new scene");
}

TEST_CASE("Metadata backups stay outside the asset index", "[Assets][Metadata][Scanner]")
{
    TemporaryMetadataDirectory temporary;
    const Path source = temporary.Root / "character.fbx";
    const Path backup = temporary.Root / "character.fbx.meta.bak";
    WriteText(source, "mesh");
    WriteText(backup, "last-good metadata");

    AssetIndex index;
    index.SetRoot(CreateRef<DirectoryEntry>(temporary.Root, temporary.Root.filename().string(), nullptr));
    const AssetFileSystemDiff diff = AssetFileSystemScanner().Scan(temporary.Root, temporary.Root, index);

    REQUIRE(diff.AddedFiles.size() == 1);
    CHECK(diff.AddedFiles.front() == source);
    CHECK(diff.DanglingMetadata.empty());
    CHECK(AssetFileSystemScanner::IsMetadata(backup));
}

TEST_CASE("An orphaned metadata backup is reported for cleanup", "[Assets][Metadata][Scanner]")
{
    TemporaryMetadataDirectory temporary;
    const Path backup = temporary.Root / "removed.fbx.meta.bak";
    WriteText(backup, "last-good metadata");

    AssetIndex index;
    index.SetRoot(CreateRef<DirectoryEntry>(temporary.Root, temporary.Root.filename().string(), nullptr));
    const AssetFileSystemDiff diff = AssetFileSystemScanner().Scan(temporary.Root, temporary.Root, index);

    REQUIRE(diff.DanglingMetadata.size() == 1);
    CHECK(diff.DanglingMetadata.front() == temporary.Root / "removed.fbx.meta");
    CHECK(diff.AddedFiles.empty());
}

TEST_CASE("Dependent asset reconciliation preserves keyed UUIDs across reordering", "[Assets][Metadata][Subassets]")
{
    AssetMetadataStore store;
    const Vector<Ref<Asset>> initialAssets{
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Walk"),
        CreateRef<NamedAsset>(AssetType::Material, "Body"),
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Run"),
    };

    AssetDependentReconciliation initial = store.ReconcileDependents({}, initialAssets);
    REQUIRE(initial.Assignments.size() == 3);
    CHECK(initial.Orphans.empty());
    const UUID walkId = initial.Assignments[0].Metadata->Uuid;
    const UUID bodyId = initial.Assignments[1].Metadata->Uuid;
    const UUID runId = initial.Assignments[2].Metadata->Uuid;

    const Vector<Ref<AssetMetadata>> existing{
        initial.Assignments[0].Metadata,
        initial.Assignments[1].Metadata,
        initial.Assignments[2].Metadata,
    };
    const Vector<Ref<Asset>> reorderedAssets{
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Run"),
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Idle"),
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Walk"),
    };

    AssetDependentReconciliation reordered = store.ReconcileDependents(existing, reorderedAssets);
    REQUIRE(reordered.Assignments.size() == 3);
    CHECK(reordered.Assignments[0].Metadata->Uuid == runId);
    CHECK(reordered.Assignments[2].Metadata->Uuid == walkId);
    CHECK(reordered.Assignments[1].Metadata->Uuid != UUID::EMPTY);
    CHECK(reordered.Assignments[1].Metadata->Uuid != walkId);
    CHECK(reordered.Assignments[1].Metadata->Uuid != runId);
    REQUIRE(reordered.Orphans.size() == 1);
    CHECK(reordered.Orphans.front()->Uuid == bodyId);
}

TEST_CASE("Legacy dependent metadata migrates by type ordinal once", "[Assets][Metadata][Subassets]")
{
    TemporaryMetadataDirectory temporary;
    const Path path = temporary.Root / "legacy.fbx.meta";
    AssetMetadataStore store;
    const UUID firstClipId(101, 102, 103, 104);
    const UUID materialId(201, 202, 203, 204);
    const UUID secondClipId(301, 302, 303, 304);
    const UUID primaryId(401, 402, 403, 404);
    const String legacyText = "Version: 1\nUuid: " + primaryId.ToString() + "\nIncludeInBuild: false\nTypeId: 5\nDependents:\n" +
                              "  - Uuid: " + firstClipId.ToString() + "\n    TypeId: 18\n" + "  - Uuid: " + materialId.ToString() +
                              "\n    TypeId: 4\n" + "  - Uuid: " + secondClipId.ToString() + "\n    TypeId: 18\n";
    WriteText(path, legacyText);
    const AssetMetadataLoadResult loaded = store.Load(path);
    REQUIRE(loaded.Status == AssetMetadataLoadStatus::Loaded);
    REQUIRE(loaded.Version == 1);
    REQUIRE(loaded.Dependents.size() == 3);

    const Vector<Ref<Asset>> imported{
        CreateRef<NamedAsset>(AssetType::Material, "Body"),
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Walk"),
        CreateRef<NamedAsset>(AssetType::AnimationClip, "Run"),
    };

    AssetDependentReconciliation migrated = store.ReconcileDependents(loaded.Dependents, imported);
    REQUIRE(migrated.Assignments.size() == 3);
    CHECK(migrated.Assignments[0].Metadata->Uuid == materialId);
    CHECK(migrated.Assignments[1].Metadata->Uuid == firstClipId);
    CHECK(migrated.Assignments[2].Metadata->Uuid == secondClipId);
    CHECK_FALSE(migrated.Assignments[0].Metadata->SubassetKey.empty());
    CHECK_FALSE(migrated.Assignments[1].Metadata->SubassetKey.empty());
    CHECK_FALSE(migrated.Assignments[2].Metadata->SubassetKey.empty());
    CHECK(migrated.Orphans.empty());
}
