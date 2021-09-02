#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorDefaults.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.inl>
#include <glm/gtx/matrix_decompose.hpp>
#include <mono/metadata/object.h>

namespace Crowny
{

    extern void LoadTexture(const std::string& path, Ref<Texture>& out);

    template <> void ComponentEditorWidget<TransformComponent>(Entity e)
    {
        auto& t = e.GetComponent<TransformComponent>();
        bool changed = false;

        ImGui::Columns(2);
        ImGui::Text("Transform");
        ImGui::NextColumn();
        ImGui::DragFloat3("Transform##", glm::value_ptr(t.Position), DRAG_SENSITIVITY);
        ImGui::NextColumn();

        ImGui::Text("Rotation");
        ImGui::NextColumn();
        glm::vec3 deg = glm::degrees(t.Rotation);
        ImGui::DragFloat3("Rotation##", glm::value_ptr(deg), DRAG_SENSITIVITY);
        ImGui::NextColumn();
        t.Rotation = glm::radians(deg);

        ImGui::Text("Scale");
        ImGui::NextColumn();
        ImGui::DragFloat3("Scale##", glm::value_ptr(t.Scale), DRAG_SENSITIVITY);
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
        if (ImGui::BeginCombo("Projection##", projections[(int32_t)cam.GetProjectionType()]))
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
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                static float maxClippingPlane = 1000000.0f;
                static float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                ImGui::Text("Near");
                ImGui::NextColumn();
                float near = cam.GetPerspectiveNearClip();
                if (ImGui::DragScalar("##near", ImGuiDataType_Float, &near, 0.1f, &minClippingPlane, &maxClippingPlane,
                                      "%.2f"))
                    cam.SetPerspectiveNearClip(std::clamp(near, minClippingPlane, maxClippingPlane));
                ImGui::NextColumn();

                float far = cam.GetPerspectiveFarClip();
                ImGui::Text("Far");
                ImGui::NextColumn();
                if (ImGui::DragScalar("##far", ImGuiDataType_Float, &far, 0.1f, &minClippingPlane, &maxClippingPlane,
                                      "%.2f"))
                    cam.SetPerspectiveFarClip(std::clamp(far, minClippingPlane, maxClippingPlane));
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
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);

            if (ImGui::CollapsingHeader("Clipping Planes"))
            {
                ImGui::Indent(30.f);
                static float maxClippingPlane = 1000000.0f;
                static float minClippingPlane = 0.0000001f;

                ImGui::Columns(2);
                ImGui::Text("Near");
                ImGui::NextColumn();
                float near = cam.GetOrthographicNearClip();
                if (ImGui::DragScalar("##near", ImGuiDataType_Float, &near, 0.1f, &minClippingPlane, &maxClippingPlane,
                                      "%.2f"))
                    cam.SetOrthographicNearClip(std::clamp(near, minClippingPlane, maxClippingPlane));
                ImGui::NextColumn();

                float far = cam.GetOrthographicFarClip();
                ImGui::Text("Far");
                ImGui::NextColumn();
                if (ImGui::DragScalar("##far", ImGuiDataType_Float, &far, 0.1f, &minClippingPlane, &maxClippingPlane,
                                      "%.2f"))
                    cam.SetOrthographicFarClip(std::clamp(far, minClippingPlane, maxClippingPlane));
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
            ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Font->GetTexture()), { (float)t.Font->GetTexture()->GetWidth(), (float)t.Font->GetTexture()->GetHeight() }, { 0, 1 }, { 1, 0 });
            ImGui::EndPopup();
        }
#endif
        ImGui::NextColumn();

        ImGui::Text("Font Size");
        ImGui::NextColumn();

        int32_t size = t.Font->GetSize();
        if (ImGui::InputInt("##fontsize", &size))
            t.Font = FontManager::Get(t.Font->GetName(), size);
        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<SpriteRendererComponent>(Entity e)
    {
        auto& t = e.GetComponent<SpriteRendererComponent>();

        if (t.Texture)
            ImGui::Image(ImGui_ImplVulkan_AddTexture(t.Texture), { 50.0f, 50.0f}, { 0, 1 }, { 1, 0 });
        else
            ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().UnassignedTexture), { 50.0f, 50.0f }, { 0, 1 }, { 1, 0 });
        if (ImGui::IsItemClicked())
        {
            std::vector<std::string> outPaths;
            if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
            {
                Ref<Texture> result;
                LoadTexture(outPaths[0], result);
                t.Texture = result;
            }
        }

        ImGui::SameLine();
        ImGui::ColorEdit4("Color", glm::value_ptr(t.Color));
    }

    template <> void ComponentEditorWidget<MeshRendererComponent>(Entity e)
    {
        auto& mesh = e.GetComponent<MeshRendererComponent>().Mesh;

        ImGui::Text("Path");
    }

    template <> void ComponentEditorWidget<AudioListenerComponent>(Entity e) {}

    template <> void ComponentEditorWidget<AudioSourceComponent>(Entity e)
    {
        auto& sourceComponent = e.GetComponent<AudioSourceComponent>();

        ImGui::Columns(2);

        ImGui::Text("Audio Clip");
        ImGui::NextColumn();
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

        float minDistance = sourceComponent.GetMinDistance();
        if (ImGui::SliderFloat("##mindistnace", &minDistance, -3.0f, 3.0f, "%.2f"))
            sourceComponent.SetMinDistance(minDistance);

        float maxDistance = sourceComponent.GetMaxDistance();
        if (ImGui::SliderFloat("##maxdistance", &maxDistance, -3.0f, 3.0f, "%.2f"))
            sourceComponent.SetMaxDistance(maxDistance);

        ImGui::Columns(1);
    }

    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity e)
    {
        auto& script = e.GetComponent<MonoScriptComponent>();
        ImGui::Columns(2);
        ImGui::Text("Script");
        ImGui::NextColumn();

        if (!script.GetManagedClass())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(255, 0, 0));
        }
        else
            ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(0, 255, 0));
        std::string name = script.GetManagedClass() ? std::string(script.GetManagedClass()->GetName()) : "";
        if (ImGui::InputText("##scriptName", &name))
        {
            script.SetClassName(name);
        }

        ImGui::PopStyleColor(1);

        if (script.GetManagedClass() == nullptr)
        {
            ImGui::Columns(1);
            return;
        }

        ImGui::NextColumn();
        auto& fields = script.GetSerializableFields();
        for (uint32_t i = 0; i < fields.size(); i++)
        {
            auto* field = fields[i];
            ImGui::PushID(i);
            ImGui::Text("%s", field->GetName().c_str());
            ImGui::NextColumn();
            MonoObject* instance = script.GetManagedInstance();
            if (instance)
            {
                switch (field->GetPrimitiveType())
                {
                case (MonoPrimitiveType::Bool): {
                    bool value = false;
                    field->Get(script.ManagedInstance, &value);
                    if (ImGui::Checkbox("##bool", &value))
                        field->Set(instance, &value);
                    break;
                }
                case (MonoPrimitiveType::String): {
                    MonoString* value;
                    field->Get(instance, value);
                    std::string nativeValue = MonoUtils::FromMonoString(value);
                    if (ImGui::InputText("##field2", &nativeValue))
                        field->Set(instance, MonoUtils::ToMonoString(nativeValue));
                    break;
                }
                case (MonoPrimitiveType::ValueType): {
                    if (MonoUtils::IsEnum(field->GetType()->GetInternalPtr()))
                    {
                        // TODO: These commonly used methods (GetEnumNames, Compile) should be stored globally instead
                        // of having to get them every time Also no mono code in the editor please.
                        void* enumType = (void*)mono_type_get_object(
                          CWMonoRuntime::GetDomain(), MonoUtils::GetType(field->GetType()->GetInternalPtr()));
                        MonoArray* ar = (MonoArray*)CWMonoRuntime::GetBuiltinClasses()
                                          .ScriptUtils->GetMethod("GetEnumNames", 1)
                                          ->Invoke(nullptr, &enumType);
                        uint32_t size = mono_array_length(ar);
                        std::vector<std::string> enumValues;
                        enumValues.resize(size);
                        for (uint32_t j = 0; j < size; j++) // Do this once!
                        {
                            enumValues[j] = MonoUtils::FromMonoString(mono_array_get(ar, MonoString*, j));
                        }
                        uint32_t value;
                        field->Get(instance, &value);
                        if (ImGui::BeginCombo("##enum", enumValues[value].c_str()))
                        {
                            uint32_t newSelected = value;
                            for (uint32_t i = 0; i < size; i++)
                            {
                                if (ImGui::Selectable(enumValues[i].c_str(), i == value))
                                {
                                    newSelected = i;
                                }
                            }
                            if (newSelected != value)
                            {
                                void* tmp = &newSelected;
                                field->Set(instance, tmp);
                            }
                            ImGui::EndCombo();
                        }
                    }
                    break;
                }
                default: {
                    CWMonoClass* rng = CWMonoRuntime::GetBuiltinClasses().RangeAttribute;
                    MonoObject* obj = field->GetAttribute(rng);
                    if (obj) // show slider, more types?
                    {
                        if (field->GetPrimitiveType() == MonoPrimitiveType::R32)
                        {
                            float min, max, val;
                            rng->GetField("min")->Get(obj, &min); // TODO: cache these
                            rng->GetField("max")->Get(obj, &max);

                            bool slider;
                            rng->GetField("slider")->Get(obj,
                                                         &slider); // TODO: fall thourgh to the code down below, with
                                                                   // the provided range if we don't need a slider.
                            field->Get(instance, &val);
                            if (slider && ImGui::SliderFloat("##sliderfloat", &val, min, max))
                                field->Set(instance, &val); // These should prob be set after compilation, not here
                        }
                        else if (field->GetPrimitiveType() == MonoPrimitiveType::R64)
                        {
                            float min, max;
                            double val;
                            rng->GetField("min")->Get(obj, &min); // TODO: cache these
                            rng->GetField("max")->Get(obj, &max);

                            bool slider;
                            rng->GetField("slider")->Get(obj,
                                                         &slider); // TODO: fall thourgh to the code down below, with
                                                                   // the provided range if we don't need a slider.
                            field->Get(instance, &val);
                            if (slider && ImGui::SliderScalar("##sliderdouble", ImGuiDataType_Double, &val, &min, &max))
                                field->Set(instance, &val); // These should prob be set after compilation, not here
                        }
                        else if (field->GetPrimitiveType() == MonoPrimitiveType::I8)
                        {
                            float min, max;
                            int8_t val;
                            rng->GetField("min")->Get(obj, &min); // TODO: cache these
                            rng->GetField("max")->Get(obj, &max);

                            bool slider; // TODO: Clamp the values, so that they are in Int8 range
                            rng->GetField("slider")->Get(obj,
                                                         &slider); // TODO: fall thourgh to the code down below, with
                                                                   // the provided range if we don't need a slider.
                            field->Get(instance, &val);
                            if (slider && ImGui::SliderInt("##sliderint", (int*)&val, (int8_t)min, (int8_t)max))
                                field->Set(instance, &val); // These should prob be set after compilation, not here
                        }
                        else if (field->GetPrimitiveType() == MonoPrimitiveType::I16)
                        {
                            float min, max;
                            int16_t val;
                            rng->GetField("min")->Get(obj, &min); // TODO: cache these
                            rng->GetField("max")->Get(obj, &max);

                            bool slider; // TODO: Clamp the values, so that they are in Int16 range
                            rng->GetField("slider")->Get(obj,
                                                         &slider); // TODO: fall thourgh to the code down below, with
                                                                   // the provided range if we don't need a slider.
                            field->Get(instance, &val);
                            if (slider && ImGui::SliderInt("##sliderdouble", (int*)&val, (int16_t)min, (int16_t)max))
                                field->Set(instance, &val); // These should prob be set after compilation, not here
                        }
                    }
                    else // show input text
                    {
                        if (field->GetPrimitiveType() == MonoPrimitiveType::R32)
                        {
                            float value;
                            field->Get(instance, &value);
                            std::string strFloat = std::to_string(value);
                            if (ImGui::InputText("##field1", &strFloat))
                            {
                                float val = StringUtils::ParseFloat(strFloat);
                                field->Set(instance, &val);
                            }
                            continue;
                        }
                        else if (field->GetPrimitiveType() == MonoPrimitiveType::R64)
                        {
                            double value;
                            field->Get(instance, &value);
                            std::string strDouble = std::to_string(value);
                            if (ImGui::InputText("##field1", &strDouble))
                            {
                                float val = StringUtils::ParseDouble(strDouble);
                                field->Set(instance, &val);
                            }
                            continue;
                        }
                        int64_t value; // do all int cases here (nobody "should" need to use full UINT64_MAX, I hope)
                        field->Get(instance, &value);
                        std::string strInt =
                          std::to_string(value); // use string so we can accept math expressions ( 1 + 2 -> 3)
                        if (ImGui::InputText("##field2", &strInt))
                        {
                            if (field->GetPrimitiveType() == MonoPrimitiveType::I8)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < INT8_MAX)
                                    val = INT8_MAX;
                                else if (val < INT8_MIN)
                                    val = INT8_MIN;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::I16)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < INT16_MAX)
                                    val = INT16_MAX;
                                else if (val < INT16_MIN)
                                    val = INT16_MIN;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::I32)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < INT32_MAX)
                                    val = INT32_MAX;
                                else if (val < INT32_MIN)
                                    val = INT32_MIN;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::I64)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < INT64_MAX)
                                    val = INT64_MAX;
                                else if (val < INT64_MIN)
                                    val = INT64_MIN;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::U8)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < UINT8_MAX)
                                    val = UINT8_MAX;
                                else if (val < 0)
                                    val = 0;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::U16)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < UINT16_MAX)
                                    val = UINT16_MAX;
                                else if (val < 0)
                                    val = 0;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::U32)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < UINT32_MAX)
                                    val = UINT32_MAX;
                                else if (val < 0)
                                    val = 0;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::U64)
                            {
                                int64_t val = StringUtils::ParseLong(strInt);
                                if (val < UINT64_MAX) // not possibe
                                    val = UINT64_MAX;
                                else if (val < 0)
                                    val = 0;
                                field->Set(instance, &val);
                            }
                            else if (field->GetPrimitiveType() == MonoPrimitiveType::Char)
                            {
                                if (strInt.size() > 1)
                                    strInt = std::string(strInt.data(), 1);
                                field->Set(instance, &strInt[0]);
                            }
                        }
                    }
                    break;
                }
                }
                ImGui::NextColumn();
            }

            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

} // namespace Crowny
