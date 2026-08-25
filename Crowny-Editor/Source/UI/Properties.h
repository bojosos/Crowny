#pragma once

#include "Crowny/Utils/SmallVector.h"
#include "UI/UIUtils.h"

#include "Editor/EditorAssets.h"
#include "Editor/UndoRedo.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <type_traits>

namespace Crowny
{
    struct PrefabComponent;

    namespace PrefabOverrideContext
    {
        inline PrefabComponent* s_ActivePrefabComponent = nullptr;
        inline String s_ActiveComponentName;

        void MarkIfModified(bool modified, const char* propertyName);
        bool IsOverridden(const char* propertyName);
    } // namespace PrefabOverrideContext

    namespace UI
    {
        enum class PropertyIconGlyph
        {
            Texture,
            Justified,
            Flush,
            VerticalTop,
            VerticalMiddle,
            VerticalBottom,
            Baseline,
            Midline
        };

        struct PropertyIconTab
        {
            const char* Tooltip = "";
            int32_t Value = 0;
            Ref<Texture> Icon;
            PropertyIconGlyph Glyph = PropertyIconGlyph::Texture;
        };

        // struct UIScope
        // {
        //     std::function<void()> BeforeValueChangedCallback;
        // };
        //
        // UIScope scope;

        static void Pre(const char* label)
        {
            ShiftCursor(10.0f, 9.0f);

            // Draw override indicator: 3px orange vertical bar
            if (PrefabOverrideContext::IsOverridden(label))
            {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x - 8.0f, pos.y), ImVec2(pos.x - 5.0f, pos.y + ImGui::GetTextLineHeight()),
                                                          IM_COL32(236, 158, 36, 255));
            }

            ImGui::Text("%s", label);
            ImGui::NextColumn();
            ShiftCursorY(4.0f);
            ImGui::PushItemWidth(-1);

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }

        static void Pre(const char* label, const char* helpText)
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text("%s", label);
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

        template <typename TUnderlying, typename TValueAt>
        static bool QuickTabsImpl(const char* label, std::initializer_list<const char*> buttonNameList, const TValueAt& valueAt, TUnderlying& value)
        {
            Pre(label);

            if (buttonNameList.size() == 0u)
            {
                Post();
                return false;
            }

            bool buttonClicked = false;
            const bool mixed = (GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;
            {
                UI::ScopedStyle noSpacingStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                float columnWidth = ImGui::GetContentRegionAvail().x;
                float minButtonSize = columnWidth / static_cast<float>(buttonNameList.size());
                uint32_t index = 0u;
                ImGui::PushID(label);
                for (const char* buttonName : buttonNameList)
                {
                    const TUnderlying optionValue = valueAt(index);

                    if (!mixed && (value & optionValue))
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(IM_COL32(84, 84, 84, 255)));
                    }
                    else
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_Button));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonActive));
                    }

                    ImGui::PushID(static_cast<int>(index));
                    if (ImGui::Button(buttonName, ImVec2(std::max(minButtonSize, ImGui::CalcTextSize(buttonName).x), 0.0f)))
                    {
                        value ^= optionValue;
                        buttonClicked = true;
                    }
                    ImGui::PopID();
                    if (++index < buttonNameList.size())
                        ImGui::SameLine();

                    ImGui::PopStyleColor(2);
                }
                ImGui::PopID();
            }
            UndoRedo::Get().OnItemInteract(buttonClicked);
            Post();
            return buttonClicked;
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabs(const char* label, std::initializer_list<const char*> buttonNameList, const Vector<TUnderlying>& enumValues,
                              TUnderlying& value)
        {
            return QuickTabsImpl(label, buttonNameList, [&enumValues](uint32_t index) { return enumValues[index]; }, value);
        }

        static void DrawPropertyIconGlyph(const PropertyIconTab& tab, const ImRect& bounds, ImU32 color)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float iconSize = std::min(bounds.GetWidth(), bounds.GetHeight()) - 8.0f;
            const ImVec2 iconMin(bounds.GetCenter().x - iconSize * 0.5f, bounds.GetCenter().y - iconSize * 0.5f);
            const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
            if (tab.Glyph == PropertyIconGlyph::Texture)
            {
                if (tab.Icon)
                {
                    drawList->AddImage(ImGuiVulkanTexture::Get(tab.Icon), iconMin, iconMax, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), color);
                }
                else
                {
                    const char fallback[] = { tab.Tooltip != nullptr && tab.Tooltip[0] != '\0' ? tab.Tooltip[0] : '?', '\0' };
                    const ImVec2 textSize = ImGui::CalcTextSize(fallback);
                    drawList->AddText(ImVec2(bounds.GetCenter().x - textSize.x * 0.5f, bounds.GetCenter().y - textSize.y * 0.5f), color, fallback);
                }
                return;
            }

            const float left = iconMin.x + 1.0f;
            const float right = iconMax.x - 1.0f;
            const float top = iconMin.y + 1.0f;
            const float bottom = iconMax.y - 1.0f;
            const float width = right - left;
            const float height = bottom - top;
            const auto line = [&](float x1, float y, float x2, float thickness = 1.0f) {
                drawList->AddLine(ImVec2(x1, y), ImVec2(x2, y), color, thickness);
            };

            if (tab.Glyph == PropertyIconGlyph::Justified || tab.Glyph == PropertyIconGlyph::Flush)
            {
                for (uint32_t row = 0; row < 4u; ++row)
                {
                    const float y = top + height * (0.15f + static_cast<float>(row) * 0.24f);
                    const float rowRight = tab.Glyph == PropertyIconGlyph::Justified && row == 3u ? left + width * 0.68f : right;
                    line(left, y, rowRight, 1.5f);
                }
                return;
            }

            if (tab.Glyph == PropertyIconGlyph::Baseline || tab.Glyph == PropertyIconGlyph::Midline)
            {
                const float guide = tab.Glyph == PropertyIconGlyph::Baseline ? bottom - height * 0.18f : top + height * 0.5f;
                line(left, guide, right, 1.5f);
                const float blockBottom = tab.Glyph == PropertyIconGlyph::Baseline ? guide : guide + height * 0.27f;
                const float heights[] = { height * 0.42f, height * 0.62f, height * 0.34f };
                for (uint32_t block = 0; block < 3u; ++block)
                {
                    const float x = left + width * (0.16f + static_cast<float>(block) * 0.34f);
                    drawList->AddRectFilled(ImVec2(x, blockBottom - heights[block]), ImVec2(x + 2.0f, blockBottom), color);
                }
                return;
            }

            const float guide = tab.Glyph == PropertyIconGlyph::VerticalTop      ? top
                                : tab.Glyph == PropertyIconGlyph::VerticalBottom ? bottom
                                                                                 : top + height * 0.5f;
            line(left, guide, right, 1.5f);
            const float contentTop = tab.Glyph == PropertyIconGlyph::VerticalTop      ? guide + height * 0.18f
                                     : tab.Glyph == PropertyIconGlyph::VerticalBottom ? guide - height * 0.52f
                                                                                      : guide - height * 0.18f;
            line(left + width * 0.15f, contentTop, right - width * 0.15f, 1.5f);
            line(left + width * 0.25f, contentTop + height * 0.22f, right - width * 0.25f, 1.5f);
        }

        template <typename TEnum> static bool PropertyIconTabs(const char* label, std::initializer_list<PropertyIconTab> tabList, TEnum& value)
        {
            Pre(label);
            if (tabList.size() == 0u)
            {
                ImGui::TextDisabled("No options available");
                Post();
                return false;
            }

            bool modified = false;
            const bool mixed = (GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;
            UI::ScopedStyle noSpacingStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            const float buttonWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x / static_cast<float>(tabList.size()));
            const float buttonHeight = ImGui::GetFrameHeight();
            uint32_t index = 0u;
            ImGui::PushID(label);
            for (const PropertyIconTab& tab : tabList)
            {
                const bool selected = !mixed && static_cast<int32_t>(value) == tab.Value;
                ImGui::PushID(static_cast<int>(index));
                const bool clicked = ImGui::InvisibleButton("##PropertyIconTab", ImVec2(buttonWidth, buttonHeight));
                ImGui::PopID();
                const ImRect bounds = UI::GetItemRect();
                const ImGuiCol backgroundColor = selected ? ImGuiCol_ButtonActive : ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered : ImGuiCol_Button;
                ImGui::GetWindowDrawList()->AddRectFilled(bounds.Min, bounds.Max, ImGui::GetColorU32(backgroundColor), 2.0f);
                DrawPropertyIconGlyph(tab, bounds, ImGui::GetColorU32(selected ? ImGuiCol_Text : ImGuiCol_TextDisabled));
                UI::SetTooltip(tab.Tooltip);
                if (clicked && !selected)
                {
                    value = static_cast<TEnum>(tab.Value);
                    modified = true;
                }
                UndoRedo::Get().OnItemInteract(clicked);
                if (++index < tabList.size())
                    ImGui::SameLine(0.0f, 0.0f);
            }
            ImGui::PopID();
            Post();
            return modified;
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabs(const char* label, const Vector<String>& buttons, const Vector<TUnderlying>& enumValues, TUnderlying& value)
        {
            Pre(label);

            if (buttons.empty())
            {
                Post();
                return false;
            }

            bool buttonClicked = false;
            const bool mixed = (GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;
            {
                UI::ScopedStyle noSpacingStyle(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                float columnWidth = ImGui::GetContentRegionAvail().x;
                float minButtonSize = columnWidth / buttons.size();
                ImGui::PushID(label);
                for (uint32_t i = 0; i < (uint32_t)buttons.size(); i++)
                {
                    if (!mixed && (value & enumValues[i]))
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_ButtonActive);
                    else
                        ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_Button);

                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Button(buttons[i].c_str(), ImVec2(std::max(minButtonSize, ImGui::CalcTextSize(buttons[i].c_str()).x), 0.0f)))
                    {
                        value ^= enumValues[i];
                        buttonClicked = true;
                    }
                    ImGui::PopID();
                    if (i + 1u < buttons.size())
                        ImGui::SameLine();

                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
            UndoRedo::Get().OnItemInteract(buttonClicked);
            Post();

            return buttonClicked;
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool QuickTabsP(const char* label, std::initializer_list<const char*> buttonNameList, Flags<TEnum, TUnderlying>& value)
        {
            uint32_t pow = 1;
            const auto valueAt = [&pow](uint32_t) {
                const TUnderlying result = static_cast<TUnderlying>(pow);
                pow *= 2;
                return result;
            };
            TUnderlying underlying = (TUnderlying)value;
            bool modified = QuickTabsImpl(label, buttonNameList, valueAt, underlying);
            value = Flags<TEnum, TUnderlying>(underlying);
            return modified;
        }

        template <typename TUnderlying = int32_t>
        static bool QuickTabsP(const char* label, std::initializer_list<const char*> buttonNameList, TUnderlying& value)
        {
            uint32_t pow = 1;
            const auto valueAt = [&pow](uint32_t) {
                const TUnderlying result = static_cast<TUnderlying>(pow);
                pow *= 2;
                return result;
            };
            return QuickTabsImpl(label, buttonNameList, valueAt, value);
        }

        static bool PropertyInput(const char* label, int8_t& value, int8_t step = 1, int8_t stepFast = 1)
        {
            Pre(label);
            UI::InputInt8(GenerateID(), &value, step, stepFast);
            bool modified = ImGui::IsItemDeactivatedAfterEdit();
            UndoRedo::Get().OnItemInteract(modified);
            Post();

            return modified;
        }

        static bool PropertyInput(const char* label, int32_t& value, int32_t step = 1, int32_t stepFast = 1)
        {
            Pre(label);
            UI::InputInt32(GenerateID(), &value, step, stepFast);
            bool modified = ImGui::IsItemDeactivatedAfterEdit();
            UndoRedo::Get().OnItemInteract(modified);
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
            UndoRedo::Get().OnItemInteract(modified);
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
            UndoRedo::Get().OnItemInteract(modified);
            Post();
            return modified;
        }

        static bool PropertyInput(const char* label, uint32_t& value, uint32_t step = 1, uint32_t stepFast = 1)
        {
            Pre(label);
            UI::InputUInt32(GenerateID(), &value, step, stepFast);
            bool modified = ImGui::IsItemDeactivatedAfterEdit();
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static void Property(const char* label)
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text("%s", label);
            ImGui::NextColumn();
            ImGui::NextColumn();
            Underline();
        }

        static bool Property(const char* label, String& value)
        {
            Pre(label);
            const bool mixed = (GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;
            bool modified = mixed ? ImGui::InputTextWithHint(GenerateID(), "Multiple values", &value) : ImGui::InputText(GenerateID(), &value);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, char& c)
        {
            char ar[2] = { c, '\0' };
            Pre(label);
            bool modified = ImGui::InputText(GenerateID(), ar, 2);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, bool& value, const char* helpText = "")
        {
            Pre(label, helpText);
            bool modified = ImGui::Checkbox(GenerateID(), &value);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertyMultiline(const char* label, String& value)
        {
            Pre(label);
            const bool mixed = (GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0;

            if (IsItemDisabled())
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            const bool modified = ImGui::InputTextMultiline(GenerateID(), &value);

            if (mixed && value.empty() && !ImGui::IsItemActive())
            {
                const ImRect bounds = UI::GetItemRect();
                ImGui::GetWindowDrawList()->AddText(bounds.Min + ImGui::GetStyle().FramePadding, ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                                    "Multiple values");
            }

            UndoRedo::Get().OnItemInteract();

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
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertyColor(const char* label, glm::vec4& value, ImGuiColorEditFlags flags = 0)
        {
            Pre(label);
            bool modified = ImGui::ColorEdit4(GenerateID(), glm::value_ptr(value), flags);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, float& value, float delta = 0.1f, float min = 0.0f, float max = 1.0f, const char* helpText = "")
        {
            Pre(label, helpText);
            bool modified = UI::DragFloat(GenerateID(), &value, delta, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, int8_t& value, int8_t min = 0, int8_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt8(GenerateID(), &value, 1.0f, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, int16_t& value, int16_t min = 0, int16_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt16(GenerateID(), &value, 1.0f, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, int32_t& value, int32_t min = 0, int32_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt32(GenerateID(), &value, 1.0f, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, int64_t& value, int64_t min = 0, int64_t max = 0)
        {
            Pre(label);
            bool modified = UI::DragInt64(GenerateID(), &value, 1.0f, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool Property(const char* label, uint8_t& value, uint8_t minValue = 0, uint8_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt8(GenerateID(), &value, 1.0f, minValue, maxValue);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, uint16_t& value, uint16_t minValue = 0, uint16_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt16(GenerateID(), &value, 1.0f, minValue, maxValue);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, uint32_t& value, uint32_t minValue = 0, uint32_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt32(GenerateID(), &value, 1.0f, minValue, maxValue);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, uint64_t& value, uint64_t minValue = 0, uint64_t maxValue = 0)
        {
            Pre(label);
            bool modified = UI::DragUInt64(GenerateID(), &value, 1.0f, minValue, maxValue);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, double& value, float delta = 0.1f, double min = 0.0, double max = 0.0)
        {
            Pre(label);
            bool modified = DragDouble(GenerateID(), &value, delta, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec2& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat2(GenerateID(), glm::value_ptr(value), delta, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec3& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat3(GenerateID(), glm::value_ptr(value), delta, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool Property(const char* label, glm::vec4& value, float delta = 0.1f, float min = 0.0f, float max = 0.0f)
        {
            Pre(label);
            bool modified = ImGui::DragFloat4(GenerateID(), glm::value_ptr(value), delta, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool PropertySlider(const char* label, int& value, int min, int max)
        {
            Pre(label);
            bool modified = ImGui::SliderInt(GenerateID(), &value, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();

            return modified;
        }

        static bool PropertySlider(const char* label, float& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat(GenerateID(), &value, min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec2& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat2(GenerateID(), glm::value_ptr(value), min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec3& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat3(GenerateID(), glm::value_ptr(value), min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertySlider(const char* label, glm::vec4& value, float min, float max)
        {
            Pre(label);
            bool modified = ImGui::SliderFloat4(GenerateID(), glm::value_ptr(value), min, max);
            UndoRedo::Get().OnItemInteract();
            Post();
            return modified;
        }

        static bool PropertyFilepath(const char* label, FileDialogType type, String& value, const String& title = {}, const Path& initialDir = {})
        {
            ShiftCursor(10.0f, 9.0f);
            ImGui::Text("%s", label);
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

            UndoRedo::Get().OnItemInteract(modified);
            Post();

            return modified;
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, const Vector<String>& options, TEnum& selected)
        {
            TUnderlying selectedIndex = (TUnderlying)selected;
            Pre(label);
            bool modified = false;
            if (options.empty())
            {
                ImGui::TextDisabled("No options available");
                Post();
                return false;
            }
            const bool indexValid = [&]() {
                if constexpr (std::is_signed_v<TUnderlying>)
                    return selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < options.size();
                else
                    return static_cast<size_t>(selectedIndex) < options.size();
            }();
            if (!indexValid)
            {
                selectedIndex = 0;
                selected = static_cast<TEnum>(selectedIndex);
                modified = true;
            }

            String current = options[static_cast<size_t>(selectedIndex)];
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            // ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo(id.c_str(), current.c_str()))
            {
                for (size_t i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (i == static_cast<size_t>(selectedIndex));
                    if (ImGui::Selectable(options[i].c_str(), is_selected))
                    {
                        current = options[i];
                        selected = static_cast<TEnum>(i);
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            UndoRedo::Get().OnItemInteract(modified);
            Post();

            return modified;
        }

        template <typename TEnum, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, std::initializer_list<const char*> optionsList, TEnum& selected)
        {
            SmallVector<const char*, 8> options(optionsList);
            TUnderlying selectedIndex = (TUnderlying)selected;
            Pre(label);
            bool modified = false;
            if (options.empty())
            {
                ImGui::TextDisabled("No options available");
                Post();
                return false;
            }
            const bool indexValid = [&]() {
                if constexpr (std::is_signed_v<TUnderlying>)
                    return selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < options.size();
                else
                    return static_cast<size_t>(selectedIndex) < options.size();
            }();
            if (!indexValid)
            {
                selectedIndex = 0;
                selected = static_cast<TEnum>(selectedIndex);
                modified = true;
            }

            const char* current = options[static_cast<size_t>(selectedIndex)];
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            if (ImGui::BeginCombo(id.c_str(), current))
            {
                for (uint32_t i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (current == options[i]);
                    if (ImGui::Selectable(options[i], is_selected))
                    {
                        current = options[i];
                        selected = static_cast<TEnum>(i);
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            UndoRedo::Get().OnItemInteract(modified);
            Post();

            return modified;
        }

        template <typename Type, typename TUnderlying = int32_t>
        static bool PropertyDropdown(const char* label, const Vector<Type>& options, TUnderlying& selected,
                                     std::function<const String&(const Type&)> selector)
        {
            TUnderlying selectedIndex = (TUnderlying)selected;
            Pre(label);
            bool modified = false;
            if (options.empty())
            {
                ImGui::TextDisabled("No options available");
                Post();
                return false;
            }
            const bool indexValid = [&]() {
                if constexpr (std::is_signed_v<TUnderlying>)
                    return selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < options.size();
                else
                    return static_cast<size_t>(selectedIndex) < options.size();
            }();
            if (!indexValid)
            {
                selectedIndex = 0;
                selected = 0;
                modified = true;
            }

            String current = selector(options[static_cast<size_t>(selectedIndex)]);
            if ((GImGui->CurrentItemFlags & ImGuiItemFlags_MixedValue) != 0)
                current = "---";

            const String id = "##" + std::string(label);
            // ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo(id.c_str(), current.c_str()))
            {
                for (size_t i = 0; i < options.size(); i++)
                {
                    const bool is_selected = (i == static_cast<size_t>(selectedIndex));
                    const String& selectable = selector(options[i]);
                    if (ImGui::Selectable(selectable.c_str(), is_selected))
                    {
                        selected = static_cast<TUnderlying>(i);
                        modified = true;
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            UndoRedo::Get().OnItemInteract(modified);
            Post();

            return modified;
        }

    } // namespace UI

} // namespace Crowny
