#include <catch2/catch_test_macros.hpp>

#include "Editor/EditorLaunchOptions.h"

using namespace Crowny;

TEST_CASE("Editor launch paths are resolved before the working directory changes", "[Application][EditorLaunch]")
{
    EditorLaunchOptions options;
    String error;
    const Path launch = fs::current_path();
    REQUIRE(ParseEditorLaunchOptions({ "editor", "--project", "My Project", "--scene=Assets/Scenes/Level.cwscene", "--render", "out.bmp",
                                       "--width=640", "--height=360", "--frames=5", "--scene-camera", "--quit" },
                                     launch, options, error));
    CHECK(options.Project == launch / "My Project");
    CHECK(options.Scene == launch / "My Project/Assets/Scenes/Level.cwscene");
    CHECK(options.RenderOutput == launch / "out.bmp");
    CHECK(options.Width == 640);
    CHECK(options.Height == 360);
    CHECK(options.Frames == 5);
    CHECK(options.SceneCamera);
    CHECK(options.Quit);
}

TEST_CASE("Editor launch rejects incomplete and conflicting requests", "[Application][EditorLaunch]")
{
    const Vector<Vector<String>> invalid = { { "editor", "--project" },
                                             { "editor", "--project=" },
                                             { "editor", "--scene", "scene.cwscene" },
                                             { "editor", "--play" },
                                             { "editor", "--quit" },
                                             { "editor", "--width=4" },
                                             { "editor", "--project=project", "--render=out.png" },
                                             { "editor", "--project=project", "--render=out.bmp", "--width=0" },
                                             { "editor", "--project=project", "--render=out.bmp", "--height=8193" },
                                             { "editor", "--project=project", "--render=out.bmp", "--frames=-1" },
                                             { "editor", "--project=project", "--render=out.bmp", "--frames=12px" },
                                             { "editor", "--project=project", "--cook-builtins" },
                                             { "editor", "--quit=false" },
                                             { "editor", "--unknown" },
                                             { "editor", "--play", "unexpected", "--project=project" },
                                             { "editor", "--", "--project=project" },
                                             { "editor", "--render-api" } };
    for (const auto& arguments : invalid)
    {
        INFO(arguments.back());
        EditorLaunchOptions options;
        String error;
        CHECK_FALSE(ParseEditorLaunchOptions(arguments, fs::current_path(), options, error));
        CHECK_FALSE(error.empty());
    }
}

TEST_CASE("Editor launch preserves interactive defaults and supports help", "[Application][EditorLaunch]")
{
    EditorLaunchOptions options;
    String error;
    REQUIRE(ParseEditorLaunchOptions({ "editor" }, fs::current_path(), options, error));
    CHECK(options.Project.empty());
    CHECK(options.RenderOutput.empty());
    CHECK_FALSE(options.Play);
    REQUIRE(ParseEditorLaunchOptions({ "editor", "--project=project", "--play" }, fs::current_path(), options, error));
    CHECK(options.Play);
    REQUIRE(ParseEditorLaunchOptions({ "editor", "--help" }, fs::current_path(), options, error));
    CHECK(options.Help);
}
