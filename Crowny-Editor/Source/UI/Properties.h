#pragma once

#include "UI/UIUtils.h"

#include "Editor/EditorAssets.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    namespace UI
    {
        // struct UIScope
        // {
        //     std::function<void()> BeforeValueChangedCallback;
        // };
        //
        // UIScope scope;

        static void Pre(const char* label)
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text(label);
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            ImGui::PushItemWidth(-1);

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        static void Pre(const char* label, const char* helpText)
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text(label);
            if (std::strlen(helpText) != 0)
            {
                ImGui::SameLine();
                HelpMarker(helpText);
            }
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            ImGui::PushItemWidth(-1);

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        static void Post()
        {
            // if (ImGui::IsItemDeactivatedAfterEdit() && scope.BeforeValueChangedCallback)
            // {
            //     scope.BeforeValueChangedCallback();
            // }
            if (IsItemDisabled())
                ImGui::PopStyleVar();

            if (!IsItemDisabled())
                DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            Underline();
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool QuickTabsP(const char* label, std::initializer_list<const char*> buttonNameList,
                               Flags<TEnum, TUnderlying>& value)
        {
            Vector<TUnderlying> values;
            values.resize(buttonNameList.size());
            uint32_t pow = 1;
            for (uint32_t i = 0; i < (uint32_t)buttonNameList.size(); i++)
            {
                values[i] = pow;
                pow *= 2;
            }
            TUnderlying underlying = (TUnderlying)value;
            bool modified = QuickTabs(label, buttonNameList, values, underlying);
            value = Flags<TEnum, TUnderlying>(underlying);
            return modified;
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabsP(const char* label, std::initializer_list<const char*> buttonNameList, TUnderlying& value)
        {
            Vector<TUnderlying> values;
            values.resize(buttonNameList.size());
            uint32_t pow = 1;
            for (uint32_t i = 0; i < (uint32_t)buttonNameList.size(); i++)
            {
                values[i] = pow;
                pow *= 2;
            }
            return QuickTabs(label, buttonNameList, values, value);
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabs(const char* label, std::initializer_list<const char*> buttonNameList,
                              const Vector<TUnderlying>& enumValues, TUnderlying& value)
        {
            Pre(label);

            bool buttonClicked = false;
            {
                Vector<const char*> buttons(buttonNameList);
                UI::ScopedStyle noSpacingStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                float columnWidth = ImGui::GetContentRegionAvail().x;
                float minButtonSize = columnWidth / buttons.size();
                for (uint32_t i = 0; i < (uint32_t)buttons.size(); i++)
                {
                    const String id = String(buttons[i]) + "##" + std::string(label) + std::to_string(i);

                    if (value & enumValues[i])
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                              ImGui::ColorConvertU32ToFloat4(IM_COL32(84, 84, 84, 255)));
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_Button));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                    }

                    if (ImGui::Button(id.c_str(),
                                      ImVec2(std::max(minButtonSize, ImGui::CalcTextSize(buttons[i]).x), 0.0f)))
                    {
                        value ^= enumValues[i];
                        buttonClicked = true;
                    }
                    ImGui::SameLine();

                    ImGui::PopStyleColor(2);
                }
            }
            Post();
            return buttonClicked;
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabs(const char* label, const Vector<String>& buttons, const Vector<TUnderlying>& enumValues,
                              TUnderlying& value)
        {
            Pre(label);

            bool buttonClicked = false;
            {
                UI::ScopedStyle noSpacingStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                float columnWidth = ImGui::GetContentRegionAvail().x;
                float minButtonSize = columnWidth / buttons.size();
                for (uint32_t i = 0; i < (uint32_t)buttons.size(); i++)
                {
                    const String id = buttons[i] + "##" + std::string(label) + std::to_string(i);

                    if (value & enumValues[i])
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_ButtonActive);
                    else
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_Button);

                    if (ImGui::Button(id.c_str(),
                                      ImVec2(std::max(minButtonSize, ImGui::CalcTextSize(buttons[i].c_str()).x), 0.0f)))
                    {
                        value ^= enumValues[i];
                        buttonClicked = true;
                    }
                    ImGui::SameLine();

                    ImGui::PopStyleColor();
                }
            }
            Post();

            return buttonClicked;
        }

        static bool PropertyInput(const char* label, int8_t& value, int8_t step = 1, int8_t stepFast = 1)
        {
            Pre(label);
            // ImGuiInputTextFlags flags = s_ReturnAfterDeactivation ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            bool modified = UI::InputInt8(GenerateID(), &value, step, stepFast, flags);
            Post();

            return modified;
        }

        static bool PropertyInput(const char* label, int32_t& value, int32_t step = 1, int32_t stepFast = 1)
        {
            Pre(label);
            // ImGuiInputTextFlags flags = s_ReturnAfterDeactivation ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            bool modified = UI::InputInt32(GenerateID(), &value, step, stepFast, flags);
            Post();

            return modified;
        }

        static bool PropertyDictionary(int32_t& key, String& value)
        {
            ShiftCursor(10.0f, 9.0f);
            bool modified = UI::InputInt32(GenerateID(), &key);
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            ImGui::PushItemWidth(-1);

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            modified |= ImGui::InputText(GenerateID(), &value);
            Post();
            return modified;
        }

        static bool PropertyDictionary(String& key, int32_t& value)
        {
            ShiftCursor(10.0f, 9.0f);
            bool modified = ImGui::InputText(GenerateID(), &key);
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            ImGui::PushItemWidth(-1);

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            modified |= UI::InputInt32(GenerateID(), &value);
            Post();
            return modified;
        }

        static bool PropertyInput(const char* label, uint32_t& value, uint32_t step = 1, uint32_t stepFast = 1)
        {
            Pre(label);
            // ImGuiInputTextFlags flags = s_ReturnAfterDeactivation ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            bool modified = UI::InputUInt32(GenerateID(), &value, step, stepFast, flags);
            Post();

            return modified;
        }

        static char* s_MultilineBuffer;

        static void Property(const char* label)
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text(label);
            ImGui::NextColumn();
            ImGui::NextColumn();
            Underline();
        }

        static bool Property(const char* label, String& value)
        {
            Pre(label);
            // if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
            // strcpy(buffer, "---");
            bool modified = ImGui::InputText(GenerateID(), &value);
            Post();

            return modified;
        }

        static bool Property(const char* label, char& c)
        {
            char ar[2] = { c, '\0' };
            Pre(label);
            bool modified = ImGui::InputText(GenerateID(), ar, 2);
            Post();

            return modified;
        }

        static bool Property(const char* label, bool& value, const char* helpText = "")
        {
            Pre(label, helpText);
            bool modified = ImGui::Checkbox(GenerateID(), &value);
            Post();
            return modified;
        }

        static bool PropertyMultiline(const char* label, String& value)
        {
            bool modified = false;

            Pre(label);

            if (!s_MultilineBuffer)
            {
                s_MultilineBuffer = new char[1024 * 1024]; // 1KB
                memset(s_MultilineBuffer, 0, 1024 * 1024);
            }

            strcpy(s_MultilineBuffer, value.c_str());

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            if (ImGui::InputTextMultiline(GenerateID(), s_MultilineBuffer, 1024 * 1024))
            {
                value = s_MultilineBuffer;
                modified = true;
            }

            if (IsItemDisabled())
                ImGui::PopStyleVar();

            ImGui::PopItemWidth();
            ImGui::NextColumn();

            return modified;
        }

        static bool PropertyColor(const char* label, glm::vec3& value, ImGuiColorEditFlags flags = 0)
        {
            Pre(label);
            bool modified = ImGui::ColorEdit3(GenerateID(), glm::value_ptr(value), flags);
            Post();
            return modified;
        }

        static bool PropertyColor(const char* label, glm::vec4& value, ImGuiColorEditFlags flags = 0)
        {
            Pre(label);
            bool modified = ImGui::ColorEdit4(GenerateID(), glm::value_ptr(value), flags);
            Post();
            return modified;
        }

        static bool Property(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f,
                             const char* helpText = "")
        {
            Pre(label, helpText);
            bool modified = UI::DragFloat(GenerateID(), &value, delta, min, max);
            Post();
            return modified;
        }

        static bool Property(const char* label, int8_t& value, int8_t min = 0, int8_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt8(GenerateID(), &value, 1.0f, min, max);
            Post();
            return modified;
        }

        static bool Property(const char* label, int16_t& value, int16_t min = 0, int16_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt16(GenerateID(), &value, 1.0f, min, max);
            Post();
            return modified;
        }

        static bool Property(const char* label, int32_t& value, int32_t min = 0, int32_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt32(GenerateID(), &value, 1.0f, min, max);
            Post();
            return modified;
        }

        static bool Property(const char* label, int64_t& value, int64_t min = 0, int64_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt64(GenerateID(), &value, 1.0f, min, max);
            Post();
            return modified;
        }

        static bool Property(const char* label, uint8_t& value, uint8_t minValue = 0, uint8_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt8(GenerateID(), &value, 1.0f, minValue, maxValue);
            Post();

            return modified;
        }

        static bool Property(const char* label, uint16_t& value, uint16_t minValue = 0, uint16_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt16(GenerateID(), &value, 1.0f, minValue, maxValue);
            Post();

            return modified;
        }

        static bool Property(const char* label, uint32_t& value, uint32_t minValue = 0, uint32_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt32(GenerateID(), &value, 1.0f, minValue, maxValue);
            Post();

            return modified;
        }

        static bool Property(const char* label, uint64_t& value, uint64_t minValue = 0, uint64_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt64(GenerateID(), &value, 1.0f, minValue, maxValue);
            Post();

            return modified;
        }

        static bool Property(const char* label, double& value, float delta = 0.1f, double min = 0.0, double max = 0.0)
        {
            Pre(label);
            bool modified = DragDouble(GenerateID(), &value, delta, min, max);
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0f,
                             float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat2(GenerateID(), glm::value_ptr(value), delta, min, max);
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0f,
                             float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat3(GenerateID(), glm::value_ptr(value), delta, min, max);
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0f,
                             float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat4(GenerateID(), glm::value_ptr(value), delta, min, max);
            Post();

            return modified;
        }

        static bool PropertySlider(const char* label, int& value, int min, int max)
        {
            Pre(label);
            bool modified = ImGui::SliderInt(GenerateID(), &value, min, max);
            Post();

            return modified;
        }

        static bool PropertySlider(const char* label, float& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat(GenerateID(), &value, min, max);
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec2& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat2(GenerateID(), glm::value_ptr(value), min, max);
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec3& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat3(GenerateID(), glm::value_ptr(value), min, max);
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec4& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat4(GenerateID(), glm::value_ptr(value), min, max);
            Post();
            return modified;
        }

        static bool PropertyFilepath(const char* label, FileDialogType type, String& value, const String& title = {}, const Path& initialDir = {})
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text(label);
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            const auto& style = ImGui::GetStyle();
            ImGui::PushItemWidth(-34.0f);
            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            bool modified = ImGui::InputText(GenerateID(), &value);
            ImGui::SameLine();
            const ImColor c_ButtonTint = IM_COL32(192, 192, 192, 255);
            const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), { 24, 24 });
            DrawButtonImage(EditorAssets::Get().FolderIcon, c_ButtonTint, c_ButtonTint, c_ButtonTint,
                            // UI::RectE(UI::GetItemRect(), 24.0f, 24.0f));
                            UI::GetItemRect());
            if (clicked)
            {
                Vector<Path> outPaths;
                const Path& initialDirToUse = initialDir.empty() ? ProjectLibrary::Get().GetAssetFolder() : initialDir;
                const String& titleToUse = title.empty() ? label : title;
                if (FileSystem::OpenFileDialog(type, outPaths, titleToUse, initialDirToUse) && outPaths.size() > 0)
                {
                    value = outPaths[0].string();
                    modified = true;
                }
            }

            Post();

            return modified;
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, const Vector<String>& options, TEnum& selected)
        {
            TUnderlying selectedIndex = (TUnderlying)selected;

            String current = options[selectedIndex];
            Pre(label);
            bool modified = false;
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            // ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo(id.c_str(), current.c_str()))
            {
                for (int i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (i == selectedIndex);
                    if (ImGui::Selectable(options[i].c_str(), is_selected))
                    {
                        current = options[i];
                        selected = (TEnum)i;
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            Post();

            return modified;
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, std::initializer_list<const char*> optionsList, TEnum& selected)
        {
            Vector<const char*> options(optionsList);
            TUnderlying selectedIndex = (TUnderlying)selected;

            const char* current = options[selectedIndex];
            Pre(label);
            bool modified = false;
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            if (ImGui::BeginCombo(id.c_str(), current))
            {
                for (int i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (current == options[i]);
                    if (ImGui::Selectable(options[i], is_selected))
                    {
                        current = options[i];
                        selected = (TEnum)i;
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            Post();

            return modified;
        }

        template <typename Type, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, const Vector<Type>& options, TUnderlying& selected,
                                     std::function<String(const Type&)> selector)
        {
            TUnderlying selectedIndex = (TUnderlying)selected;
            String& current = selector(options[selectedIndex]);
            Pre(label);
            bool modified = false;
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            // ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo(id.c_str(), current.c_str()))
            {
                for (int i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (i == selectedIndex);
                    const String& selectable = selector(options[i]);
                    if (ImGui::Selectable(selectable.c_str(), is_selected))
                    {
                        current = selectable[i];
                        selected = (TUnderlying)i;
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            Post();

            return modified;
        }

    } // namespace UI

} // namespace Crowny