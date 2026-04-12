#include "cwepch.h"

#include "Editor/Editor.h"
#include "Editor/EditorLayer.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Input/Input.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Panels/HierarchyPanel.h"
#include "Panels/ViewportPanel.h"
#include "UI/UIUtils.h"

#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>

namespace Crowny
{

    static ImGuizmo::OPERATION GetImGuizmoMode(GizmoEditMode gizmoMode)
    {
        switch (gizmoMode)
        {
        case GizmoEditMode::Translate:
            return ImGuizmo::TRANSLATE;
        case GizmoEditMode::Rotate:
            return ImGuizmo::ROTATE;
        case GizmoEditMode::Scale:
            return ImGuizmo::SCALE;
        case GizmoEditMode::Bounds:
            return ImGuizmo::BOUNDS;
        }
        return ImGuizmo::TRANSLATE;
    }

    ViewportPanel::ViewportPanel(const String& name) : ImGuiPanel(name), m_ViewportBounds(0.0f)
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        m_GizmoMode = projSettings->GizmoMode;
        m_LocalMode = projSettings->GizmoLocalMode;
    }

    void ViewportPanel::Render()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        BeginPanel();
        gApplication->GetImGuiLayer()->BlockEvents(!m_Hovered);

        if (GImGui->ActiveId == 0)
        {
            if (!Input::IsMouseButtonPressed(Mouse::ButtonRight)) // && m_CurrentScene != m_RuntimeScene)
            {
                if (Input::IsKeyPressed(Key::Q))
                    m_GizmoMode = GizmoEditMode::None;
                if (Input::IsKeyPressed(Key::W))
                    m_GizmoMode = GizmoEditMode::Translate;
                if (Input::IsKeyPressed(Key::E))
                    m_GizmoMode = GizmoEditMode::Rotate;
                if (Input::IsKeyPressed(Key::R))
                    m_GizmoMode = GizmoEditMode::Scale;
                if (Input::IsKeyPressed(Key::T))
                    m_GizmoMode = GizmoEditMode::Bounds;
                if (Input::IsKeyDown(Key::X))
                    m_LocalMode = !m_LocalMode;
                if (Input::IsKeyDown(Key::F))
                {
                    const Entity selectedEntity = HierarchyPanel::GetSelectedEntity();
                    EditorLayer::GetEditorCamera().Focus(selectedEntity.GetWorldPosition());
                }
            }
        }

        const ImVec2 minBound = ImGui::GetWindowPos();
        const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        const ImVec2 viewportOffset = ImGui::GetCursorPos();

        m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
        RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
        Ref<Texture> texture = rt->GetColorTexture(0);

        ImTextureID textureID = ImGui_ImplVulkan_AddTexture(texture);
        ImGui::Image(textureID, ImVec2(m_ViewportSize.x, m_ViewportSize.y), ImVec2{ 0, 1 }, ImVec2{ 1, 0 }); // The viewport itself

        if (ImGui::BeginDragDropTarget())
        {
            auto validationCallback = [](const FileEntry* fileEntry) {
                if (fileEntry->Metadata == nullptr)
                    return false;
                const AssetType assetType = fileEntry->Metadata->Type;
                return assetType == AssetType::Scene || assetType == AssetType::Material || assetType == AssetType::Mesh ||
                       assetType == AssetType::AudioClip || assetType == AssetType::Prefab;
            };

            if (const FileEntry* fileEntry = UIUtils::AcceptAssetPayload(validationCallback))
            {
                const glm::vec4& bounds = GetViewportBounds();
                ImVec2 mouseCoords = ImGui::GetMousePos();
                glm::vec2 coords = { mouseCoords.x - bounds.x, mouseCoords.y - bounds.y };
                coords.y = m_ViewportSize.y - coords.y - 1;

                ImGuiViewportSceneDraggedEvent fileDragEvent(fileEntry, coords);
                OnEvent(fileDragEvent);
            }
            ImGui::EndDragDropTarget();
        }

        glm::vec2 bounds[2];
        ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
        ImVec2 viewportMaxRegion = ImGui::GetWindowContentRegionMax();
        ImVec2 viewportOffset2 = ImGui::GetWindowPos();
        bounds[0] = { viewportMinRegion.x + viewportOffset2.x, viewportMinRegion.y + viewportOffset2.y };
        bounds[1] = { viewportMaxRegion.x + viewportOffset2.x, viewportMaxRegion.y + viewportOffset2.y };
        m_ViewportBounds.x = bounds[0].x;
        m_ViewportBounds.y = bounds[0].y;
        m_ViewportBounds.z = bounds[1].x;
        m_ViewportBounds.w = bounds[1].y;

        Entity selected = HierarchyPanel::GetSelectedEntity();

        EditorCamera& camera = EditorLayer::GetEditorCamera();
        const glm::mat4& proj = camera.GetProjection();
        glm::mat4 view = camera.GetViewMatrix();
        ImGuizmo::SetRect(bounds[0].x, bounds[0].y, bounds[1].x - bounds[0].x, bounds[1].y - bounds[0].y);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        if (selected && m_GizmoMode != GizmoEditMode::None)
        {

            const float width = (float)ImGui::GetWindowWidth();
            const float height = (float)ImGui::GetWindowHeight();
            TransformComponent& tc = selected.GetComponent<TransformComponent>();
            glm::mat4 transform = selected.GetWorldMatrix();

            bool snap = Input::IsKeyPressed(Key::LeftControl);
            float snapValue = 0.1f; // TODO: These snaps should be loaded from the editor settings
            if (m_GizmoMode == GizmoEditMode::Rotate)
                snapValue = 15.0f;
            ImGuizmo::AllowAxisFlip(false);
            float snapValues[3] = { snapValue, snapValue, snapValue };
            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), GetImGuizmoMode(m_GizmoMode),
                                     (!m_LocalMode && m_GizmoMode == GizmoEditMode::Translate) ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
                                     glm::value_ptr(transform), nullptr,
                                     snap ? snapValues : nullptr)) // TODO: Bounds, does rotation work?
            {
                glm::vec3 position, scale;
                glm::quat rotation;
                if (Math::DecomposeMatrix(transform, position, rotation, scale))
                {
                    glm::vec3 rotationEuler = glm::eulerAngles(rotation);
                    glm::vec3 transformRotation = glm::eulerAngles(selected.GetWorldRotation());
                    const glm::vec3 deltaRot = rotationEuler - transformRotation;
                    selected.SetWorldPosition(position);
                    selected.SetWorldRotation(transformRotation + deltaRot);
                    selected.SetWorldScale(scale);
                }
            }
        }
        if (ImGuizmo::ViewManipulate(glm::value_ptr(view), camera.GetDistance(), { m_ViewportBounds.z - 136.0f, m_ViewportBounds.y },
                                     ImVec2(128, 128), 0x10101010))
        {
            // ViewManipulate modifies the view matrix in-place. Invert it to get the
            // camera transform (world-space position + orientation) before decomposing.
            glm::mat4 cameraTransform = glm::inverse(view);
            glm::vec3 t, s;
            glm::quat orientation;
            Math::DecomposeMatrix(cameraTransform, t, orientation, s);
            // The camera builds its orientation as quat(vec3(-pitch, -yaw, roll)),
            // so recover pitch/yaw/roll from the extracted Euler angles.
            glm::vec3 euler = glm::eulerAngles(orientation);
            camera.SetPitch(-euler.x);
            camera.SetYaw(-euler.y);
            camera.SetRoll(euler.z);
        }

        EndPanel();
        ImGui::PopStyleVar();
    }

    void ViewportPanel::SetEventCallback(const EventCallbackFn& onEvent) { OnEvent = onEvent; }

    void ViewportPanel::SetEditorRenderTarget(const Ref<RenderTarget>& rt) { m_RenderTarget = rt; }

} // namespace Crowny
