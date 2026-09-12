#include "cwepch.h"

#include "Crowny/Scene/Scene.h"
#include "Editor/DecalBoundsInteraction.h"
#include <glm/gtc/constants.hpp>

namespace Crowny
{
    namespace
    {
        class DecalBoundsAction : public UndoAction
        {
        public:
            DecalBoundsAction(Entity target, const DecalSettings& before, const DecalSettings& after)
              : UndoAction("Resize decal"), m_Scene(target.GetScene()), m_Id(target.GetUuid()), m_Before(before), m_After(after)
            {
                if (target.HasComponent<PrefabComponent>())
                {
                    const auto& prefab = target.GetComponent<PrefabComponent>();
                    DecalSettings old = before, updated = after;
                    old.Visit([&](const char* name, const auto& value) {
                        updated.Visit([&](const char* newName, const auto& newValue) {
                            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::decay_t<decltype(newValue)>>)
                                if (StringView(name) == newName && value != newValue)
                                {
                                    String path = String("Decal.") + name;
                                    m_Overrides.push_back({ path, prefab.IsPropertyOverridden(path) });
                                }
                        });
                    });
                }
            }
            void Commit() override { Apply(m_After, true); }
            void Revert() override { Apply(m_Before, false); }
            Entity GetFocusEntity() const override { return m_Scene->TryGetEntityFromUuid(m_Id); }

        private:
            void Apply(const DecalSettings& settings, bool committed)
            {
                Entity entity = GetFocusEntity();
                if (entity && entity.HasComponent<DecalComponent>())
                    static_cast<DecalSettings&>(entity.GetComponent<DecalComponent>()) = settings;
                if (entity && entity.HasComponent<PrefabComponent>())
                    for (const auto& [path, wasOverridden] : m_Overrides)
                    {
                        auto& prefab = entity.GetComponent<PrefabComponent>();
                        if (committed || wasOverridden)
                            prefab.MarkOverridden(path);
                        else
                            prefab.ClearOverride(path);
                    }
            }
            Ref<Scene> m_Scene;
            UUID m_Id;
            DecalSettings m_Before, m_After;
            Vector<std::pair<String, bool>> m_Overrides;
        };
    } // namespace
    DecalBoundsInteraction::~DecalBoundsInteraction() { Cancel(); }
    bool DecalBoundsInteraction::Begin(Entity target, int handle)
    {
        Cancel();
        if (!target || !target.HasComponent<DecalComponent>() || handle < 0)
            return false;
        const auto& settings = target.GetComponent<DecalComponent>();
        if (handle >= (settings.Projection == DecalProjection::Box ? 6 : 7))
            return false;
        m_Scene = Ref<Scene>(target.GetScene());
        m_Target = target.GetUuid();
        m_Before = settings;
        m_Handle = handle;
        return true;
    }
    void DecalBoundsInteraction::Update(float delta, bool symmetric, float snap)
    {
        Entity entity = m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{};
        if (!IsUsing() || !entity || !entity.HasComponent<DecalComponent>())
            return;
        if (snap > 0 && std::isfinite(snap))
            delta = std::round(delta / snap) * snap;
        static_cast<DecalSettings&>(entity.GetComponent<DecalComponent>()) = Resize(m_Before, m_Handle, delta, symmetric);
    }
    void DecalBoundsInteraction::Commit() { Finish(false); }
    DecalSettings DecalBoundsInteraction::Resize(const DecalSettings& before, int handle, float delta, bool symmetric)
    {
        DecalSettings after = before;
        if (!std::isfinite(delta))
            return after;
        if (before.Projection == DecalProjection::Box)
        {
            if (handle < 0 || handle >= 6)
                return after;
            int axis = handle / 2;
            float sign = handle % 2 == 0 ? -1.0f : 1.0f;
            after.Size[axis] = std::max(0.001f, before.Size[axis] + delta * (symmetric ? 2.0f : 1.0f));
            if (!symmetric)
                after.Offset[axis] += sign * (after.Size[axis] - before.Size[axis]) * 0.5f;
        }
        else
        {
            switch (handle)
            {
            case 0:
                after.BottomRadius = std::max(0.001f, before.BottomRadius + delta);
                break;
            case 1:
                after.TopRadius = std::max(0.001f, before.TopRadius + delta);
                break;
            case 2:
            case 3:
                after.Height = std::max(0.001f, before.Height + delta * (symmetric ? 2.0f : 1.0f));
                if (!symmetric)
                    after.Offset.y += (handle == 2 ? -1.0f : 1.0f) * (after.Height - before.Height) * 0.5f;
                break;
            case 4:
                after.ShellThickness = std::max(0.001f, before.ShellThickness + 2 * delta);
                break;
            case 5:
                after.Arc = std::clamp(before.Arc + delta, 0.1f, 360.0f);
                break;
            case 6:
                after.SeamRotation = before.SeamRotation + delta;
                break;
            default:
                break;
            }
        }
        if (before.PreserveTexelDensity)
        {
            if (before.Projection == DecalProjection::Box)
                after.UVScale *= glm::vec2(after.Size) / glm::max(glm::vec2(before.Size), glm::vec2(0.001f));
            else
                after.UVScale *= glm::vec2((after.BottomRadius + after.TopRadius) * after.Arc /
                                             std::max((before.BottomRadius + before.TopRadius) * before.Arc, 0.001f),
                                           after.Height / std::max(before.Height, 0.001f));
        }
        return after;
    }
    void DecalBoundsInteraction::Finish(bool cancel)
    {
        if (!IsUsing())
            return;
        Entity entity = m_Scene ? m_Scene->TryGetEntityFromUuid(m_Target) : Entity{};
        if (entity && entity.HasComponent<DecalComponent>())
        {
            auto& settings = static_cast<DecalSettings&>(entity.GetComponent<DecalComponent>());
            if (cancel)
                settings = m_Before;
            else if (!(settings == m_Before))
            {
                auto action = CreateRef<DecalBoundsAction>(entity, m_Before, settings);
                action->Commit();
                UndoRedo::Get().RegisterAction(action);
            }
        }
        m_Handle = -1;
        m_Scene = nullptr;
        m_Target = {};
    }
    void DecalBoundsInteraction::Cancel() { Finish(true); }
    glm::vec3 DecalBoundsInteraction::CylinderPoint(const DecalSettings& s, float angle, float heightFraction, float shellOffset)
    {
        const float radius = std::max(glm::mix(s.BottomRadius, s.TopRadius, heightFraction) + shellOffset, 0.001f);
        return { std::sin(angle) * radius, (heightFraction - 0.5f) * s.Height, std::cos(angle) * radius };
    }

    std::array<DecalBoundsInteraction::Handle, 7> DecalBoundsInteraction::CylinderHandles(const DecalSettings& s)
    {
        const float seam = glm::radians(s.SeamRotation), arc = glm::radians(s.Arc);
        const float radialAngle = seam + arc * 0.25f;
        const glm::vec3 radial(std::sin(radialAngle), 0, std::cos(radialAngle));
        const auto angularDirection = [&](float angle, float height) {
            return glm::vec3(std::cos(angle), 0, -std::sin(angle)) * glm::mix(s.BottomRadius, s.TopRadius, height) * glm::radians(1.0f);
        };
        return { { { CylinderPoint(s, radialAngle, 0), radial, "Bottom radius" },
                   { CylinderPoint(s, radialAngle, 1), radial, "Top radius" },
                   { CylinderPoint(s, seam + arc * 0.5f, 0), { 0, -1, 0 }, "Bottom edge" },
                   { CylinderPoint(s, seam + arc * 0.5f, 1), { 0, 1, 0 }, "Top edge" },
                   { CylinderPoint(s, radialAngle, 0.5f, s.ShellThickness * 0.5f), radial, "Shell thickness" },
                   { CylinderPoint(s, seam + arc, 0.25f), angularDirection(seam + arc, 0.25f), "Wrap angle" },
                   { CylinderPoint(s, seam, 0.75f), angularDirection(seam, 0.75f), "Seam rotation" } } };
    }

    bool DecalBoundsInteraction::Draw(Entity target, const glm::mat4& vp, const glm::vec4& viewport, bool editable, float snap, float angularSnap)
    {
        if (!target || !target.HasComponent<DecalComponent>())
        {
            Cancel();
            return false;
        }
        if (IsUsing() && (m_Target != target.GetUuid() || m_Scene.get() != target.GetScene() || !editable))
            Cancel();
        const glm::mat4 world = target.GetWorldMatrix();
        auto& s = static_cast<DecalSettings&>(target.GetComponent<DecalComponent>());
        if (!DecalMath::IsValid(s, world))
        {
            Cancel();
            return false;
        }
        const auto project = [&](glm::vec3 p) -> std::optional<glm::vec2> {
            glm::vec4 clip = vp * world * glm::vec4(p + s.Offset, 1);
            if (clip.w <= 0.0001f || clip.z < -clip.w)
                return {};
            glm::vec2 ndc = glm::vec2(clip) / clip.w;
            return glm::vec2(viewport.x + (ndc.x * 0.5f + 0.5f) * (viewport.z - viewport.x),
                             viewport.y + (0.5f - ndc.y * 0.5f) * (viewport.w - viewport.y));
        };
        const auto projectedDirection = [&](const Handle& handle) {
            const glm::vec4 clip = vp * world * glm::vec4(handle.Position + s.Offset, 1);
            if (clip.w <= 0.0001f)
                return glm::vec2(0);
            const glm::vec4 axis = vp * world * glm::vec4(handle.Direction, 0);
            const glm::vec2 derivative = (glm::vec2(axis) - glm::vec2(clip) * (axis.w / clip.w)) / clip.w;
            return derivative * glm::vec2((viewport.z - viewport.x) * 0.5f, (viewport.y - viewport.w) * 0.5f);
        };
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(ImVec2(viewport.x, viewport.y), ImVec2(viewport.z, viewport.w), true);
        const ImU32 color = IM_COL32(80, 210, 235, 220);
        const auto line = [&](glm::vec3 a, glm::vec3 b, ImU32 tint = IM_COL32(80, 210, 235, 220)) {
            auto x = project(a), y = project(b);
            if (x && y)
                draw->AddLine(ImVec2(x->x, x->y), ImVec2(y->x, y->y), tint, 1.4f);
        };
        Vector<Handle> handles;
        if (s.Projection == DecalProjection::Box)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                for (int corner = 0; corner < 4; ++corner)
                {
                    glm::vec3 a = s.Size * -0.5f;
                    a[(axis + 1) % 3] *= (corner & 1) ? -1.0f : 1.0f;
                    a[(axis + 2) % 3] *= (corner & 2) ? -1.0f : 1.0f;
                    glm::vec3 b = a;
                    b[axis] *= -1;
                    line(a, b);
                }
                for (int sign : { -1, 1 })
                {
                    glm::vec3 p(0), direction(0);
                    p[axis] = s.Size[axis] * 0.5f * sign;
                    direction[axis] = static_cast<float>(sign);
                    static const char* labels[] = { "Left face", "Right face", "Bottom face", "Top face", "Back face", "Front face" };
                    handles.push_back({ p, direction, labels[axis * 2 + (sign > 0 ? 1 : 0)] });
                }
            }
            line({ 0, 0, s.Size.z * 0.5f }, { 0, 0, -s.Size.z * 0.75f });
        }
        else
        {
            const float seam = glm::radians(s.SeamRotation), arc = glm::radians(s.Arc);
            // Show the material-bearing sleeve. End rings alone read as disconnected
            // discs, especially on a full wrap where both angular boundaries coincide.
            const ImDrawListFlags savedFlags = draw->Flags;
            draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            for (int segment = 0; segment < 64; ++segment)
            {
                const float a = seam + arc * segment / 64, b = seam + arc * (segment + 1) / 64;
                const auto p0 = project(CylinderPoint(s, a, 0)), p1 = project(CylinderPoint(s, b, 0));
                const auto p2 = project(CylinderPoint(s, b, 1)), p3 = project(CylinderPoint(s, a, 1));
                if (p0 && p1 && p2 && p3)
                    draw->AddQuadFilled(ImVec2(p0->x, p0->y), ImVec2(p1->x, p1->y), ImVec2(p2->x, p2->y), ImVec2(p3->x, p3->y),
                                        IM_COL32(70, 195, 220, 22));
                for (float end : { 0.0f, 1.0f })
                {
                    line(CylinderPoint(s, a, end), CylinderPoint(s, b, end));
                    for (float shell : { -0.5f, 0.5f })
                        line(CylinderPoint(s, a, end, s.ShellThickness * shell), CylinderPoint(s, b, end, s.ShellThickness * shell),
                             IM_COL32(80, 210, 235, 65));
                }
                if (segment % 8 == 0)
                    line(CylinderPoint(s, a, 0), CylinderPoint(s, a, 1), IM_COL32(80, 210, 235, 100));
            }
            draw->Flags = savedFlags;
            line(CylinderPoint(s, seam, 0), CylinderPoint(s, seam, 1), IM_COL32(255, 185, 65, 240));
            if (s.Arc < 359.99f)
                line(CylinderPoint(s, seam + arc, 0), CylinderPoint(s, seam + arc, 1));
            const auto cylinderHandles = CylinderHandles(s);
            handles.assign(cylinderHandles.begin(), cylinderHandles.end());
        }
        ImVec2 mouse = ImGui::GetMousePos();
        glm::vec2 cursor(mouse.x, mouse.y);
        int hovered = -1;
        float nearest = 10.0f;
        const bool inside = cursor.x >= viewport.x && cursor.y >= viewport.y && cursor.x < viewport.z && cursor.y < viewport.w;
        if (editable && inside)
            for (int i = 0; i < static_cast<int>(handles.size()); ++i)
                if (const auto p = project(handles[i].Position))
                {
                    const float distance = glm::length(cursor - *p);
                    if (distance < nearest && glm::length(projectedDirection(handles[i])) > 0.003f)
                    {
                        hovered = i;
                        nearest = distance;
                    }
                }
        if (editable)
            for (int i = 0; i < static_cast<int>(handles.size()); ++i)
            {
                const auto& handle = handles[i];
                auto p = project(handle.Position);
                if (!p)
                    continue;
                const glm::vec2 direction = projectedDirection(handle);
                const bool hit = hovered == i;
                const float radius = hit || m_Handle == i ? 6.0f : 4.5f;
                const bool angular = s.Projection == DecalProjection::Cylinder && i >= 5;
                const ImU32 tint = glm::length(direction) <= 0.003f ? IM_COL32(100, 115, 120, 120) : angular ? IM_COL32(255, 185, 65, 255) : color;
                if (angular)
                    draw->AddQuadFilled(ImVec2(p->x, p->y - radius), ImVec2(p->x + radius, p->y), ImVec2(p->x, p->y + radius),
                                        ImVec2(p->x - radius, p->y), tint);
                else
                    draw->AddRectFilled(ImVec2(p->x - radius, p->y - radius), ImVec2(p->x + radius, p->y + radius), tint, 1.5f);
                if (hit || m_Handle == i)
                    ImGui::SetTooltip("%s", handle.Label);
                if (hit && !IsUsing() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (glm::dot(direction, direction) < 0.00001f)
                        continue;
                    Begin(target, i);
                    m_MouseStart = cursor;
                    m_Direction = direction;
                }
            }
        if (IsUsing())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                Finish(true);
            else
            {
                // The last cursor movement may arrive with the release event.
                // Apply it before committing, even if no held frame saw it.
                float delta = glm::dot(cursor - m_MouseStart, m_Direction) / glm::dot(m_Direction, m_Direction);
                Update(delta, ImGui::GetIO().KeyShift, m_Before.Projection == DecalProjection::Cylinder && m_Handle >= 5 ? angularSnap : snap);
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    Finish(false);
            }
        }
        draw->PopClipRect();
        return hovered >= 0 || IsUsing();
    }
} // namespace Crowny
