#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/EditorCamera.h"
#include "Crowny/Scene/SceneCamera.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

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

    bool IsNear(const glm::mat4& lhs, const glm::mat4& rhs, float epsilon = 0.00001f)
    {
        for (uint32_t column = 0; column < 4; column++)
            for (uint32_t row = 0; row < 4; row++)
                if (std::abs(lhs[column][row] - rhs[column][row]) > epsilon)
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

TEST_CASE("Editor camera normalizes invalid projection inputs", "[Renderer][Camera]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    EditorCamera camera(nan, 0.0f, -infinity, nan);

    CHECK(IsFinite(camera.GetProjection()));
    CHECK(IsFinite(camera.GetViewMatrix()));
    CHECK(IsFinite(camera.GetViewProjection()));

    const glm::mat4 projection = camera.GetProjection();
    camera.SetViewportSize(nan, infinity);
    CHECK(IsNear(camera.GetProjection(), projection));
}

TEST_CASE("Editor camera pose setters keep derived matrices coherent", "[Renderer][Camera]")
{
    EditorCamera camera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera.SetFocalPoint({ 2.0f, -1.0f, 4.0f });
    camera.SetPitch(0.25f);
    camera.SetYaw(-0.5f);
    camera.SetRoll(0.1f);
    camera.SetDistance(7.0f);

    const glm::vec3 expectedPosition = camera.GetFocalPoint() - camera.GetForwardDirection() * camera.GetDistance();
    CHECK(glm::length(camera.GetPosition() - expectedPosition) < 0.00001f);

    const glm::mat4& view = camera.GetViewMatrix();
    const glm::vec4 cameraOrigin = view * glm::vec4(camera.GetPosition(), 1.0f);
    CHECK(glm::length(glm::vec3(cameraOrigin)) < 0.00001f);
    CHECK(IsNear(camera.GetViewProjection(), camera.GetProjection() * view));

    const glm::vec3 requestedPosition(-3.0f, 5.0f, 8.0f);
    camera.SetPosition(requestedPosition);
    CHECK(glm::length(camera.GetPosition() - requestedPosition) < 0.00001f);
    CHECK(IsNear(camera.GetViewProjection(), camera.GetProjection() * camera.GetViewMatrix()));
}

TEST_CASE("Editor camera reuses stable matrices without allocations", "[Renderer][Camera][Memory][Frame]")
{
    EditorCamera camera(45.0f, 4.0f / 3.0f, 0.1f, 1000.0f);
    camera.SetViewportSize(1280.0f, 720.0f);
    const glm::mat4 projection = camera.GetProjection();
    CHECK(std::abs(projection[1][1] / projection[0][0] - 16.0f / 9.0f) < 0.00001f);
    const glm::mat4* view = &camera.GetViewMatrix();
    const glm::mat4* viewProjection = &camera.GetViewProjection();

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 1000; frame++)
    {
        camera.SetViewportSize(1280.0f, 720.0f);
        view = &camera.GetViewMatrix();
        viewProjection = &camera.GetViewProjection();
    }
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(view == &camera.GetViewMatrix());
    CHECK(viewProjection == &camera.GetViewProjection());
    CHECK(IsNear(camera.GetProjection(), projection));
    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
}
