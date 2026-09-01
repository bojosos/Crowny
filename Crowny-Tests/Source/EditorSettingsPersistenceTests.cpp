#include <catch2/catch_test_macros.hpp>

#include "Editor/Settings/EditorSettingsPersistence.h"

using namespace Crowny;

TEST_CASE("Recent project paths normalize before storage", "[Editor][Settings][RecentProjects]")
{
    CHECK(NormalizeProjectPath(Path("/projects/Game/../Game")) == Path("/projects/Game"));
}

TEST_CASE("Recent projects stay unique newest-first and bounded", "[Editor][Settings][RecentProjects]")
{
    EditorSettings settings;
    const Path first = "/projects/First";
    const Path second = "/projects/Second";
    const Path third = "/projects/Third";
    const Path fourth = "/projects/Fourth";
    const Path fifth = "/projects/Fifth";
    const Path sixth = "/projects/Sixth";

    RecordRecentProject(settings, first, 1);
    RecordRecentProject(settings, second, 2);
    RecordRecentProject(settings, third, 3);
    RecordRecentProject(settings, fourth, 4);
    RecordRecentProject(settings, fifth, 5);
    RecordRecentProject(settings, sixth, 6);
    RecordRecentProject(settings, third, 7);

    CHECK(settings.RecentProjects[0].ProjectPath == third);
    CHECK(settings.RecentProjects[0].Timestamp == 7);
    CHECK(settings.RecentProjects[1].ProjectPath == sixth);
    CHECK(settings.RecentProjects[2].ProjectPath == fifth);
    CHECK(settings.RecentProjects[3].ProjectPath == fourth);
    CHECK(settings.RecentProjects[4].ProjectPath == second);
    CHECK(settings.LastOpenProject == third);
}

TEST_CASE("Legacy recent paths normalize without dropping later valid entries", "[Editor][Settings][RecentProjects]")
{
    EditorSettings settings;
    settings.RecentProjects[0] = { "/projects/Game", 30 };
    settings.RecentProjects[1] = { "/projects/Tools/../Game", 20 };
    settings.RecentProjects[2] = { "/projects/Other", 10 };
    settings.LastOpenProject = "/projects/Tools/../Game";

    CHECK(NormalizeRecentProjects(settings, [](const Path& path) { return path.lexically_normal(); }));
    CHECK(settings.RecentProjects[0].ProjectPath == Path("/projects/Game"));
    CHECK(settings.RecentProjects[0].Timestamp == 30);
    CHECK(settings.RecentProjects[1].ProjectPath == Path("/projects/Other"));
    CHECK(settings.RecentProjects[2].ProjectPath.empty());
    CHECK(settings.LastOpenProject == Path("/projects/Game"));
}

TEST_CASE("Startup selects the newest existing recent project", "[Editor][Settings][RecentProjects]")
{
    EditorSettings settings;
    settings.RecentProjects[0] = { "/projects/Missing", 30 };
    settings.RecentProjects[1] = { "/projects/NewestExisting", 20 };
    settings.RecentProjects[2] = { "/projects/OlderExisting", 10 };
    settings.LastOpenProject = "/projects/Legacy";

    const Path selected = SelectStartupProject(settings, [](const Path& path) {
        return path == Path("/projects/NewestExisting") || path == Path("/projects/OlderExisting") || path == Path("/projects/Legacy");
    });

    CHECK(selected == Path("/projects/NewestExisting"));
    CHECK(settings.RecentProjects[0].ProjectPath == Path("/projects/Missing"));
    CHECK(settings.RecentProjects[2].ProjectPath == Path("/projects/OlderExisting"));
}

TEST_CASE("Startup falls back to the legacy last project", "[Editor][Settings][RecentProjects]")
{
    EditorSettings settings;
    settings.RecentProjects[0] = { "/projects/Missing", 20 };
    settings.LastOpenProject = "/projects/Legacy";

    CHECK(SelectStartupProject(settings, [](const Path& path) { return path == Path("/projects/Legacy"); }) == Path("/projects/Legacy"));
}

TEST_CASE("Global editor settings take precedence over legacy worktree settings", "[Editor][Settings][Persistence]")
{
    const Path persistent = "/user/Crowny/Editor/Settings.yaml";
    const Path legacy = "/checkout/Crowny-Editor/Editor/Settings.yaml";
    const EditorSettingsPaths paths = SelectEditorSettingsPaths(persistent, legacy, true, true);

    CHECK(paths.PersistentPath == persistent);
    CHECK(paths.LoadPath == persistent);
    CHECK_FALSE(paths.MigrateLegacy);
}

TEST_CASE("Legacy editor settings migrate only when global settings are absent", "[Editor][Settings][Persistence]")
{
    const Path persistent = "/user/Crowny/Editor/Settings.yaml";
    const Path legacy = "/checkout/Crowny-Editor/Editor/Settings.yaml";
    const EditorSettingsPaths paths = SelectEditorSettingsPaths(persistent, legacy, false, true);

    CHECK(paths.PersistentPath == persistent);
    CHECK(paths.LoadPath == legacy);
    CHECK(paths.MigrateLegacy);
}

TEST_CASE("New editor settings start without a load source", "[Editor][Settings][Persistence]")
{
    const Path persistent = "/user/Crowny/Editor/Settings.yaml";
    const EditorSettingsPaths paths = SelectEditorSettingsPaths(persistent, "/checkout/Editor/Settings.yaml", false, false);

    CHECK(paths.PersistentPath == persistent);
    CHECK(paths.LoadPath.empty());
    CHECK_FALSE(paths.MigrateLegacy);
}
