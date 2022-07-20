#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/RenderAPI/Texture.h"

#include "Crowny/Scripting/Mono/MonoManager.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorDefaults.h"
#include "UI/Properties.h"
#include "UI/ScriptInspector.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/fwd.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Crowny
{

    static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f,
                                float columnWidth = 100.0f)
    {
        bool modified = false;

        UI::PushID();
        ImGui::TableSetColumnIndex(0);
        UI::ShiftCursor(17.0f, 7.0f);

        ImGui::Text(label.c_str());
        UI::Underline(false, 0.0f, 2.0f);

        ImGui::TableSetColumnIndex(1);
        UI::ShiftCursor(7.0f, 0.0f);

        {
            const float spacingX = 8.0f;
            UI::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2{ spacingX, 0.0f });
            UI::ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 2.0f });

            {
                UI::ScopedColor padding(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
                UI::ScopedStyle frame(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

                ImGui::BeginChild(
                  ImGui::GetID((label + "fr").c_str()),
                  ImVec2(ImGui::GetContentRegionAvail().x - spacingX, ImGui::GetFrameHeightWithSpacing() + 8.0f),
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            }
            const float framePadding = 2.0f;
            const float outlineSpacing = 1.0f;
            const float lineHeight = GImGui->Font->FontSize + framePadding * 2.0f;
            const ImVec2 buttonSize = { lineHeight + 2.0f, lineHeight };
            const float inputItemWidth = (ImGui::GetContentRegionAvail().x - spacingX) / 3.0f - buttonSize.x;

            const ImGuiIO& io = ImGui::GetIO();
            auto boldFont = io.Fonts->Fonts[1];

            auto drawControl = [&](const std::string& label, float& value, const ImVec4& colourN, const ImVec4& colorH,
                                   const ImVec4& colorP) {
                {
                    UI::ScopedStyle buttonFrame(ImGuiStyleVar_FramePadding, ImVec2(framePadding, 0.0f));
                    UI::ScopedStyle buttonRounding(ImGuiStyleVar_FrameRounding, 1.0f);
                    UI::ScopedColor buttonColor(ImGuiCol_Button, colourN);
                    UI::ScopedColor buttonHover(ImGuiCol_ButtonHovered, colorH);
                    UI::ScopedColor buttonActive(ImGuiCol_ButtonActive, colorP);

                    ImGui::PushFont(boldFont);

                    UI::ShiftCursorY(2.0f);
                    if (ImGui::Button(label.c_str(), buttonSize))
                    {
                        value = resetValue;
                        modified = true;
                    }
                    ImGui::PopFont();
                }

                ImGui::SameLine(0.0f, outlineSpacing);
                ImGui::SetNextItemWidth(inputItemWidth);
                UI::ShiftCursorY(-2.0f);
                // ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, renderMultiSelect);
                bool wasTempInputActive = ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str()));
                modified |= UI::DragFloat(("##" + label).c_str(), &value, 0.1f, 0.0f, 0.0f, "%.2f", 0);

                // NOTE(Peter): Ugly hack to make tabbing behave the same as Enter (e.g marking it as manually modified)
                /*if (modified && Input::IsKeyPressed(KeyCode::Tab))
                    manuallyEdited = true;

                if (ImGui::TempInputIsActive(ImGui::GetID(("##" + label).c_str())))
                    modified = false;*/

                // ImGui::PopItemFlag();

                if (!UI::IsItemDisabled())
                    UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

                /*if (wasTempInputActive)
                    manuallyEdited |= ImGui::IsItemDeactivatedAfterEdit();*/
            };

            drawControl("X", values.x, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f },
                        ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });

            ImGui::SameLine(0.0f, outlineSpacing);
            drawControl("Y", values.y, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f },
                        ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });

            ImGui::SameLine(0.0f, outlineSpacing);
            drawControl("Z", values.z, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f },
                        ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });

            ImGui::EndChild();
        }
        UI::PopID();

        return modified;
    }

    template <> void ComponentEditorWidget<TransformComponent>(Entity e)
    {
        auto& transform = e.GetComponent<TransformComponent>();
        bool changed = false;
        ImGui::Columns(1);
        UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
        UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));

        ImGui::BeginTable("transformComponent", 2,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoClip);
        ImGui::TableSetupColumn("label_column", 0, 100.0f);
        ImGui::TableSetupColumn("value_column", ImGuiTableColumnFlags_IndentEnable | ImGuiTableColumnFlags_NoClip,
                                ImGui::GetContentRegionAvail().x - 100.0f);

        ImGui::TableNextRow();
        DrawVec3Control("Transform", transform.Position);

        glm::vec3 deg = glm::degrees(transform.Rotation);
        ImGui::TableNextRow();
        DrawVec3Control("Rotation", deg);
        transform.Rotation = glm::radians(deg);

        ImGui::TableNextRow();
        DrawVec3Control("Scale", transform.Scale, 1.0f);

        ImGui::EndTable();
    }

    template <> void ComponentEditorWidget<CameraComponent>(Entity e)
    {
        auto& camera = e.GetComponent<CameraComponent>().Camera;
        glm::vec3 color = camera.GetBackgroundColor();
        if (UI::PropertyColor("Background", color))
            camera.SetBackgroundColor(color);

        SceneCamera::CameraProjection projection = camera.GetProjectionType();
        if (UI::PropertyDropdown("Projection", { "Orthographic", "Perspective" }, projection))
            camera.SetProjectionType(projection);

        if (camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
        {
            ImGui::Text("Field of View");
            ImGui::NextColumn();
            float fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
            if (ImGui::SliderFloat("##fov", &fov, 0, 180, "%.3f"))
                camera.SetPerspectiveVerticalFOV(glm::radians(fov));

            ImGui::Columns(1);

            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                static float maxClippingPlane = 1000000.0f;
                static float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                float nearPlane = camera.GetPerspectiveNearClip();
                if (UI::Property("Near", nearPlane, minClippingPlane, maxClippingPlane))
                    camera.SetPerspectiveNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));

                float farPlane = camera.GetPerspectiveFarClip();
                ImGui::Text("Far");
                ImGui::NextColumn();
                if (UI::Property("Far", farPlane, minClippingPlane, maxClippingPlane))
                    camera.SetPerspectiveFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
                ImGui::Unindent(30.f);
            }
        }

        else if (camera.GetProjectionType() == SceneCamera::CameraProjection::Orthographic)
        {
            ImGui::Text("Size");
            ImGui::NextColumn();
            float size = camera.GetOrthographicSize();
            if (ImGui::SliderFloat("##fov", &size, 0.0f, 180.0f, "%.3f"))
                camera.SetOrthographicSize(size);

            ImGui::Columns(1);
            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                const float maxClippingPlane = 1000000.0f;
                const float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                float nearPlane = camera.GetOrthographicNearClip();
                if (UI::Property("Near", nearPlane, minClippingPlane, maxClippingPlane))
                    camera.SetOrthographicNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));

                float farPlane = camera.GetOrthographicFarClip();
                if (UI::Property("Far", farPlane, minClippingPlane, maxClippingPlane))
                    camera.SetOrthographicFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
                ImGui::Unindent(30.f);
            }
        }
        ImGui::Columns(2);

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        glm::vec4 viewport = camera.GetViewportRect();
        if (UI::Property("Viewport", viewport, 0.01f, 0.0f, 1.0f))
            camera.SetViewportRect(viewport);

        bool occlusion = camera.GetOcclusionCulling();
        if (UI::Property("Occlusion Culling", occlusion))
            camera.SetOcclusionCulling(occlusion);

        bool hdr = camera.GetHDR();
        if (UI::Property("HDR", hdr))
            camera.SetHDR(hdr);

        bool msaa = camera.GetMSAA();
        if (UI::Property("MSAA", msaa))
            camera.SetMSAA(msaa);
    }

    template <> void ComponentEditorWidget<TextComponent>(Entity e)
    {
        auto& textComponent = e.GetComponent<TextComponent>();

        UI::Property("Text", textComponent.Text);
        UI::PropertyColor("Color", textComponent.Color);

        String str = textComponent.Font->GetName();
        UI::Property("Font", str);
        ImGui::Text("Font"); // Hook up drag drop here.
#ifdef CW_DEBUG
        ImGui::SameLine();
        if (ImGui::Button("Show Font Atlas"))
            ImGui::OpenPopup(textComponent.Font->GetName().c_str());

        if (ImGui::BeginPopup(textComponent.Font->GetName().c_str()))
        {
            ImGui::Text("%s", textComponent.Font->GetName().c_str());
            ImGui::Separator();
            ImGui::Image(ImGui_ImplVulkan_AddTexture(textComponent.Font->GetTexture()),
                         { (float)textComponent.Font->GetTexture()->GetWidth(),
                           (float)textComponent.Font->GetTexture()->GetHeight() });
            ImGui::EndPopup();
        }
#endif
        ImGui::NextColumn();

        float size = textComponent.Font->GetSize();
        if (UI::Property("Font Size", size))
            textComponent.Font = FontManager::Get(textComponent.Font->GetName(), size);
    }

    template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e)
    {
        auto& t = e.GetComponent<SpriteRendererComponent>();

        if (t.Texture)
            ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Texture), { 50.0f, 50.0f }, { 0, 1 }, { 1, 0 });
        else
            ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), { 50.0f, 50.0f }, { 0, 1 },
                         { 1, 0 });
        if (ImGui::IsItemClicked())
        {
            Vector<Path> outPaths;
            if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", {}, outPaths) && outPaths.size() >= 1)
                t.Texture = Importer::Get().Import<Texture>(outPaths[0]);
        }

        UI::PropertyColor("Sprite Color", t.Color);
    }

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e)
    {
        auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;
        ImGui::Text("Path");
    }

    template <> void ComponentEditorWidget<Rigidbody2DComponent>(Entity entity)
    {
        Rigidbody2DComponent& rb2d = entity.GetComponent<Rigidbody2DComponent>();

        uint32_t layerMask = rb2d.GetLayerMask();
        if (UIUtils::PropertyLayer("Layer", layerMask))
            rb2d.SetLayerMask(layerMask, entity);

        RigidbodyBodyType bodyType = rb2d.GetBodyType();
        if (UI::PropertyDropdown("Body Type", { "Static", "Dynamic", "Kinematic" }, bodyType))
            rb2d.SetBodyType(bodyType);

        if (bodyType == RigidbodyBodyType::Dynamic)
        {
            bool autoMass = rb2d.GetAutoMass();
            if (UI::Property("Auto Mass", autoMass))
                rb2d.SetAutoMass(autoMass, entity);

            ImGui::BeginDisabled(autoMass);
            float mass = autoMass ? Physics2D::Get().CalculateMass(entity) : rb2d.GetMass();
            if (UI::Property("Mass", mass))
                rb2d.SetMass(mass);
            ImGui::EndDisabled();

            float gravityScale = rb2d.GetGravityScale();
            if (UI::Property("Gravity Scale", gravityScale))
                rb2d.SetGravityScale(gravityScale);

            float linearDrag = rb2d.GetLinearDrag();
            if (UI::Property("Linear Drag", linearDrag))
                rb2d.SetLinearDrag(linearDrag);

            float angularDrag = rb2d.GetAngularDrag();
            if (UI::Property("Angular Drag", angularDrag))
                rb2d.SetAngularDrag(angularDrag);
        }

        if (bodyType != RigidbodyBodyType::Static)
        {
            CollisionDetectionMode2D continuous = rb2d.GetCollisionDetectionMode();
            if (UI::PropertyDropdown("Collision Detection", { "Discrete", "Continuous" }, continuous))
                rb2d.SetCollisionDetectionMode(continuous);

            RigidbodySleepMode sleepMode = rb2d.GetSleepMode();
            if (UI::PropertyDropdown("Sleeping Mode", { "NeverSleep", "StartAwake", "StartSleeping" }, sleepMode))
                rb2d.SetSleepMode(sleepMode);

            ImGui::Columns(1);
            if (ImGui::CollapsingHeader("Constraints"))
            {
                ImGui::Indent(30.f);
                ImGui::Columns(2);
                ImGui::Text("Fixed Position");
                ImGui::NextColumn();

                ImGui::Text("X");
                ImGui::SameLine();
                Rigidbody2DConstraints constraints = rb2d.GetConstraints();
                bool freezeX = constraints.IsSet(Rigidbody2DConstraintsBits::FreezePositionX);
                if (ImGui::Checkbox("##rb2dxPosLock", &freezeX))
                {
                    if (freezeX)
                        constraints.Set(Rigidbody2DConstraintsBits::FreezePositionX);
                    else
                        constraints.Unset(Rigidbody2DConstraintsBits::FreezePositionX);
                    rb2d.SetConstraints(constraints);
                }

                ImGui::SameLine();
                ImGui::Text("Y");
                ImGui::SameLine();
                bool freezeY = rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezePositionY);
                if (ImGui::Checkbox("##rb2dyPosLock", &freezeY))
                {
                    if (freezeY)
                        constraints.Set(Rigidbody2DConstraintsBits::FreezePositionY);
                    else
                        constraints.Unset(Rigidbody2DConstraintsBits::FreezePositionY);
                    rb2d.SetConstraints(constraints);
                }
                ImGui::NextColumn();
                ImGui::Text("Fixed Rotation");
                ImGui::NextColumn();
                ImGui::Text("Z");
                ImGui::SameLine();
                bool freezeRotation = rb2d.GetConstraints().IsSet(Rigidbody2DConstraintsBits::FreezeRotation);
                if (ImGui::Checkbox("##rb2dyRotLock", &freezeRotation))
                {
                    if (freezeRotation)
                        constraints.Set(Rigidbody2DConstraintsBits::FreezeRotation);
                    else
                        constraints.Unset(Rigidbody2DConstraintsBits::FreezeRotation);
                    rb2d.SetConstraints(constraints);
                }

                ImGui::Unindent(30.0f);
            }
        }
    }

    static void DrawPhysicsMaterial(const AssetHandle<PhysicsMaterial2D>& material)
    {
        UI::Property("Density", material->GetDensity());
        UI::Property("Friction", material->GetFriction());
        UI::Property("Restitution", material->GetRestitution());
        UI::Property("Restitution Threshold", material->GetRestitutionThreshold());
    }

    template <> void ComponentEditorWidget<BoxCollider2DComponent>(Entity entity)
    {
        BoxCollider2DComponent& bc2d = entity.GetComponent<BoxCollider2DComponent>();

        glm::vec2 offset = bc2d.GetOffset();
        if (UI::Property("Offset", offset, 0.05f))
            bc2d.SetOffset(offset, entity);

        glm::vec2 size = bc2d.GetSize();
        if (UI::Property("Size", size, 0.05f, 0.0f, 0.0f) && size.x > 0.0f && size.y > 0.0f)
            bc2d.SetSize(size, entity);

        bool isTrigger = bc2d.IsTrigger();
        if (UI::Property("Is Trigger", isTrigger))
            bc2d.SetIsTrigger(isTrigger);
        DrawPhysicsMaterial(bc2d.GetMaterial());
    }

    template <> void ComponentEditorWidget<CircleCollider2DComponent>(Entity entity)
    {
        CircleCollider2DComponent& cc2d = entity.GetComponent<CircleCollider2DComponent>();

        glm::vec2 offset = cc2d.GetOffset();
        if (UI::Property("Offset", offset, 0.05f))
            cc2d.SetOffset(offset, entity);

        float radius = cc2d.GetRadius();
        if (UI::Property("Radius", radius, 0.05f, 0.0f, 0.0f) && radius > 0.0f)
            cc2d.SetRadius(radius, entity);

        bool isTrigger = cc2d.IsTrigger();
        if (UI::Property("Is Trigger", isTrigger))
            cc2d.SetIsTrigger(isTrigger);
        DrawPhysicsMaterial(cc2d.GetMaterial());
    }

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity entity) {}

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity entity)
    {
        AudioSourceComponent& sourceComponent = entity.GetComponent<AudioSourceComponent>();

        AssetHandle<AudioClip> handle = sourceComponent.GetClip();
        if (UIUtils::AssetReference<AudioClip>("Audio Clip", handle) && handle)
            sourceComponent.SetClip(handle);

        float volume = sourceComponent.GetVolume();
        if (UI::PropertySlider("Volume", volume, 0.0f, 1.0f))
            sourceComponent.SetVolume(volume);

        bool mute = sourceComponent.GetIsMuted();
        if (UI::Property("Mute", mute))
            sourceComponent.SetIsMuted(mute);

        float pitch = sourceComponent.GetPitch();
        if (UI::PropertySlider("Pitch", pitch, -3.0f, 3.0f))
            sourceComponent.SetPitch(pitch);

        bool playOnAwake = sourceComponent.GetPlayOnAwake();
        if (UI::Property("Play On Awake", playOnAwake))
            sourceComponent.SetPlayOnAwake(playOnAwake);

        bool looping = sourceComponent.GetLooping();
        if (UI::Property("Loop", looping))
            sourceComponent.SetLooping(looping);

        float minDistance = sourceComponent.GetMinDistance();
        if (UI::Property("Min Distance", minDistance))
            sourceComponent.SetMinDistance(minDistance);

        float maxDistance = sourceComponent.GetMaxDistance();
        if (UI::Property("Max Distance", minDistance))
            sourceComponent.SetMaxDistance(maxDistance);
    }

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e)
    {
        MonoScriptComponent& scriptComponent = e.GetComponent<MonoScriptComponent>();
        for (uint32_t i = 0; i < scriptComponent.Scripts.size(); i++)
        {
            auto& script = scriptComponent.Scripts[i];
            ImGui::PushID(i);
            if (ImGui::Button("-"))
            {
                scriptComponent.Scripts.erase(scriptComponent.Scripts.begin() + i);
                ImGui::PopID();
                continue;
            }
            ImGui::SameLine();
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(script.GetTypeName().c_str()))
            {
                ImGui::Indent(30.f);
                ImGui::PushID("Widget");
                UI::BeginPropertyGrid();

                String typeName = script.GetTypeName();
                if (UIUtils::PropertyScript("Class name", typeName))
                {
                    script.SetClassName(typeName);
                    // FIXME script.OnInitialize(e);
                }

                if (script.GetManagedClass() == nullptr)
                {
                    UI::EndPropertyGrid();
                    ImGui::PopID();
                    ImGui::Unindent(30.f);
                    ImGui::PopID();
                    continue;
                }

                Ref<SerializableObjectInfo> objectInfo = script.GetObjectInfo();
                MonoObject* instance = script.GetManagedInstance();
                ScriptInspector::DrawObjectInspector(objectInfo, instance, nullptr);

                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(30.f);
            }
            ImGui::PopID();
        }
    }

} // namespace Crowny
