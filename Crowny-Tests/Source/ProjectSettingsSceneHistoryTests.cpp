#include <catch2/catch_test_macros.hpp>

#include "Editor/Settings/ProjectSettings.h"
#include "Serialization/ProjectSettingsSerializer.h"

using namespace Crowny;

namespace
{
    YAML::Node SerializeSettings(const Ref<ProjectSettings>& settings)
    {
        YAML::Emitter emitter;
        ProjectSettingsSerializer::Serialize(settings, emitter);
        return YAML::Load(emitter.c_str());
    }
} // namespace

TEST_CASE("Project scene history serializes stable asset identities", "[Editor][ProjectSettings][SceneHistory]")
{
    const UUID activeId(1, 2, 3, 4);
    const UUID recentId(5, 6, 7, 8);
    Ref<ProjectSettings> settings = CreateRef<ProjectSettings>();
    settings->LastOpenSceneId = activeId;
    settings->RecentSceneIds = { activeId, recentId };

    const YAML::Node serialized = SerializeSettings(settings);
    CHECK(serialized["LastOpenSceneId"].as<UUID>() == activeId);
    REQUIRE(serialized["RecentSceneIds"].size() == 2);
    CHECK_FALSE(serialized["LastOpenScene"]);
    CHECK_FALSE(serialized["RecentScenes"]);

    const Ref<ProjectSettings> restored = ProjectSettingsSerializer::Deserialize(serialized);
    CHECK(restored->LastOpenSceneId == activeId);
    REQUIRE(restored->RecentSceneIds.size() == 2);
    CHECK(restored->RecentSceneIds[0] == activeId);
    CHECK(restored->RecentSceneIds[1] == recentId);
}

TEST_CASE("Legacy project scene paths migrate without losing unresolved entries", "[Editor][ProjectSettings][SceneHistory]")
{
    const Path lastPath = "C:/OldProject/Assets/Scenes/Main.cwscene";
    const Path recentPath = "C:/OldProject/Assets/Scenes/Secondary.cwscene";
    const Path unresolvedPath = "C:/OldProject/Assets/Scenes/Missing.cwscene";
    const UUID lastId(10, 11, 12, 13);
    const UUID recentId(20, 21, 22, 23);

    YAML::Node legacy = SerializeSettings(CreateRef<ProjectSettings>());
    legacy.remove("LastOpenSceneId");
    legacy.remove("RecentSceneIds");
    legacy["LastOpenScene"] = lastPath.string();
    legacy["RecentScenes"].push_back(recentPath.string());
    legacy["RecentScenes"].push_back(recentPath.string());
    legacy["RecentScenes"].push_back(unresolvedPath.string());

    Ref<ProjectSettings> settings = ProjectSettingsSerializer::Deserialize(legacy);
    const bool changed = ProjectSettingsSerializer::MigrateLegacySceneReferences(*settings, [&](const Path& path, UUID& sceneId) {
        if (path == lastPath)
            sceneId = lastId;
        else if (path == recentPath)
            sceneId = recentId;
        else
            return false;
        return true;
    });

    CHECK(changed);
    CHECK(settings->LastOpenSceneId == lastId);
    CHECK(settings->LegacyLastOpenScenePath.empty());
    REQUIRE(settings->RecentSceneIds.size() == 1);
    CHECK(settings->RecentSceneIds[0] == recentId);
    REQUIRE(settings->LegacyRecentScenePaths.size() == 1);
    CHECK(settings->LegacyRecentScenePaths[0] == unresolvedPath);

    const YAML::Node migrated = SerializeSettings(settings);
    CHECK(migrated["LastOpenSceneId"].as<UUID>() == lastId);
    CHECK_FALSE(migrated["LastOpenScene"]);
    REQUIRE(migrated["RecentScenes"].size() == 1);
    CHECK(migrated["RecentScenes"][0].as<String>() == unresolvedPath.string());
}

TEST_CASE("UUID project scene settings take precedence over legacy paths", "[Editor][ProjectSettings][SceneHistory]")
{
    const UUID activeId(30, 31, 32, 33);
    Ref<ProjectSettings> settings = CreateRef<ProjectSettings>();
    settings->LastOpenSceneId = activeId;
    settings->LegacyLastOpenScenePath = "Assets/Old.cwscene";
    uint32_t resolverCalls = 0;

    CHECK(ProjectSettingsSerializer::MigrateLegacySceneReferences(*settings, [&](const Path&, UUID&) {
        ++resolverCalls;
        return false;
    }));
    CHECK(settings->LastOpenSceneId == activeId);
    CHECK(settings->LegacyLastOpenScenePath.empty());
    CHECK(resolverCalls == 0);
}

TEST_CASE("Recent project scenes stay unique newest-first and bounded", "[Editor][ProjectSettings][SceneHistory]")
{
    Ref<ProjectSettings> settings = CreateRef<ProjectSettings>();
    const UUID first(1, 0, 0, 0);
    const UUID second(2, 0, 0, 0);
    const UUID third(3, 0, 0, 0);
    const UUID fourth(4, 0, 0, 0);
    const UUID fifth(5, 0, 0, 0);
    const UUID sixth(6, 0, 0, 0);

    for (const UUID& sceneId : { first, second, third, fourth, fifth, sixth })
        ProjectSettingsSerializer::AddRecentScene(*settings, sceneId);
    ProjectSettingsSerializer::AddRecentScene(*settings, third);
    ProjectSettingsSerializer::AddRecentScene(*settings, UUID::EMPTY);

    REQUIRE(settings->RecentSceneIds.size() == 5);
    CHECK(settings->RecentSceneIds[0] == third);
    CHECK(settings->RecentSceneIds[1] == sixth);
    CHECK(settings->RecentSceneIds[2] == fifth);
    CHECK(settings->RecentSceneIds[3] == fourth);
    CHECK(settings->RecentSceneIds[4] == second);
}
