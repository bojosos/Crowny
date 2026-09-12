#include <catch2/catch_test_macros.hpp>

#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Mesh.h"
#include "Editor/UndoRedo.h"
#include "Editor/ViewportAssetDrop.h"

using namespace Crowny;

namespace
{
    struct DropLibrary
    {
        Ref<FileEntry> File;
        AssetHandle<Asset> Asset;
        bool Importing = true;
        bool ImportSucceeds = true;
        int Imports = 0;
        int Loads = 0;

        ViewportAssetDrop Create()
        {
            return ViewportAssetDrop({
              [this](const Path& path) { return File && path == File->Filepath ? File : nullptr; },
              [this](const Path&) {
                  ++Imports;
                  return ImportSucceeds ? Path("Assets/model.obj") : Path{};
              },
              [this](const FileEntry&) {
                  ++Loads;
                  return Asset;
              },
              [this]() { return Importing; },
            });
        }

        void Complete(AssetType type)
        {
            File = CreateRef<FileEntry>();
            File->Filepath = "Assets/model.obj";
            File->Metadata = CreateRef<AssetMetadata>();
            File->Metadata->Type = type;
            File->Metadata->Uuid = UuidGenerator::Generate();
            Importing = false;
        }
    };

    class DropTestMesh : public Mesh
    {
    public:
        DropTestMesh() = default;
    };

    struct DropRuntime
    {
        bool OwnsListeners = !AssetListenerManager::IsStartedUp();
        bool OwnsAssets = !AssetManager::IsStartedUp();
        bool OwnsUndo = !UndoRedo::IsStartedUp();
        DropRuntime()
        {
            if (OwnsListeners)
                AssetListenerManager::StartUp();
            if (OwnsAssets)
                AssetManager::StartUp();
            if (OwnsUndo)
                UndoRedo::StartUp();
        }
        ~DropRuntime()
        {
            if (OwnsUndo)
                UndoRedo::Shutdown();
            if (OwnsAssets)
                AssetManager::Shutdown();
            if (OwnsListeners)
                AssetListenerManager::Shutdown();
        }
    };
} // namespace

TEST_CASE("Viewport drops share acceptance for source paths and library metadata", "[Editor][Viewport][Drop]")
{
    DropLibrary library;
    auto drops = library.Create();
    CHECK(drops.Describe("outside/model.OBJ") != nullptr);
    CHECK(drops.Describe("outside/clip.wav") != nullptr);
    CHECK(drops.Describe("outside/file.txt") == nullptr);
    CHECK(drops.Describe("outside/image.png") == nullptr);
    library.Complete(AssetType::Mesh);
    CHECK(String(drops.Describe(library.File->Filepath)) == String(drops.Describe("outside/model.obj")));
    library.File->Metadata->Type = AssetType::Texture;
    CHECK(drops.Describe(library.File->Filepath) == nullptr);
    CHECK_FALSE(drops.Submit("outside/file.txt", { CreateRef<Scene>(false), {}, {} }, 0));
    CHECK(library.Imports == 0);
}

TEST_CASE("Viewport mesh sources share placement selection and undo after import", "[Editor][Viewport][Drop]")
{
    DropRuntime runtime;
    DropLibrary library;
    library.Asset = AssetManager::Get().CreateAssetHandle(CreateRef<DropTestMesh>());
    auto drops = library.Create();
    Entity selected;
    drops.SetActions({ {}, [&](Entity entity) { selected = entity; } });
    Ref<Scene> scene = CreateRef<Scene>(false);
    const glm::vec3 position(3, 0, -7);
    ViewportDropContext context{ scene, {}, position };

    SECTION("Explorer source waits for import")
    {
        REQUIRE(drops.Submit("outside/model.obj", context, 0));
        context.WorldPosition = glm::vec3(100);
        drops.Update(scene, true, 120);
        CHECK(drops.GetPendingCount() == 1);
        CHECK_FALSE(selected);
        library.Complete(AssetType::Mesh);
        drops.Update(scene, true, 121);
    }
    SECTION("Asset browser source is already imported")
    {
        library.Complete(AssetType::Mesh);
        REQUIRE(drops.Submit(library.File->Filepath, context, 0));
        CHECK(library.Imports == 0);
    }
    SECTION("Asset browser source has no metadata yet")
    {
        library.File = CreateRef<FileEntry>();
        library.File->Filepath = "Assets/model.obj";
        REQUIRE(drops.Submit(library.File->Filepath, context, 0));
        CHECK(library.Imports == 1);
        library.Complete(AssetType::Mesh);
        drops.Update(scene, true, 1);
    }
    REQUIRE(selected);
    CHECK(glm::vec3(selected.GetWorldMatrix()[3]) == position);
    CHECK(selected.GetComponent<MeshRendererComponent>().MeshHandle.GetUUID() == library.Asset.GetUUID());
    CHECK(drops.GetPendingCount() == 0);
    CHECK(library.Loads == 1);
    const UUID id = selected.GetUuid();
    REQUIRE(UndoRedo::Get().Undo());
    CHECK_FALSE(scene->GetEntityFromUuid(id));
    REQUIRE(UndoRedo::Get().Redo());
    REQUIRE(scene->GetEntityFromUuid(id));
    CHECK(glm::vec3(scene->GetEntityFromUuid(id).GetWorldMatrix()[3]) == position);
}

TEST_CASE("Pending viewport drops cancel when their scene or edit state changes", "[Editor][Viewport][Drop]")
{
    DropLibrary library;
    auto drops = library.Create();
    Ref<Scene> scene = CreateRef<Scene>(false);
    REQUIRE(drops.Submit("outside/model.obj", { scene, {}, glm::vec3(0) }, 0));
    library.Complete(AssetType::Mesh);
    SECTION("Scene switch") { drops.Update(CreateRef<Scene>(false), true, 1); }
    SECTION("Play mode") { drops.Update(scene, false, 1); }
    SECTION("Project close") { drops.Update(nullptr, true, 1); }
    CHECK(drops.GetPendingCount() == 0);
    CHECK(library.Loads == 0);
}

TEST_CASE("Failed viewport imports expire without applying an action", "[Editor][Viewport][Drop]")
{
    DropLibrary library;
    auto drops = library.Create();
    Ref<Scene> scene = CreateRef<Scene>(false);
    SECTION("Copy failed")
    {
        library.ImportSucceeds = false;
        CHECK_FALSE(drops.Submit("outside/model.obj", { scene, {}, {} }, 0));
    }
    SECTION("Importer finished without metadata")
    {
        REQUIRE(drops.Submit("outside/model.obj", { scene, {}, {} }, 0));
        library.Importing = false;
        drops.Update(scene, true, 61);
    }
    CHECK(drops.GetPendingCount() == 0);
    CHECK(library.Loads == 0);
}

TEST_CASE("Opening a dropped scene discards pending requests for the previous scene", "[Editor][Viewport][Drop]")
{
    DropLibrary library;
    auto drops = library.Create();
    Ref<Scene> scene = CreateRef<Scene>(false);
    int opened = 0;
    drops.SetActions({ [&](const UUID&) { ++opened; }, {} });
    REQUIRE(drops.Submit("outside/first.obj", { scene, {}, {} }, 0));
    REQUIRE(drops.Submit("outside/second.obj", { scene, {}, {} }, 0));
    library.Complete(AssetType::Scene);
    drops.Update(scene, true, 1);
    CHECK(opened == 1);
    CHECK(drops.GetPendingCount() == 0);
    CHECK(library.Loads == 0);
}
