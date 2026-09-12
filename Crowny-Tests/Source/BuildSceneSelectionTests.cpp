#include "Build/BuildSceneSelection.h"
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Build scene selection distinguishes complete filenames and folders", "[Build][Editor][SceneSelection]")
{
    const UUID first(1, 0, 0, 0), second(2, 0, 0, 0), third(3, 0, 0, 0);
    const auto scenes = MakeBuildSceneOptions({ { first, "Project/Assets/Main.cwscene" },
                                                { second, "Project/Assets/Main.cwscene.cwscene" },
                                                { third, "Project/Assets/Levels/Main.cwscene" },
                                                { first, "Project/Assets/Main.cwscene" } },
                                              "Project/Assets");
    REQUIRE(scenes.size() == 3);
    CHECK(scenes[0].DisplayName == "Levels/Main.cwscene");
    CHECK(scenes[1].DisplayName == "Main.cwscene");
    CHECK(scenes[2].DisplayName == "Main.cwscene.cwscene");
    CHECK(scenes[1].Id == first);
    CHECK(scenes[2].Id == second);
}
