#include "Crowny/Scene/SceneCamera.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace Crowny;

namespace
{
    bool IsFinite(const glm::mat4& matrix)
    {
        for (uint32_t column = 0; column < 4; column++)
            for (uint32_t row = 0; row < 4; row++)
                if (!std::isfinite(matrix[column][row]))
                    return false;
        return true;
    }
} // namespace

TEST_CASE("Scene camera keeps projection parameters valid", "[Renderer][Camera]")
{
    SceneCamera camera;
    CHECK(camera.GetAspectRatio() == 1.0f);
    CHECK(IsFinite(camera.GetProjection()));

    camera.SetViewportSize(0, 0);
    CHECK(camera.GetAspectRatio() == 1.0f);
    CHECK(IsFinite(camera.GetProjection()));

    camera.SetViewportSize(1920, 1080);
    CHECK(camera.GetAspectRatio() == 1920.0f / 1080.0f);
    camera.SetPerspective(0.0f, -10.0f, -1.0f);
    CHECK(camera.GetPerspectiveVerticalFOV() == glm::radians(1.0f));
    CHECK(camera.GetPerspectiveNearClip() > 0.0f);
    CHECK(camera.GetPerspectiveFarClip() > camera.GetPerspectiveNearClip());
    CHECK(IsFinite(camera.GetProjection()));

    camera.SetPerspectiveNearClip(2000.0f);
    CHECK(camera.GetPerspectiveFarClip() > camera.GetPerspectiveNearClip());
    camera.SetPerspectiveFarClip(0.0f);
    CHECK(camera.GetPerspectiveFarClip() > camera.GetPerspectiveNearClip());
}

TEST_CASE("Scene camera clamps viewport rectangles and orthographic ranges", "[Renderer][Camera]")
{
    SceneCamera camera;
    camera.SetViewportRect({ -0.5f, 0.75f, 2.0f, 0.75f });
    const glm::vec4 viewport = camera.GetViewportRect();
    CHECK(viewport.x == 0.0f);
    CHECK(viewport.y == 0.75f);
    CHECK(viewport.z == 1.0f);
    CHECK(viewport.w == 0.25f);

    camera.SetOrthographic(0.0f, 5.0f, 5.0f);
    CHECK(camera.GetOrthographicSize() > 0.0f);
    CHECK(camera.GetOrthographicFarClip() > camera.GetOrthographicNearClip());
    CHECK(IsFinite(camera.GetProjection()));
}
