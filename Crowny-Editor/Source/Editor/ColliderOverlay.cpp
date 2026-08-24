#include "cwepch.h"

#include "Editor/ColliderOverlay.h"

#include "Crowny/Common/Math.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Scene/Scene.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Crowny::ColliderOverlay
{
    namespace
    {
        constexpr uint32_t CIRCLE_SEGMENTS = 32;
        constexpr float LINE_THICKNESS = 0.01f;

        glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point) { return glm::vec3(transform * glm::vec4(point, 1.0f)); }

        void DrawArc(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& firstAxis, const glm::vec3& secondAxis, float radius,
                     float startAngle, float endAngle, uint32_t segments, const glm::vec4& color)
        {
            glm::vec3 previous =
              TransformPoint(transform, center + firstAxis * (std::cos(startAngle) * radius) + secondAxis * (std::sin(startAngle) * radius));
            for (uint32_t segment = 1; segment <= segments; segment++)
            {
                const float factor = static_cast<float>(segment) / static_cast<float>(segments);
                const float angle = glm::mix(startAngle, endAngle, factor);
                const glm::vec3 point =
                  TransformPoint(transform, center + firstAxis * (std::cos(angle) * radius) + secondAxis * (std::sin(angle) * radius));
                Renderer2D::DrawLine(previous, point, color, LINE_THICKNESS);
                previous = point;
            }
        }

        void DrawCircle(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& firstAxis, const glm::vec3& secondAxis, float radius,
                        const glm::vec4& color)
        {
            DrawArc(transform, center, firstAxis, secondAxis, radius, 0.0f, glm::two_pi<float>(), CIRCLE_SEGMENTS, color);
        }

        void DrawWireBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color)
        {
            const std::array<glm::vec3, 8> localCorners = {
                glm::vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z), glm::vec3(halfExtents.x, -halfExtents.y, -halfExtents.z),
                glm::vec3(halfExtents.x, halfExtents.y, -halfExtents.z),   glm::vec3(-halfExtents.x, halfExtents.y, -halfExtents.z),
                glm::vec3(-halfExtents.x, -halfExtents.y, halfExtents.z),  glm::vec3(halfExtents.x, -halfExtents.y, halfExtents.z),
                glm::vec3(halfExtents.x, halfExtents.y, halfExtents.z),    glm::vec3(-halfExtents.x, halfExtents.y, halfExtents.z)
            };
            std::array<glm::vec3, 8> corners;
            for (size_t i = 0; i < corners.size(); i++)
                corners[i] = TransformPoint(transform, localCorners[i]);

            constexpr std::array<std::array<uint8_t, 2>, 12> edges = {
                { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } }
            };
            for (const auto& edge : edges)
                Renderer2D::DrawLine(corners[edge[0]], corners[edge[1]], color, LINE_THICKNESS);
        }

        void DrawWireSphere(const glm::mat4& transform, float radius, const glm::vec4& color)
        {
            DrawCircle(transform, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), radius, color);
            DrawCircle(transform, glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), radius, color);
            DrawCircle(transform, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), radius, color);
        }

        void DrawWireCapsule(const glm::mat4& transform, float radius, float height, const glm::vec4& color)
        {
            const float halfSegment = std::max(height * 0.5f - radius, 0.0f);
            if (halfSegment <= std::numeric_limits<float>::epsilon())
            {
                DrawWireSphere(transform, radius, color);
                return;
            }

            const glm::vec3 bottomCenter(0.0f, -halfSegment, 0.0f);
            const glm::vec3 topCenter(0.0f, halfSegment, 0.0f);
            const glm::vec3 xAxis(1.0f, 0.0f, 0.0f);
            const glm::vec3 yAxis(0.0f, 1.0f, 0.0f);
            const glm::vec3 zAxis(0.0f, 0.0f, 1.0f);

            DrawCircle(transform, bottomCenter, xAxis, zAxis, radius, color);
            DrawCircle(transform, topCenter, xAxis, zAxis, radius, color);

            const std::array<glm::vec3, 4> tangentDirections = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
                                                                 glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
            for (const glm::vec3& direction : tangentDirections)
            {
                Renderer2D::DrawLine(TransformPoint(transform, bottomCenter + direction * radius),
                                     TransformPoint(transform, topCenter + direction * radius), color, LINE_THICKNESS);
            }

            constexpr uint32_t hemisphereSegments = CIRCLE_SEGMENTS / 2;
            DrawArc(transform, topCenter, xAxis, yAxis, radius, 0.0f, glm::pi<float>(), hemisphereSegments, color);
            DrawArc(transform, topCenter, zAxis, yAxis, radius, 0.0f, glm::pi<float>(), hemisphereSegments, color);
            DrawArc(transform, bottomCenter, xAxis, yAxis, radius, glm::pi<float>(), glm::two_pi<float>(), hemisphereSegments, color);
            DrawArc(transform, bottomCenter, zAxis, yAxis, radius, glm::pi<float>(), glm::two_pi<float>(), hemisphereSegments, color);
        }

        glm::mat4 GetPhysics2DBodyTransform(const Transform& worldTransform)
        {
            const float rotation = glm::eulerAngles(worldTransform.GetRotation()).z;
            return Math::ComposeMatrix(worldTransform.GetPosition(), glm::angleAxis(rotation, glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(1.0f));
        }

        glm::mat4 GetPhysics3DColliderTransform(const Transform& worldTransform, const Collider3D& collider)
        {
            const glm::quat worldRotation = worldTransform.GetRotation();
            const glm::vec3 center = worldTransform.GetPosition() + worldRotation * (collider.GetOffset() * worldTransform.GetScale());
            return Math::ComposeMatrix(center, glm::normalize(worldRotation * collider.GetRotation()), glm::vec3(1.0f));
        }

        void Draw2DColliders(Scene& scene, const glm::vec4& color)
        {
            for (auto handle : scene.GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>())
            {
                Entity entity(handle, &scene);
                const Transform& world = entity.GetWorldTransform();
                const BoxCollider2DComponent& collider = entity.GetComponent<BoxCollider2DComponent>();
                const glm::vec2 halfExtents = glm::abs(collider.GetSize() * glm::vec2(world.GetScale()));
                const glm::mat4 transform = GetPhysics2DBodyTransform(world) *
                                            glm::translate(glm::mat4(1.0f), glm::vec3(collider.GetOffset(), 0.0f)) *
                                            glm::scale(glm::mat4(1.0f), glm::vec3(halfExtents * 2.0f, 1.0f));
                Renderer2D::DrawRect(transform, color, LINE_THICKNESS);
            }

            for (auto handle : scene.GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>())
            {
                Entity entity(handle, &scene);
                const Transform& world = entity.GetWorldTransform();
                const CircleCollider2DComponent& collider = entity.GetComponent<CircleCollider2DComponent>();
                const glm::vec2 scale = glm::abs(glm::vec2(world.GetScale()));
                const float radius = std::abs(collider.GetRadius()) * 0.5f * (scale.x + scale.y);
                const glm::mat4 transform = GetPhysics2DBodyTransform(world) *
                                            glm::translate(glm::mat4(1.0f), glm::vec3(collider.GetOffset(), 0.0f)) *
                                            glm::scale(glm::mat4(1.0f), glm::vec3(radius * 2.0f, radius * 2.0f, 1.0f));
                Renderer2D::DrawCircle(transform, color, 0.05f);
            }
        }

        void Draw3DColliders(Scene& scene, const glm::vec4& color)
        {
            for (auto handle : scene.GetAllEntitiesWith<TransformComponent, BoxCollider3DComponent>())
            {
                Entity entity(handle, &scene);
                const Transform& world = entity.GetWorldTransform();
                const BoxCollider3DComponent& collider = entity.GetComponent<BoxCollider3DComponent>();
                const glm::vec3 halfExtents = glm::max(glm::abs(collider.GetSize()) * glm::abs(world.GetScale()) * 0.5f, glm::vec3(0.0005f));
                DrawWireBox(GetPhysics3DColliderTransform(world, collider), halfExtents, color);
            }

            for (auto handle : scene.GetAllEntitiesWith<TransformComponent, SphereCollider3DComponent>())
            {
                Entity entity(handle, &scene);
                const Transform& world = entity.GetWorldTransform();
                const SphereCollider3DComponent& collider = entity.GetComponent<SphereCollider3DComponent>();
                const glm::vec3 scale = glm::abs(world.GetScale());
                const float radius = std::max(collider.GetRadius() * std::max({ scale.x, scale.y, scale.z }), 0.001f);
                DrawWireSphere(GetPhysics3DColliderTransform(world, collider), radius, color);
            }

            for (auto handle : scene.GetAllEntitiesWith<TransformComponent, CapsuleCollider3DComponent>())
            {
                Entity entity(handle, &scene);
                const Transform& world = entity.GetWorldTransform();
                const CapsuleCollider3DComponent& collider = entity.GetComponent<CapsuleCollider3DComponent>();
                const glm::vec3 scale = glm::abs(world.GetScale());
                const float radius = std::max(collider.GetRadius() * std::max(scale.x, scale.z), 0.001f);
                const float height = std::max(collider.GetHeight() * scale.y, radius * 2.0f);
                DrawWireCapsule(GetPhysics3DColliderTransform(world, collider), radius, height, color);
            }
        }
    } // namespace

    void Draw(Scene& scene, const glm::vec4& color)
    {
        Draw2DColliders(scene, color);
        Draw3DColliders(scene, color);
    }
} // namespace Crowny::ColliderOverlay
