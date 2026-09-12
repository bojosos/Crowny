#include "cwepch.h"

#include "Editor/SceneGizmoGeometry.h"
#include "Editor/SceneGizmos.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Renderer2D.h"

namespace Crowny::SceneGizmos
{
    namespace
    {
        enum class IconKind
        {
            Point,
            Spot,
            Directional,
            Camera,
            Source,
            Listener,
            Decal
        };
        struct Icon
        {
            Entity Owner;
            IconKind Kind;
            glm::vec3 Screen;
            bool Disabled = false;
        };

        const char* Label(IconKind kind)
        {
            switch (kind)
            {
            case IconKind::Point:
                return "Point light";
            case IconKind::Spot:
                return "Spot light";
            case IconKind::Directional:
                return "Directional light";
            case IconKind::Camera:
                return "Camera";
            case IconKind::Source:
                return "Audio source";
            case IconKind::Listener:
                return "Audio listener";
            case IconKind::Decal:
                return "Decal";
            }
            return "";
        }

        Vector<Icon> CollectIcons(Scene& scene, const glm::mat4& vp, const glm::vec4& bounds, const SceneGizmoSettings& settings)
        {
            Vector<Icon> icons;
            if (!settings.Enabled)
                return icons;
            for (auto handle : scene.GetAllEntitiesWith<TransformComponent>())
            {
                Entity entity(handle, &scene);
                std::array<IconKind, 5> kinds;
                size_t count = 0;
                if (settings.Lights && entity.HasComponent<LightComponent>())
                {
                    const LightType type = entity.GetComponent<LightComponent>().Type;
                    kinds[count++] = type == LightType::Directional ? IconKind::Directional
                                     : type == LightType::Spot      ? IconKind::Spot
                                                                    : IconKind::Point;
                }
                if (settings.Cameras && entity.HasComponent<CameraComponent>())
                    kinds[count++] = IconKind::Camera;
                if (settings.Audio && entity.HasComponent<AudioSourceComponent>())
                    kinds[count++] = IconKind::Source;
                if (settings.Audio && entity.HasComponent<AudioListenerComponent>())
                    kinds[count++] = IconKind::Listener;
                if (entity.HasComponent<DecalComponent>())
                    kinds[count++] = IconKind::Decal;
                if (count == 0)
                    continue;
                const auto projected = ProjectIcon(glm::vec3(entity.GetWorldMatrix()[3]), vp, bounds);
                if (!projected)
                    continue;
                for (size_t i = 0; i < count; ++i)
                {
                    glm::vec3 screen = *projected;
                    screen.x += (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * (settings.IconSize + 2.0f);
                    const bool disabled = (kinds[i] == IconKind::Source && entity.GetComponent<AudioSourceComponent>().GetIsMuted()) ||
                                          (kinds[i] <= IconKind::Directional && !entity.GetComponent<LightComponent>().Enabled);
                    icons.push_back({ entity, kinds[i], screen, disabled });
                }
            }
            // Nearest icon is drawn last and wins picking where icons overlap.
            std::stable_sort(icons.begin(), icons.end(), [](const Icon& a, const Icon& b) { return a.Screen.z > b.Screen.z; });
            return icons;
        }

        bool Contains(const Icon& icon, const glm::vec2& cursor, float size) { return glm::length(cursor - glm::vec2(icon.Screen)) <= size * 0.5f; }

        void DrawSymbol(ImDrawList& draw, IconKind kind, ImVec2 center, float size, ImU32 color)
        {
            const float unit = size / 24.0f;
            const auto point = [&](float x, float y) { return ImVec2(center.x + x * unit, center.y + y * unit); };
            const auto line = [&](float x, float y, float a, float b) { draw.AddLine(point(x, y), point(a, b), color, 1.6f * unit); };
            if (kind == IconKind::Point)
            {
                draw.AddCircle(point(0, -2), 5.0f * unit, color, 20, 1.6f * unit);
                line(-3, 2, -3, 6);
                line(3, 2, 3, 6);
                line(-3, 6, 3, 6);
                line(-2, 8, 2, 8);
            }
            else if (kind == IconKind::Directional)
            {
                draw.AddCircle(center, 3.5f * unit, color, 16, 1.6f * unit);
                for (int i = 0; i < 8; ++i)
                {
                    const float angle = i * glm::two_pi<float>() / 8.0f;
                    line(std::cos(angle) * 5.5f, std::sin(angle) * 5.5f, std::cos(angle) * 8.0f, std::sin(angle) * 8.0f);
                }
                line(1, 1, 8, 8);
                line(4, 8, 8, 8);
                line(8, 4, 8, 8);
            }
            else if (kind == IconKind::Spot)
            {
                draw.AddCircle(point(0, -6), 2 * unit, color, 12, 1.6f * unit);
                line(-1, -4, -8, 7);
                line(1, -4, 8, 7);
                line(-8, 7, 8, 7);
                line(0, -1, 0, 4);
            }
            else if (kind == IconKind::Camera)
            {
                draw.AddRect(point(-8, -5), point(3, 5), color, 2 * unit, 0, 1.6f * unit);
                line(3, -2, 8, -5);
                line(8, -5, 8, 5);
                line(8, 5, 3, 2);
                line(-5, -7, 0, -7);
            }
            else if (kind == IconKind::Decal)
            {
                line(-7, -7, 7, -7); line(7, -7, 7, 2); line(7, 2, 2, 7);
                line(2, 7, -7, 7); line(-7, 7, -7, -7); line(2, 7, 2, 2); line(2, 2, 7, 2);
            }
            else if (kind == IconKind::Source)
            {
                line(-8, -3, -4, -3);
                line(-4, -3, 0, -7);
                line(0, -7, 0, 7);
                line(0, 7, -4, 3);
                line(-4, 3, -8, 3);
                line(-8, 3, -8, -3);
                draw.PathArcTo(point(0, 0), 5 * unit, -0.9f, 0.9f, 10);
                draw.PathStroke(color, 0, 1.6f * unit);
                draw.PathArcTo(point(0, 0), 9 * unit, -0.9f, 0.9f, 10);
                draw.PathStroke(color, 0, 1.6f * unit);
            }
            else
            {
                draw.PathArcTo(point(0, 1), 7 * unit, glm::pi<float>(), glm::two_pi<float>(), 16);
                draw.PathStroke(color, 0, 1.6f * unit);
                draw.AddRect(point(-8, 0), point(-4, 7), color, unit, 0, 1.6f * unit);
                draw.AddRect(point(4, 0), point(8, 7), color, unit, 0, 1.6f * unit);
            }
        }

        bool Finite(const glm::vec3& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

        void Line(const glm::mat4& world, const glm::vec3& a, const glm::vec3& b, const glm::vec4& color)
        {
            const glm::vec3 start = glm::vec3(world * glm::vec4(a, 1));
            const glm::vec3 end = glm::vec3(world * glm::vec4(b, 1));
            if (Finite(start) && Finite(end))
                Renderer2D::DrawLine(start, end, color, 0.015f);
        }

        void Sphere(const glm::mat4& world, float radius, const glm::vec4& color)
        {
            if (!std::isfinite(radius) || radius <= 0)
                return;
            for (int axis = 0; axis < 3; ++axis)
                for (int i = 0; i < 64; ++i)
                {
                    glm::vec3 a(0), b(0);
                    const float t = i * glm::two_pi<float>() / 64;
                    const float next = (i + 1) * glm::two_pi<float>() / 64;
                    a[(axis + 1) % 3] = std::cos(t) * radius;
                    a[(axis + 2) % 3] = std::sin(t) * radius;
                    b[(axis + 1) % 3] = std::cos(next) * radius;
                    b[(axis + 2) % 3] = std::sin(next) * radius;
                    Line(world, a, b, color);
                }
        }

        void Cone(const glm::mat4& world, float radius, float halfAngle, const glm::vec4& color)
        {
            if (!std::isfinite(radius) || radius <= 0 || !std::isfinite(halfAngle))
                return;
            halfAngle = std::clamp(halfAngle, 0.0f, glm::pi<float>());
            for (int i = 0; i < 64; ++i)
            {
                const glm::vec3 a = ConePoint(radius, halfAngle, i * glm::two_pi<float>() / 64);
                const glm::vec3 b = ConePoint(radius, halfAngle, (i + 1) * glm::two_pi<float>() / 64);
                Line(world, a, b, color);
                if (i % 16 == 0)
                    Line(world, glm::vec3(0), a, color);
            }
        }
    } // namespace

    Entity PickIcon(Scene& scene, const glm::mat4& vp, const glm::vec4& bounds, const SceneGizmoSettings& settings, const glm::vec2& cursor)
    {
        if (cursor.x < bounds.x || cursor.x >= bounds.z || cursor.y < bounds.y || cursor.y >= bounds.w)
            return {};
        const auto icons = CollectIcons(scene, vp, bounds, settings);
        for (auto it = icons.rbegin(); it != icons.rend(); ++it)
            if (Contains(*it, cursor, settings.IconSize))
                return it->Owner;
        return {};
    }

    void DrawIcons(Scene& scene, const glm::mat4& vp, const glm::vec4& bounds, const SceneGizmoSettings& settings, const Vector<Entity>& selection)
    {
        const auto icons = CollectIcons(scene, vp, bounds, settings);
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.PushClipRect(ImVec2(bounds.x, bounds.y), ImVec2(bounds.z, bounds.w), true);
        const ImVec2 mouse = ImGui::GetMousePos();
        const Icon* hovered = nullptr;
        for (const Icon& icon : icons)
        {
            const bool selected = std::find(selection.begin(), selection.end(), icon.Owner) != selection.end();
            const ImVec2 center(icon.Screen.x, icon.Screen.y);
            const ImU32 color = icon.Disabled                        ? IM_COL32(145, 145, 145, 230)
                                : icon.Kind <= IconKind::Directional ? IM_COL32(255, 211, 105, 255)
                                : icon.Kind == IconKind::Camera      ? IM_COL32(115, 195, 255, 255)
                                                                     : IM_COL32(127, 228, 178, 255);
            draw.AddCircleFilled(center, settings.IconSize * 0.5f, IM_COL32(24, 26, 30, 210), 24);
            if (selected)
                draw.AddCircle(center, settings.IconSize * 0.5f, IM_COL32(255, 161, 64, 255), 24, 2.0f);
            DrawSymbol(draw, icon.Kind, center, settings.IconSize, color);
            if (icon.Disabled)
                draw.AddLine(ImVec2(center.x - settings.IconSize * 0.3f, center.y + settings.IconSize * 0.3f),
                             ImVec2(center.x + settings.IconSize * 0.3f, center.y - settings.IconSize * 0.3f), color, 1.5f);
            if (Contains(icon, { mouse.x, mouse.y }, settings.IconSize))
                hovered = &icon;
        }
        draw.PopClipRect();
        if (hovered && ImGui::IsWindowHovered() && !ImGui::IsAnyMouseDown() && mouse.x >= bounds.x && mouse.x < bounds.z && mouse.y >= bounds.y &&
            mouse.y < bounds.w)
            ImGui::SetTooltip("%s: %s%s", Label(hovered->Kind), hovered->Owner.GetName().c_str(), hovered->Disabled ? " (disabled / muted)" : "");
    }

    void DrawSelectionGuides(const Vector<Entity>& selection, const SceneGizmoSettings& settings)
    {
        if (!settings.Enabled || !settings.SelectionGuides)
            return;
        for (Entity entity : selection)
        {
            if (!entity.IsValid())
                continue;
            // Physical light/audio distances and camera dimensions do not inherit entity scale.
            const glm::mat4 world = glm::translate(glm::mat4(1), glm::vec3(entity.GetWorldMatrix()[3])) * glm::mat4_cast(entity.GetWorldRotation());
            if (settings.Lights && entity.HasComponent<LightComponent>())
            {
                const auto& light = entity.GetComponent<LightComponent>();
                const glm::vec4 color(1.0f, 0.75f, 0.25f, 1.0f);
                if (light.Type == LightType::Point)
                    Sphere(world, light.Range, color);
                else if (light.Type == LightType::Spot)
                {
                    Cone(world, light.Range, light.SpotOuterAngle * 0.5f, color);
                    Cone(world, light.Range, light.SpotInnerAngle * 0.5f, glm::vec4(1.0f, 0.9f, 0.6f, 1.0f));
                }
                else
                    for (int i = -1; i <= 1; ++i)
                    {
                        const float x = i * 0.4f;
                        Line(world, { x, 0, 0 }, { x, 0, -2 }, color);
                        Line(world, { x, 0, -2 }, { x - 0.15f, 0, -1.7f }, color);
                        Line(world, { x, 0, -2 }, { x + 0.15f, 0, -1.7f }, color);
                    }
                if (light.Type != LightType::Directional)
                    Sphere(world, light.SourceRadius, color);
            }
            if (settings.Cameras && entity.HasComponent<CameraComponent>())
            {
                const SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
                const bool perspective = camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective;
                const auto corners =
                  FrustumCorners(perspective, camera.GetPerspectiveVerticalFOV(), camera.GetOrthographicSize(), camera.GetAspectRatio(),
                                 perspective ? camera.GetPerspectiveNearClip() : camera.GetOrthographicNearClip(),
                                 perspective ? camera.GetPerspectiveFarClip() : camera.GetOrthographicFarClip());
                const glm::vec4 color(0.4f, 0.75f, 1.0f, 1.0f);
                for (size_t i = 0; i < 4; ++i)
                {
                    Line(world, corners[i], corners[(i + 1) % 4], color);
                    Line(world, corners[i + 4], corners[(i + 1) % 4 + 4], color);
                    Line(world, corners[i], corners[i + 4], color);
                }
            }
            if (settings.Audio && entity.HasComponent<AudioSourceComponent>())
            {
                const auto& source = entity.GetComponent<AudioSourceComponent>();
                const glm::vec4 color(0.35f, 0.9f, 0.6f, 1.0f);
                Sphere(world, source.GetMinDistance(), color);
                Sphere(world, source.GetMaxDistance(), glm::vec4(0.25f, 0.55f, 0.4f, 1.0f));
                if (source.GetConeOuterAngle() < 360.0f)
                {
                    Cone(world, source.GetMinDistance(), glm::radians(source.GetConeInnerAngle()) * 0.5f, color);
                    Cone(world, source.GetMinDistance(), glm::radians(source.GetConeOuterAngle()) * 0.5f, glm::vec4(1, 0.6f, 0.2f, 1));
                }
            }
            if (settings.Audio && entity.HasComponent<AudioListenerComponent>())
            {
                const glm::vec4 color(0.35f, 0.9f, 0.6f, 1);
                Line(world, { 0, 0, 0 }, { 0, 0, -1 }, color);
                Line(world, { 0, 0, -1 }, { -0.15f, 0, -0.7f }, color);
                Line(world, { 0, 0, -1 }, { 0.15f, 0, -0.7f }, color);
            }
        }
    }
} // namespace Crowny::SceneGizmos
