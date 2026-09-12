#include "Crowny/Ecs/Components.h"
#include "Editor/SceneGizmoGeometry.h"
#include "Editor/SceneGizmos.h"

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

using namespace Crowny::SceneGizmos;
using namespace Crowny;

TEST_CASE("Scene icon picking follows visibility, depth order, and parent transforms", "[Editor][Gizmos]")
{
    Scene scene(false);
    const glm::mat4 vp = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 100.0f);
    const glm::vec4 bounds(100, 50, 900, 450);
    SceneGizmoSettings settings;
    Entity farEntity = scene.CreateEntity("Far light");
    farEntity.AddComponent<LightComponent>();
    farEntity.SetWorldPosition({ 0, 0, -20 });
    Entity parent = scene.CreateEntity("Parent");
    parent.SetWorldPosition({ 0, 0, -10 });
    Entity nearEntity = scene.CreateEntity("Near camera");
    nearEntity.AddComponent<CameraComponent>();
    nearEntity.SetParent(parent);
    nearEntity.GetTransform().SetPosition({ 0, 0, 0 });
    CHECK(PickIcon(scene, vp, bounds, settings, { 500, 250 }) == nearEntity);
    settings.Cameras = false;
    CHECK(PickIcon(scene, vp, bounds, settings, { 500, 250 }) == farEntity);
    settings.Lights = false;
    CHECK_FALSE(PickIcon(scene, vp, bounds, settings, { 500, 250 }));
    settings.Cameras = true;
    settings.Enabled = false;
    CHECK_FALSE(PickIcon(scene, vp, bounds, settings, { 500, 250 }));
    settings.Enabled = true;
    CHECK_FALSE(PickIcon(scene, vp, bounds, settings, { 99, 250 }));
    parent.SetWorldPosition({ 0, 0, 10 });
    CHECK_FALSE(PickIcon(scene, vp, bounds, settings, { 500, 250 }));
}

TEST_CASE("Multiple component icons remain independently reachable", "[Editor][Gizmos]")
{
    Scene scene(false);
    Entity entity = scene.CreateEntity("Camera with listener");
    entity.SetWorldPosition({ 0, 0, -10 });
    entity.AddComponent<CameraComponent>();
    entity.AddComponent<AudioListenerComponent>();
    const glm::mat4 vp = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 100.0f);
    const glm::vec4 bounds(100, 50, 900, 450);
    SceneGizmoSettings settings;
    CHECK(PickIcon(scene, vp, bounds, settings, { 487, 250 }) == entity);
    CHECK(PickIcon(scene, vp, bounds, settings, { 513, 250 }) == entity);
    settings.Audio = false;
    CHECK(PickIcon(scene, vp, bounds, settings, { 500, 250 }) == entity);
    CHECK_FALSE(PickIcon(scene, vp, bounds, settings, { 513, 250 }));
}

TEST_CASE("Scene icons project into viewport coordinates and reject clipped origins", "[Editor][Gizmos]")
{
    const glm::mat4 vp = glm::perspective(glm::radians(60.0f), 2.0f, 0.1f, 100.0f);
    const glm::vec4 bounds(100, 50, 900, 450);
    const auto center = ProjectIcon({ 0, 0, -10 }, vp, bounds);
    REQUIRE(center);
    CHECK(std::abs(center->x - 500) < 0.001f);
    CHECK(std::abs(center->y - 250) < 0.001f);
    const auto above = ProjectIcon({ 0, 1, -10 }, vp, bounds);
    REQUIRE(above);
    CHECK(above->y < center->y);
    CHECK_FALSE(ProjectIcon({ 0, 0, 10 }, vp, bounds));
    CHECK_FALSE(ProjectIcon({ 0, 0, -0.01f }, vp, bounds));
    CHECK_FALSE(ProjectIcon({ 0, 0, -101 }, vp, bounds));
    CHECK_FALSE(ProjectIcon({ 100, 0, -10 }, vp, bounds));
    CHECK_FALSE(ProjectIcon({ std::numeric_limits<float>::quiet_NaN(), 0, -10 }, vp, bounds));
    CHECK_FALSE(ProjectIcon({ 0, 0, -10 }, vp, { 0, 0, 0, 0 }));
}

TEST_CASE("Camera guides preserve perspective and orthographic clip planes", "[Editor][Gizmos]")
{
    const auto perspective = FrustumCorners(true, glm::half_pi<float>(), 10, 2, 1, 10);
    CHECK(glm::length(perspective[0] - glm::vec3(-2, -1, -1)) < 0.001f);
    CHECK(glm::length(perspective[6] - glm::vec3(20, 10, -10)) < 0.001f);
    const auto orthographic = FrustumCorners(false, 1, 10, 2, -1, 100);
    CHECK(glm::length(orthographic[0] - glm::vec3(-10, -5, 1)) < 0.001f);
    CHECK(glm::length(orthographic[6] - glm::vec3(10, 5, -100)) < 0.001f);
}

TEST_CASE("Audio cone guides stay finite at and beyond a hemisphere", "[Editor][Gizmos]")
{
    for (float degrees : { 0.0f, 45.0f, 180.0f, 270.0f, 359.0f, 360.0f })
    {
        const glm::vec3 point = ConePoint(5.0f, glm::radians(degrees) * 0.5f, 0.7f);
        CHECK(std::isfinite(point.x));
        CHECK(std::isfinite(point.y));
        CHECK(std::isfinite(point.z));
        CHECK(std::abs(glm::length(point) - 5.0f) < 0.001f);
    }
    CHECK(ConePoint(5, glm::radians(270.0f) * 0.5f, 0).z > 0);
}
