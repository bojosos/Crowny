#include "cwepch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorDefaults.h"
#include "UI/Properties.h"

#include "UI/ScriptInspector.h"
#include "Crowny/Scripting/Mono/MonoManager.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "glm/fwd.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace Crowny
{

    template <> void ComponentEditorWidget<TransformComponent>(Entity e)
    {
        auto& t = e.GetComponent<TransformComponent>();
        bool changed = false;

        ImGui::Columns(2);
        ImGui::Text("Transform");
        ImGui::NextColumn();
        ImGui::DragFloat3("##TransformTransform", glm::value_ptr(t.Position), DRAG_SENSITIVITY);
        ImGui::NextColumn();

        ImGui::Text("Rotation");
        ImGui::NextColumn();
        glm::vec3 deg = glm::degrees(t.Rotation);
        ImGui::DragFloat3("##TransformRotation", glm::value_ptr(deg), DRAG_SENSITIVITY);
        ImGui::NextColumn();
        t.Rotation = glm::radians(deg);

        ImGui::Text("Scale");
        ImGui::NextColumn();
        ImGui::DragFloat3("##TransformScale", glm::value_ptr(t.Scale), DRAG_SENSITIVITY);
        ImGui::NextColumn();
        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<CameraComponent>(Entity e)
    {
        auto& cam = e.GetComponent<CameraComponent>().Camera;
        glm::vec3 tmp = cam.GetBackgroundColor();
        if (ImGui::ColorEdit3("Background", glm::value_ptr(tmp)))
            cam.SetBackgroundColor(tmp);

        ImGui::Columns(2);
        ImGui::Text("Projection");
        ImGui::NextColumn();
        const char* projections[2] = { "Orthographic", "Perspective" };
        if (ImGui::BeginCombo("##Projection", projections[(uint32_t)cam.GetProjectionType()]))
        {
            for (uint32_t i = 0; i < 2; i++)
            {
                const bool is_selected = ((uint32_t)cam.GetProjectionType() == i);
                if (ImGui::Selectable(projections[i], is_selected))
                {
                    cam.SetProjectionType((SceneCamera::CameraProjection)i);
                }

                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        if (cam.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
        {
            ImGui::Text("Filed of View");
            ImGui::NextColumn();
            float fov = glm::degrees(cam.GetPerspectiveVerticalFOV());
            if (ImGui::SliderFloat("##fov", &fov, 0, 180, "%.3f"))
                cam.SetPerspectiveVerticalFOV(glm::radians(fov));

            ImGui::Columns(1);
            // ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                static float maxClippingPlane = 1000000.0f;
                static float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                ImGui::Text("Near");
                ImGui::NextColumn();
                float nearPlane = cam.GetPerspectiveNearClip();
                if (ImGui::DragScalar("##near", ImGuiDataType_Float, &nearPlane, 0.1f, &minClippingPlane,
                                      &maxClippingPlane, "%.2f"))
                    cam.SetPerspectiveNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));
                ImGui::NextColumn();

                float farPlane = cam.GetPerspectiveFarClip();
                ImGui::Text("Far");
                ImGui::NextColumn();
                if (ImGui::DragScalar("##far", ImGuiDataType_Float, &farPlane, 0.1f, &minClippingPlane,
                                      &maxClippingPlane, "%.2f"))
                    cam.SetPerspectiveFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
                ImGui::Unindent(30.f);
                ImGui::NextColumn();
            }
        }

        else if (cam.GetProjectionType() == SceneCamera::CameraProjection::Orthographic)
        {
            ImGui::Text("Size");
            ImGui::NextColumn();
            float size = cam.GetOrthographicSize();
            if (ImGui::SliderFloat("##fov", &size, 0.0f, 180.0f, "%.3f"))
                cam.SetOrthographicSize(size);

            ImGui::Columns(1);
            // ImGui::SetNextItemOpen(true, ImGuiCond_Once); TODO: maybe replace with tree node? collapsing header too
            // thick

            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                static float maxClippingPlane = 1000000.0f;
                static float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                ImGui::Text("Near");
                ImGui::NextColumn();
                float nearPlane = cam.GetOrthographicNearClip();
                if (ImGui::DragScalar("##near", ImGuiDataType_Float, &nearPlane, 0.1f, &minClippingPlane,
                                      &maxClippingPlane, "%.2f"))
                    cam.SetOrthographicNearClip(std::clamp(nearPlane, minClippingPlane, maxClippingPlane));
                ImGui::NextColumn();

                float farPlane = cam.GetOrthographicFarClip();
                ImGui::Text("Far");
                ImGui::NextColumn();
                if (ImGui::DragScalar("##far", ImGuiDataType_Float, &farPlane, 0.1f, &minClippingPlane,
                                      &maxClippingPlane, "%.2f"))
                    cam.SetOrthographicFarClip(std::clamp(farPlane, minClippingPlane, maxClippingPlane));
                ImGui::Unindent(30.f);
                ImGui::NextColumn();
            }
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        ImGui::Columns(1);
        if (ImGui::CollapsingHeader("Viewport Rect"))
        {
            ImGui::Columns(2);
            ImGui::Indent(30.f);

            const float minViewport = 0.0f;
            const float maxViewport = 1.0f;

            ImGui::Text("X");
            ImGui::NextColumn();
            glm::vec4 tmp = cam.GetViewportRect();
            ImGui::DragScalar("##rectx", ImGuiDataType_Float, &tmp.x, 0.01f, &minViewport, &maxViewport, "%.2f");
            ImGui::NextColumn();

            ImGui::Text("Y");
            ImGui::NextColumn();
            ImGui::DragScalar("##recty", ImGuiDataType_Float, &tmp.y, 0.01f, &minViewport, &maxViewport, "%.2f");
            ImGui::NextColumn();

            ImGui::Text("Width");
            ImGui::NextColumn();
            ImGui::DragScalar("##rectw", ImGuiDataType_Float, &tmp.z, 0.01f, &minViewport, &maxViewport, "%.2f");
            ImGui::NextColumn();

            ImGui::Text("Height");
            ImGui::NextColumn();
            ImGui::DragScalar("##recth", ImGuiDataType_Float, &tmp.w, 0.01f, &minViewport, &maxViewport, "%.2f");

            tmp.x = std::clamp(tmp.x, minViewport, maxViewport);
            tmp.y = std::clamp(tmp.y, minViewport, maxViewport);
            tmp.z = std::clamp(tmp.z, minViewport, maxViewport);
            tmp.w = std::clamp(tmp.w, minViewport, maxViewport);
            cam.SetViewportRect(tmp);

            ImGui::Columns(1);
            ImGui::Unindent(30.f);
        }

        ImGui::Columns(2);
        ImGui::Text("Occlusion Culling");
        ImGui::NextColumn();
        bool occ = cam.GetOcclusionCulling();
        if (ImGui::Checkbox("##occcull", &occ))
            cam.SetOcclusionCulling(occ);
        ImGui::NextColumn();

        ImGui::Text("HDR");
        ImGui::NextColumn();
        bool hdr = cam.GetHDR();
        if (ImGui::Checkbox("##hdr", &hdr))
            cam.SetHDR(hdr);
        ImGui::NextColumn();

        ImGui::Text("MSAA");
        ImGui::NextColumn();
        bool msaa = cam.GetMSAA();
        if (ImGui::Checkbox("##msaa", &msaa))
            cam.SetMSAA(msaa);
        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<TextComponent>(Entity e)
    {
        auto& t = e.GetComponent<TextComponent>();

        ImGui::Columns(2);
        ImGui::Text("Text");
        ImGui::NextColumn();
        ImGui::InputText("##text", &t.Text);
        ImGui::NextColumn();

        ImGui::Text("Color");
        ImGui::NextColumn();
        ImGui::ColorEdit4("##textcolor", glm::value_ptr(t.Color));
        ImGui::NextColumn();

        ImGui::Text("Font"); // Hook up drag drop here.
        ImGui::NextColumn();
        ImGui::Text("%s", t.Font->GetName().c_str());
#ifdef CW_DEBUG
        ImGui::SameLine();
        if (ImGui::Button("Show Font Atlas"))
        {
            ImGui::OpenPopup(t.Font->GetName().c_str());
        }

        if (ImGui::BeginPopup(t.Font->GetName().c_str()))
        {
            ImGui::Text("%s", t.Font->GetName().c_str());
            ImGui::Separator();
            ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Font->GetTexture()),
                         { (float)t.Font->GetTexture()->GetWidth(), (float)t.Font->GetTexture()->GetHeight() });
            ImGui::EndPopup();
        }
#endif
        ImGui::NextColumn();

        ImGui::Text("Font Size");
        ImGui::NextColumn();

        float size = t.Font->GetSize();
        if (ImGui::InputFloat("##fontsize", &size))
            t.Font = FontManager::Get(t.Font->GetName(), size);
        ImGui::Columns(1);
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
            if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", {}, outPaths))
            {
                Ref<Texture> result;
                // LoadTexture(outPaths[0], result);
                t.Texture = result;
            }
        }

        ImGui::SameLine();
        ImGui::ColorEdit4("##SpriteColor", glm::value_ptr(t.Color));
    }

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e)
    {
        auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;

        ImGui::Text("Path");
    }

    template <> void ComponentEditorWidget<Rigidbody2DComponent>(Entity e)
    {
        Rigidbody2DComponent& rb2d = e.GetComponent<Rigidbody2DComponent>();

        ImGui::Columns(2);
        ImGui::Text("Body Type");
        ImGui::NextColumn();

        const char* bodyTypes[3] = { "Static", "Dynamic", "Kinematic" };
        if (ImGui::BeginCombo("##rb2dbodyType", bodyTypes[(uint32_t)rb2d.GetBodyType()]))
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                const bool isSelected = ((uint32_t)rb2d.GetBodyType() == i);
                if (ImGui::Selectable(bodyTypes[i], isSelected))
                    rb2d.SetBodyType((RigidbodyBodyType)i);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::NextColumn();
        ImGui::Text("Mass");
        ImGui::NextColumn();
        float mass = rb2d.GetMass();
        if (ImGui::DragFloat("##rb2dmass", &mass, DRAG_SENSITIVITY))
            rb2d.SetMass(mass);

        ImGui::NextColumn();
        ImGui::Text("Gravity Scale");
        ImGui::NextColumn();
        float gravityScale = rb2d.GetGravityScale();
        if (ImGui::DragFloat("##rb2gravityScale", &gravityScale, DRAG_SENSITIVITY))
            rb2d.SetGravityScale(gravityScale);

        ImGui::NextColumn();
        ImGui::Text("Collision Detection");
        ImGui::NextColumn();
        if (ImGui::BeginCombo("##rb2dcollisionDetectionMode",
                              rb2d.GetContinuousCollisionDetection() ? "Continuous" : "Discrete"))
        {
            if (ImGui::Selectable("Discrete", rb2d.GetContinuousCollisionDetection() == false))
                rb2d.SetContinuousCollisionDetection(false);

            if (ImGui::Selectable("Continuous", rb2d.GetContinuousCollisionDetection() == true))
                rb2d.SetContinuousCollisionDetection(true);

            if (rb2d.GetContinuousCollisionDetection())
                ImGui::SetItemDefaultFocus();
            ImGui::EndCombo();
        }

        ImGui::NextColumn();
        ImGui::Text("Sleeping Mode");
        ImGui::NextColumn();
        const char* sleepModes[3] = { "NeverSleep", "StartAwake", "StartSleeping" };
        if (ImGui::BeginCombo("##rb2dsleepMode", sleepModes[(uint32_t)rb2d.GetSleepMode()]))
        {
            for (uint32_t i = 0; i < 3; i++)
            {
                const bool isSelected = ((uint32_t)rb2d.GetSleepMode() == i);
                if (ImGui::Selectable(sleepModes[i], isSelected))
                    rb2d.SetSleepMode((RigidbodySleepMode)i);

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Columns(1);
        // ImGui::SetNextItemOpen(true, ImGuiCond_Once);

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

        ImGui::Columns(1);
    }

    static void DrawPhysicsMaterial(PhysicsMaterial2D& material)
    {
        ImGui::Columns(2);
        ImGui::Text("Density");
        ImGui::NextColumn();
        ImGui::DragFloat("##physicsMatDensity", &material.Density, DRAG_SENSITIVITY);
        ImGui::NextColumn();
        ImGui::Text("Friction");
        ImGui::NextColumn();
        ImGui::DragFloat("##physicsMatFriction", &material.Friction, DRAG_SENSITIVITY);
        ImGui::NextColumn();
        ImGui::Text("Restitution");
        ImGui::NextColumn();
        ImGui::DragFloat("##physicsMatRestition", &material.Restitution, DRAG_SENSITIVITY);
        ImGui::NextColumn();
        ImGui::Text("Restitution Threshold");
        ImGui::NextColumn();
        ImGui::DragFloat("##physicsMatRestitionThreshold", &material.RestitutionThreshold, DRAG_SENSITIVITY);
        ImGui::NextColumn();
    }

    template <> void ComponentEditorWidget<BoxCollider2DComponent>(Entity e)
    {
        BoxCollider2DComponent& bc2d = e.GetComponent<BoxCollider2DComponent>();

        ImGui::Columns(2);
        ImGui::Text("Offset");
        ImGui::NextColumn();
        ImGui::DragFloat2("##boxCollider2Doffset", glm::value_ptr(bc2d.Offset), DRAG_SENSITIVITY);
        ImGui::NextColumn();

        ImGui::Text("Size");
        ImGui::NextColumn();
        ImGui::DragFloat2("##boxCollider2Dsize", glm::value_ptr(bc2d.Size), DRAG_SENSITIVITY);
        ImGui::NextColumn();

        ImGui::Text("Is Trigger");
        ImGui::NextColumn();
        ImGui::Checkbox("##boxCollider2Dtrigger", &bc2d.IsTrigger);
        ImGui::NextColumn();
        DrawPhysicsMaterial(bc2d.Material);
        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<CircleCollider2DComponent>(Entity e)
    {
        auto& cc2d = e.GetComponent<CircleCollider2DComponent>();

        ImGui::Columns(2);
        ImGui::Text("Offset");
        ImGui::NextColumn();
        ImGui::DragFloat2("##circleCollider2Doffset", glm::value_ptr(cc2d.Offset), DRAG_SENSITIVITY);
        ImGui::NextColumn();

        ImGui::Text("Radius");
        ImGui::NextColumn();
        ImGui::DragFloat("##circleCollider2Dradius", &cc2d.Radius, DRAG_SENSITIVITY);
        ImGui::NextColumn();

        // ImGui::Text("Is Trigger"); ImGui::NextColumn();
        // ImGui::Checkbox("##circleCollider2Dtrigger", &cc2d.IsTrigger);
        // ImGui::NextColumn();
        DrawPhysicsMaterial(cc2d.Material);

        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e) {}

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e)
    {
        AudioSourceComponent& sourceComponent = e.GetComponent<AudioSourceComponent>();

        ImGui::Columns(2);

        ImGui::Text("Audio Clip");
        ImGui::NextColumn();
        if (sourceComponent.GetClip() != nullptr)
            ImGui::Text("%s", sourceComponent.GetClip()->GetName().c_str());
        else
            ImGui::Text("Audio clip goes here");
        ImGui::NextColumn();

        ImGui::Text("Volume");
        ImGui::NextColumn();
        float volume = sourceComponent.GetVolume();
        if (ImGui::SliderFloat("##volume", &volume, 0.0f, 1.0f, "%.2f"))
            sourceComponent.SetVolume(volume);
        ImGui::NextColumn();

        ImGui::Text("Mute");
        ImGui::NextColumn();
        bool mute = sourceComponent.GetIsMuted();
        if (ImGui::Checkbox("##mute", &mute))
            sourceComponent.SetIsMuted(mute);
        ImGui::NextColumn();

        ImGui::Text("Pitch");
        ImGui::NextColumn();
        float pitch = sourceComponent.GetPitch();
        if (ImGui::SliderFloat("##pitch", &pitch, -3.0f, 3.0f, "%.2f"))
            sourceComponent.SetPitch(pitch);
        ImGui::NextColumn();

        ImGui::Text("Play On Awake");
        ImGui::NextColumn();
        bool playOnAwake = sourceComponent.GetPlayOnAwake();
        if (ImGui::Checkbox("##playonawake", &playOnAwake))
            sourceComponent.SetPlayOnAwake(playOnAwake);
        ImGui::NextColumn();

        ImGui::Text("Loop");
        ImGui::NextColumn();
        bool looping = sourceComponent.GetLooping();
        if (ImGui::Checkbox("##loop", &looping))
            sourceComponent.SetLooping(looping);
        ImGui::NextColumn();

        ImGui::Text("Min Distance");
        ImGui::NextColumn();
        float minDistance = sourceComponent.GetMinDistance();
        if (ImGui::SliderFloat("##mindistnaceaudio", &minDistance, -3.0f, 3.0f, "%.2f"))
            sourceComponent.SetMinDistance(minDistance);
        ImGui::NextColumn();

        ImGui::Text("Max Distance");
        ImGui::NextColumn();
        float maxDistance = sourceComponent.GetMaxDistance();
        if (ImGui::SliderFloat("##maxdistanceaudio", &maxDistance, -3.0f, 3.0f, "%.2f"))
            sourceComponent.SetMaxDistance(maxDistance);
        ImGui::NextColumn();
        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e)
    {
        MonoScriptComponent& script = e.GetComponent<MonoScriptComponent>();
        
        if (!script.GetManagedClass())
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));
        if (UIUtils::PropertyScript("Script", script.GetTypeName()))
        {
            script.SetClassName(script.GetTypeName());
            script.OnInitialize(e);
        }

        ImGui::PopStyleColor(1);

        if (script.GetManagedClass() == nullptr)
            return;

        Ref<SerializableObjectInfo> objectInfo = script.GetObjectInfo();
        MonoObject* instance = script.GetManagedInstance();
        ScriptInspector::DrawObjectInspector(objectInfo, instance, nullptr);
    }

} // namespace Crowny
