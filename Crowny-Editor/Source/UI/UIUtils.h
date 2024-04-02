#pragma once

#include "Crowny/Application/Application.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Editor/Editor.h"
#include "Editor/ProjectLibrary.h"

#include "Crowny/Physics/Physics2D.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/fmt/fmt.h>

namespace Crowny
{
    namespace UI
    {
        extern uint32_t s_Id;

        extern uint32_t s_Counter;
        extern char s_IDBuffer[16];
        extern char s_LabelIDBuffer[1024];

        struct ScopedStyle
        {
            ScopedStyle(const ScopedStyle&) = delete;
            ScopedStyle operator=(const ScopedStyle&) = delete;
            template <typename T> ScopedStyle(ImGuiStyleVar styleVar, const T& value)
            {
                ImGui::PushStyleVar(styleVar, value);
            }
            ~ScopedStyle() { ImGui::PopStyleVar(); }
        };

        struct ScopedColor
        {
            ScopedColor(const ScopedColor&) = delete;
            ScopedColor operator=(const ScopedColor&) = delete;
            template <typename T> ScopedColor(ImGuiCol colorVar, const T& color)
            {
                static_assert(!std::is_floating_point<T>::value);
                ImGui::PushStyleColor(colorVar, color);
            }
            ~ScopedColor() { ImGui::PopStyleColor(); }
        };

        struct ScopedDisable
        {
            ScopedDisable(bool disabled = false) { ImGui::BeginDisabled(disabled); }
            ScopedDisable(const ScopedDisable&) = delete;
            ScopedDisable operator=(const ScopedDisable&) = delete;
            ~ScopedDisable() { ImGui::EndDisabled(); }
        };

        struct ScopedFont
        {
            enum Font
            {
                Bold
            };
            ScopedFont(Font font)
            {
                switch (font)
                {
                case Bold:
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
                    break;
                default:
                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
                    break;
                }
            }
            ScopedFont(const ScopedFont&) = delete;
            ScopedFont operator=(const ScopedFont&) = delete;
            ~ScopedFont() { ImGui::PopFont(); }
        };

        static const char* GenerateID()
        {
            _itoa(s_Counter++, s_IDBuffer + 2, 16);
            return s_IDBuffer;
        }

        static const char* GenerateLabelID(const StringView label)
        {
            *fmt::format_to_n(s_LabelIDBuffer, std::size(s_LabelIDBuffer), "{}##{}", label, s_Counter++).out = 0;
            return s_LabelIDBuffer;
        }

        static void PushID()
        {
            ImGui::PushID(s_Id++);
            s_Counter = 0;
        }

        static void PopID()
        {
            ImGui::PopID();
            s_Id--;
        }

        static inline ImRect GetItemRect() { return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()); }

        static inline ImRect RectExpanded(const ImRect& rect, float x, float y)
        {
            ImRect result = rect;
            result.Min.x -= x;
            result.Min.y -= y;
            result.Max.x += x;
            result.Max.y += y;
            return result;
        }

        static inline ImRect RectOffset(const ImRect& rect, float x, float y)
        {
            ImRect result = rect;
            result.Min.x += x;
            result.Min.y += y;
            result.Max.x += x;
            result.Max.y += y;
            return result;
        }

        static inline ImRect RectOffset(const ImRect& rect, ImVec2 xy) { return RectOffset(rect, xy.x, xy.y); }

        static void DrawItemActivityOutline(float rounding = 0.0f, bool drawWhenInactive = false,
                                            ImColor colourWhenActive = ImColor(80, 80, 80))
        {
            auto* drawList = ImGui::GetWindowDrawList();
            const ImRect rect = RectExpanded(GetItemRect(), 1.0f, 1.0f);
            if (ImGui::IsItemHovered() && !ImGui::IsItemActive())
            {
                drawList->AddRect(rect.Min, rect.Max, ImColor(60, 60, 60), rounding, 0, 1.5f);
            }
            if (ImGui::IsItemActive())
            {
                drawList->AddRect(rect.Min, rect.Max, colourWhenActive, rounding, 0, 1.0f);
            }
            else if (!ImGui::IsItemHovered() && drawWhenInactive)
            {
                drawList->AddRect(rect.Min, rect.Max, ImColor(50, 50, 50), rounding, 0, 1.0f);
            }
        }

        static void ShiftCursor(float x, float y)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y);
        }

        static void ShiftCursorX(float x) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x); }

        static void ShiftCursorY(float y) { ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y); }

        static bool IsItemDisabled() { return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled; }

        static void HelpMarker(const char* desc)
        {
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(desc);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }

        static ImU32 ColourWithMultipliedValue(const ImColor& color, float multiplier)
        {
            const ImVec4& colRow = color.Value;
            float hue, sat, val;
            ImGui::ColorConvertRGBtoHSV(colRow.x, colRow.y, colRow.z, hue, sat, val);
            return ImColor::HSV(hue, sat, std::min(val * multiplier, 1.0f));
        }

        static void Underline(bool fullWidth = false, float offsetX = 0.0f, float offsetY = -1.0f)
        {
            if (fullWidth)
            {
                if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
                    ImGui::PushColumnsBackground();
                else if (ImGui::GetCurrentTable() != nullptr)
                    ImGui::TablePushBackgroundChannel();
            }

            const float width = fullWidth ? ImGui::GetWindowWidth() : ImGui::GetContentRegionAvail().x;
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(cursor.x + offsetX, cursor.y + offsetY),
                                                ImVec2(cursor.x + width, cursor.y + offsetY), IM_COL32(26, 26, 26, 255),
                                                1.0f);

            if (fullWidth)
            {
                if (ImGui::GetCurrentWindow()->DC.CurrentColumns != nullptr)
                    ImGui::PopColumnsBackground();
                else if (ImGui::GetCurrentTable() != nullptr)
                    ImGui::TablePopBackgroundChannel();
            }
        }

        static void DrawButtonImage(const Ref<Texture>& imageNormal, const Ref<Texture>& imageHovered,
                                    const Ref<Texture>& imagePressed, ImU32 tintNormal, ImU32 tintHovered,
                                    ImU32 tintPressed, ImVec2 rectMin, ImVec2 rectMax)
        {
            auto* drawList = ImGui::GetWindowDrawList();
            if (ImGui::IsItemActive())
                drawList->AddImage(ImGui_ImplVulkan_AddTexture(imagePressed), rectMin, rectMax, ImVec2(0, 1),
                                   ImVec2(1, 0), tintPressed);
            else if (ImGui::IsItemHovered())
                drawList->AddImage(ImGui_ImplVulkan_AddTexture(imageHovered), rectMin, rectMax, ImVec2(0, 1),
                                   ImVec2(1, 0), tintHovered);
            else
                drawList->AddImage(ImGui_ImplVulkan_AddTexture(imageNormal), rectMin, rectMax, ImVec2(0, 1),
                                   ImVec2(1, 0), tintNormal);
        }

        static void DrawButtonImage(const Ref<Texture>& image, ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed,
                                    ImRect rectangle)
        {
            DrawButtonImage(image, image, image, tintNormal, tintHovered, tintPressed, rectangle.Min, rectangle.Max);
        }

        static bool IsItemHovered(float delayInSeconds = 0.1f, ImGuiHoveredFlags flags = 0)
        {
            return ImGui::IsItemHovered() && GImGui->HoveredIdTimer > delayInSeconds;
        }

        static void SetTooltip(const StringView text, float delayInSeconds = 0.02f, bool allowWhenDisabled = true,
                               ImVec2 padding = ImVec2(5, 5))
        {
            if (IsItemHovered(delayInSeconds, allowWhenDisabled ? ImGuiHoveredFlags_AllowWhenDisabled : 0))
            {
                ScopedStyle tooltipPadding(ImGuiStyleVar_WindowPadding, padding);
                ScopedColor textCol(ImGuiCol_Text, IM_COL32(210, 210, 210, 255));
                ImGui::SetTooltip(text.data());
            }
        }

        static void BeginPropertyGrid()
        {
            PushID();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
            ImGui::Columns(2);
        }

        static void EndPropertyGrid()
        {
            ImGui::Columns(1);
            Underline();
            ImGui::PopStyleVar(2); // ItemSpacing, FramePadding
            ShiftCursorY(18.0f);
            PopID();
        }

        static bool InputUInt32(const char* label, uint32_t* v, uint32_t step = 1, uint32_t step_fast = 100,
                                ImGuiInputTextFlags flags = 0)
        {
            return ImGui::InputScalar(label, ImGuiDataType_U32, v, &step, &step_fast, "%d", flags);
        }

        static bool DragFloat(const char* label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f,
                              const char* format = "%.3f", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_Float, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragDouble(const char* label, double* v, float v_speed = 1.0f, double v_min = 0.0,
                               double v_max = 0.0, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_Double, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragInt8(const char* label, int8_t* v, float v_speed = 1.0f, int8_t v_min = 0, int8_t v_max = 0,
                             const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_S8, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragInt16(const char* label, int16_t* v, float v_speed = 1.0f, int16_t v_min = 0, int16_t v_max = 0,
                              const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_S16, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragInt32(const char* label, int32_t* v, float v_speed = 1.0f, int32_t v_min = 0, int32_t v_max = 0,
                              const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_S32, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragInt64(const char* label, int64_t* v, float v_speed = 1.0f, int64_t v_min = 0, int64_t v_max = 0,
                              const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_S64, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragUInt8(const char* label, uint8_t* v, float v_speed = 1.0f, uint8_t v_min = 0, uint8_t v_max = 0,
                              const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_U8, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragUInt16(const char* label, uint16_t* v, float v_speed = 1.0f, uint16_t v_min = 0,
                               uint16_t v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_U16, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragUInt32(const char* label, uint32_t* v, float v_speed = 1.0f, uint32_t v_min = 0,
                               uint32_t v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_U32, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool DragUInt64(const char* label, uint64_t* v, float v_speed = 1.0f, uint64_t v_min = 0,
                               uint64_t v_max = 0, const char* format = "%d", ImGuiSliderFlags flags = 0)
        {
            return ImGui::DragScalar(label, ImGuiDataType_U64, v, v_speed, &v_min, &v_max, format, flags);
        }

        static bool InputInt8(const char* label, int8_t* v, int8_t step = 1, int8_t step_fast = 1,
                              ImGuiInputTextFlags flags = 0)
        {
            return ImGui::InputScalar(label, ImGuiDataType_S8, v, &step, &step_fast, "%d", flags);
        }

        static bool InputInt16(const char* label, int16_t* v, int16_t step = 1, int16_t step_fast = 10,
                               ImGuiInputTextFlags flags = 0)
        {
            return ImGui::InputScalar(label, ImGuiDataType_S16, v, &step, &step_fast, "%d", flags);
        }

        static bool InputInt32(const char* label, int32_t* v, int32_t step = 1, int32_t step_fast = 100,
                               ImGuiInputTextFlags flags = 0)
        {
            return ImGui::InputScalar(label, ImGuiDataType_S32, v, &step, &step_fast, "%d", flags);
        }

        static bool InputInt64(const char* label, int64_t* v, int64_t step = 1, int64_t step_fast = 1000,
                               ImGuiInputTextFlags flags = 0)
        {
            return ImGui::InputScalar(label, ImGuiDataType_S64, v, &step, &step_fast, "%d", flags);
        }
    } // namespace UI

    class UIUtils
    {
    public:
        enum class MessageBoxButtons
        {
            None = -1, // Utility value used the check if MessageBox should be drawn.
            OK = 0,
            OKCancel = 1,
            AbortRetryIgnore = 2,
            YesNoCancel = 3,
            YesNo = 4,
            RetryCancel = 5
        };

        enum class DialogResult
        {
            OK = 0,
            Cancel = 1,
            Abort = 2,
            Retry = 3,
            Ignore = 4,
            Yes = 5,
            No = 6,
        };

        static DialogResult ShowYesNoMessageBox(const String& title, const String& message, MessageBoxButtons buttons);

        static bool BeginPopup(const char* str_id, ImGuiWindowFlags flags = 0)
        {
            bool opened = false;
            if (ImGui::BeginPopup(str_id, flags))
            {
                opened = true;
                // Fill background with nice gradient
                const float padding = ImGui::GetStyle().WindowBorderSize;
                const ImRect windowRect = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), -padding, -padding);
                ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
                const ImColor col1 = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg); // Colours::Theme::backgroundPopup;
                const ImColor col2 = UI::ColourWithMultipliedValue(col1, 0.8f);
                ImGui::GetWindowDrawList()->AddRectFilledMultiColor(windowRect.Min, windowRect.Max, col1, col1, col2,
                                                                    col2);
                ImGui::GetWindowDrawList()->AddRect(windowRect.Min, windowRect.Max,
                                                    UI::ColourWithMultipliedValue(col1, 1.1f));
                ImGui::PopClipRect();

                // Popped in EndPopup()
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 80));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
            }

            return opened;
        }

        static bool BeginPopupModal(const char* str_id, ImGuiWindowFlags flags = 0)
        {
            bool open = false;
            if (ImGui::BeginPopupModal(str_id))
            {
                open = true;
                const float padding = ImGui::GetStyle().WindowBorderSize;
                const ImRect windowRect = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), -padding, -padding);
                ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
                const ImColor col1 = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
                const ImColor col2 = UI::ColourWithMultipliedValue(col1, 0.8f);
                ImGui::GetWindowDrawList()->AddRectFilledMultiColor(windowRect.Min, windowRect.Max, col1, col1, col2,
                                                                    col2);
                ImGui::GetWindowDrawList()->AddRect(windowRect.Min, windowRect.Max,
                                                    UI::ColourWithMultipliedValue(col1, 1.1f));
                ImGui::PopClipRect();

                // Popped in EndPopup()
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(0, 0, 0, 80));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
            }
            return open;
        }

        static void EndPopup()
        {
            ImGui::PopStyleVar();   // WindowPadding;
            ImGui::PopStyleColor(); // HeaderHovered;
            ImGui::EndPopup();
        }

        static bool SearchWidget(String& searchString, const char* hint = "Search...", bool* grabFocus = nullptr);

        static bool ScriptSearchPopup(const String& id, String& selectedScript, bool* cleared = nullptr,
                                      const char* hint = "Search Entities",
                                      const ImVec2& size = ImVec2{ 250.0f, 350.0f })
        {
            UI::ScopedColor popupBG(ImGuiCol_PopupBg, IM_COL32(36 * 1.6f, 36 * 1.6f, 36 * 1.6f, 255));

            bool modified = false;

            String preview;
            float itemHeight = size.y / 20.0f;

            const auto view = SceneManager::GetActiveScene()->GetAllEntitiesWith<TagComponent>();
            String current = selectedScript;

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            ImGui::SetNextWindowSize({ size.x, 0.0f });

            static bool grabFocus = true;

            if (BeginPopup(id.c_str(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                static String searchString;

                if (ImGui::GetCurrentWindow()->Appearing)
                {
                    grabFocus = true;
                    searchString.clear();
                }

                // Search widget
                ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 3.0f, ImGui::GetCursorPosY() + 2.0f });
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
                SearchWidget(searchString, hint, &grabFocus);

                const bool searching = !searchString.empty();

                if (cleared != nullptr)
                {
                    UI::ScopedColor buttonColor1(ImGuiCol_Button,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.0f));
                    UI::ScopedColor buttonColor2(ImGuiCol_ButtonHovered,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.2f));
                    UI::ScopedColor buttonColor3(ImGuiCol_ButtonActive,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 0.9f));

                    UI::ScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);

                    ImGui::SetCursorPosX(0);

                    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, searching);

                    if (ImGui::Button("CLEAR", { ImGui::GetWindowWidth(), 0.0f }))
                    {
                        *cleared = true;
                        modified = true;
                    }

                    ImGui::PopItemFlag();
                }

                // List of assets
                {
                    UI::ScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
                    UI::ScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

                    ImGuiID listID = ImGui::GetID("##SearchListBox");
                    // if (ImGui::BeginChild(listID, ImVec2(-FLT_MIN, itemHeight * 10.0f), false))
                    if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
                    {
                        bool forwardFocus = false;

                        ImGuiContext& g = *GImGui;
                        if (g.NavJustMovedToId != 0)
                        {
                            if (g.NavJustMovedToId == listID)
                            {
                                forwardFocus = true;
                                // ActivateItem moves keyboard navigation focus inside of the window
                                ImGui::ActivateItem(listID);
                                ImGui::SetKeyboardFocusHere(1);
                            }
                        }

                        const auto& classes = ScriptInfoManager::Get().GetEntityBehaviours();
                        for (auto [name, klass] : classes)
                        {
                            if (klass->GetFullName() ==
                                ScriptInfoManager::Get().GetBuiltinClasses().EntityBehaviour->GetFullName())
                                continue;
                            if (!searchString.empty() && !StringUtils::IsSearchMathing(name, searchString))
                                continue;

                            bool isSelected = (current == name);
                            if (ImGui::Selectable(name.c_str(), isSelected))
                            {
                                current = name;
                                selectedScript = name;
                                modified = true;
                            }

                            if (forwardFocus)
                                forwardFocus = false;
                            else if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndListBox();
                    }
                }
                if (modified)
                    ImGui::CloseCurrentPopup();

                EndPopup();
            }

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PopStyleVar();

            return modified;
        }

        static bool PropertyLayer(const String& label, uint32_t& selectedLayer)
        {
            bool modified = false;

            ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 10.0f, ImGui::GetCursorPosY() + 9.0f });
            ImGui::Text(label.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::PushItemWidth(-1);

            ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
            {
                ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
                float width = ImGui::GetContentRegionAvail().x;
                float itemHeight = 28.0f;

                String buttonText = Physics2D::Get().GetLayerName(selectedLayer);
                String layerSearchPopupId = UI::GenerateLabelID("EntitySearch");
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
                if (ImGui::Button(UI::GenerateLabelID(buttonText), { width, itemHeight }))
                    ImGui::OpenPopup(layerSearchPopupId.c_str());
                ImGui::PopStyleColor();
                ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

                if (LayerSearchPopup(layerSearchPopupId, selectedLayer))
                    modified = true;
            }

            if (!UI::IsItemDisabled())
                UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            UI::Underline();

            return modified;
        }

        static bool LayerSearchPopup(const String& id, uint32_t& selectedLayerMask, const char* hint = "Layer",
                                     const ImVec2& size = ImVec2{ 250.0f, 350.0f })
        {
            UI::ScopedColor popupBG(ImGuiCol_PopupBg, IM_COL32(36 * 1.6f, 36 * 1.6f, 36 * 1.6f, 255));

            bool modified = false;

            String preview;
            float itemHeight = size.y / 20.0f;

            const auto view = SceneManager::GetActiveScene()->GetAllEntitiesWith<TagComponent>();
            uint32_t current = selectedLayerMask;

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            ImGui::SetNextWindowSize({ size.x, 0.0f });

            static bool grabFocus = true;

            if (BeginPopup(id.c_str(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                static String searchString;

                if (ImGui::GetCurrentWindow()->Appearing)
                {
                    grabFocus = true;
                    searchString.clear();
                }

                // Search widget
                ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 3.0f, ImGui::GetCursorPosY() + 2.0f });
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
                SearchWidget(searchString, hint, &grabFocus);

                const bool searching = !searchString.empty();

                {
                    UI::ScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
                    UI::ScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

                    ImGuiID listID = ImGui::GetID("##SearchListBox");
                    if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
                    {
                        bool forwardFocus = false;

                        ImGuiContext& g = *GImGui;
                        if (g.NavJustMovedToId != 0)
                        {
                            if (g.NavJustMovedToId == listID)
                            {
                                forwardFocus = true;
                                // ActivateItem moves keyboard navigation focus inside of the window
                                ImGui::ActivateItem(listID);
                                ImGui::SetKeyboardFocusHere(1);
                            }
                        }

                        for (uint32_t i = 0; i < 32; i++)
                        {
                            const String& layerName = Physics2D::Get().GetLayerName(i);
                            if (layerName.empty() ||
                                !searchString.empty() && !StringUtils::IsSearchMathing(layerName, searchString))
                                continue;

                            bool isSelected = (current == i);
                            if (ImGui::Selectable(layerName.c_str(), isSelected))
                            {
                                current = i;
                                selectedLayerMask = i;
                                modified = true;
                            }

                            if (forwardFocus)
                                forwardFocus = false;
                            else if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }

                        ImGui::EndListBox();
                    }
                }
                if (modified)
                    ImGui::CloseCurrentPopup();

                EndPopup();
            }

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PopStyleVar();

            return modified;
        }

        static bool EntitySearchPopup(const String& id, Entity& selectedEntity, bool* cleared = nullptr,
                                      const char* hint = "Search Entities",
                                      const ImVec2& size = ImVec2{ 250.0f, 350.0f })
        {
            UI::ScopedColor popupBG(ImGuiCol_PopupBg, IM_COL32(36 * 1.6f, 36 * 1.6f, 36 * 1.6f, 255));

            bool modified = false;

            // String preview;
            // float itemHeight = size.y / 20.0f;

            const auto view = SceneManager::GetActiveScene()->GetAllEntitiesWith<TagComponent>();
            Entity current = selectedEntity;

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            ImGui::SetNextWindowSize({ size.x, 0.0f });

            static bool grabFocus = true;

            if (BeginPopup(id.c_str(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                static String searchString;

                if (ImGui::GetCurrentWindow()->Appearing)
                {
                    grabFocus = true;
                    searchString.clear();
                }

                // Search widget
                ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 3.0f, ImGui::GetCursorPosY() + 2.0f });
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
                SearchWidget(searchString, hint, &grabFocus);

                const bool searching = !searchString.empty();

                if (cleared != nullptr)
                {
                    UI::ScopedColor buttonColor1(ImGuiCol_Button,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.0f));
                    UI::ScopedColor buttonColor2(ImGuiCol_ButtonHovered,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.2f));
                    UI::ScopedColor buttonColor3(ImGuiCol_ButtonActive,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 0.9f));

                    UI::ScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);

                    ImGui::SetCursorPosX(0);

                    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, searching);

                    if (ImGui::Button("CLEAR", { ImGui::GetWindowWidth(), 0.0f }))
                    {
                        *cleared = true;
                        modified = true;
                    }

                    ImGui::PopItemFlag();
                }

                // List of assets
                {
                    UI::ScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
                    UI::ScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

                    ImGuiID listID = ImGui::GetID("##SearchListBox");
                    // if (ImGui::BeginChild(listID, ImVec2(-FLT_MIN, itemHeight * 10.0f), false))
                    if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
                    {
                        bool forwardFocus = false;

                        ImGuiContext& g = *GImGui;
                        if (g.NavJustMovedToId != 0)
                        {
                            if (g.NavJustMovedToId == listID)
                            {
                                forwardFocus = true;
                                // ActivateItem moves keyboard navigation focus inside of the window
                                ImGui::ActivateItem(listID);
                                ImGui::SetKeyboardFocusHere(1);
                            }
                        }

                        Scene* scene = SceneManager::GetActiveScene().get();
                        for (auto e : view)
                        {
                            Entity entity = { e, scene };

                            const String& entityName = entity.GetName();
                            if (!searchString.empty() && !StringUtils::IsSearchMathing(entityName, searchString))
                                continue;

                            bool isSelected = (current == entity);
                            if (ImGui::Selectable(entityName.c_str(), isSelected))
                            {
                                current = entity;
                                selectedEntity = entity;
                                modified = true;
                            }

                            if (forwardFocus)
                                forwardFocus = false;
                            else if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }

                        // ImGui::EndChild();
                        ImGui::EndListBox();
                    }
                }
                if (modified)
                    ImGui::CloseCurrentPopup();

                EndPopup();
            }

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PopStyleVar();

            return modified;
        }

        static bool AssetSearchPopup(const String& id, AssetType assetType, AssetHandle<Asset>& assetHandle,
                                     bool* cleared = nullptr, const char* hint = "Search Entities",
                                     const ImVec2& size = ImVec2{ 250.0f, 350.0f })
        {
            UI::ScopedColor popupBG(ImGuiCol_PopupBg, IM_COL32(36 * 1.6f, 36 * 1.6f, 36 * 1.6f, 255));

            bool modified = false;

            String preview;
            float itemHeight = size.y / 20.0f;

            const auto view = SceneManager::GetActiveScene()->GetAllEntitiesWith<TagComponent>();
            AssetHandle<Asset> current = assetHandle;

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

            ImGui::SetNextWindowSize({ size.x, 0.0f });

            static bool grabFocus = true;

            if (BeginPopup(id.c_str(), ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
            {
                static String searchString;

                if (ImGui::GetCurrentWindow()->Appearing)
                {
                    grabFocus = true;
                    searchString.clear();
                }

                // Search widget
                ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 3.0f, ImGui::GetCursorPosY() + 2.0f });
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() * 2.0f);
                SearchWidget(searchString, hint, &grabFocus);

                const bool searching = !searchString.empty();

                if (cleared != nullptr)
                {
                    UI::ScopedColor buttonColor1(ImGuiCol_Button,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.0f));
                    UI::ScopedColor buttonColor2(ImGuiCol_ButtonHovered,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 1.2f));
                    UI::ScopedColor buttonColor3(ImGuiCol_ButtonActive,
                                                 UI::ColourWithMultipliedValue(IM_COL32(36, 36, 36, 255), 0.9f));

                    UI::ScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);

                    ImGui::SetCursorPosX(0);

                    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, searching);

                    if (ImGui::Button("CLEAR", { ImGui::GetWindowWidth(), 0.0f }))
                    {
                        *cleared = true;
                        modified = true;
                    }

                    ImGui::PopItemFlag();
                }

                // List of assets
                {
                    UI::ScopedColor listBoxBg(ImGuiCol_FrameBg, IM_COL32_DISABLE);
                    UI::ScopedColor listBoxBorder(ImGuiCol_Border, IM_COL32_DISABLE);

                    ImGuiID listID = ImGui::GetID("##SearchListBox");
                    if (ImGui::BeginListBox("##SearchListBox", ImVec2(-FLT_MIN, 0.0f)))
                    {
                        bool forwardFocus = false;

                        ImGuiContext& g = *GImGui;
                        if (g.NavJustMovedToId != 0)
                        {
                            if (g.NavJustMovedToId == listID)
                            {
                                forwardFocus = true;
                                // ActivateItem moves keyboard navigation focus inside of the window
                                ImGui::ActivateItem(listID);
                                ImGui::SetKeyboardFocusHere(1);
                            }
                        }

                        Vector<UUID> assets = ProjectLibrary::Get().GetAllAssets(assetType);
                        for (const auto& uuid : assets)
                        {
                            // TODO: This shouldn't load
                            AssetHandle<Asset> handle = AssetManager::Get().LoadFromUUID(uuid);
                            const String& assetName = handle->GetName();
                            if (!searchString.empty() && !StringUtils::IsSearchMathing(assetName, searchString))
                                continue;

                            bool isSelected = current && (current->GetName() == handle->GetName());
                            if (ImGui::Selectable(assetName.c_str(), isSelected))
                            {
                                current = handle;
                                assetHandle = handle;
                                modified = true;
                            }

                            if (forwardFocus)
                                forwardFocus = false;
                            else if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }

                        // ImGui::EndChild();
                        ImGui::EndListBox();
                    }
                }
                if (modified)
                    ImGui::CloseCurrentPopup();

                EndPopup();
            }

            if (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled)
                ImGui::PopStyleVar();

            return modified;
        }

        static bool EntityReference(const String& label, Entity& entity)
        {
            bool modified = false;

            ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 10.0f, ImGui::GetCursorPosY() + 9.0f });
            ImGui::Text(label.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::PushItemWidth(-1);

            ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
            {
                ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
                float width = ImGui::GetContentRegionAvail().x;
                float itemHeight = 28.0f;

                String buttonText = "Null";
                if (entity)
                    buttonText = entity.GetName();
                String entitySearchPopupId = UI::GenerateLabelID("EntitySearch");
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
                if (ImGui::Button(UI::GenerateLabelID(buttonText), { width, itemHeight }))
                    ImGui::OpenPopup(entitySearchPopupId.c_str());
                ImGui::PopStyleColor();
                ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

                bool clear = false;
                if (EntitySearchPopup(entitySearchPopupId, entity, &clear))
                {
                    if (clear)
                        entity = { entt::null, nullptr };
                    modified = true;
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* data = AcceptEntityPayload())
                {
                    entity = GetEntityFromPayload(data);
                    modified = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (!UI::IsItemDisabled())
                UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            UI::Underline();

            return modified;
        }

        template <typename AssetType>
        static bool AssetReference(const String& label, AssetHandle<AssetType>& assetHandle)
        {
            bool modified = false;

            ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 10.0f, ImGui::GetCursorPosY() + 9.0f });
            ImGui::Text(label.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::PushItemWidth(-1);

            ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
            {
                ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
                float width = ImGui::GetContentRegionAvail().x;
                float itemHeight = 28.0f;

                String buttonText = "Null";
                if (assetHandle)
                    buttonText = assetHandle->GetName();
                String entitySearchPopupId = UI::GenerateLabelID("AssetSearch");
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
                if (ImGui::Button(UI::GenerateLabelID(buttonText), { width, itemHeight }))
                    ImGui::OpenPopup(entitySearchPopupId.c_str());
                ImGui::PopStyleColor();
                ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

                bool clear = false;
                AssetHandle<Asset> asset = static_asset_cast<Asset>(assetHandle);
                if (AssetSearchPopup(entitySearchPopupId, AssetType::GetStaticType(), asset, &clear))
                {
                    if (clear)
                        assetHandle = AssetHandle<AssetType>();
                    else
                        assetHandle = static_asset_cast<AssetType>(asset);
                    modified = true;
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* data = AcceptAssetPayload())
                {
                    Path path = GetPathFromPayload(data);
                    if (ProjectLibrary::Get().GetAssetType(path) == AssetType::GetStaticType())
                        assetHandle = static_asset_cast<AssetType>(ProjectLibrary::Get().Load(path));
                    modified = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (!UI::IsItemDisabled())
                UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            UI::Underline();

            return modified;
        }

        static bool AssetReference(const String& label, AssetHandle<Asset>& assetHandle, AssetType assetType)
        {
            bool modified = false;

            ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 10.0f, ImGui::GetCursorPosY() + 9.0f });
            ImGui::Text(label.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::PushItemWidth(-1);

            ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
            {
                ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
                float width = ImGui::GetContentRegionAvail().x;
                float itemHeight = 28.0f;

                String buttonText = "Null";
                if (assetHandle)
                    buttonText = assetHandle->GetName();
                String entitySearchPopupId = UI::GenerateLabelID("AssetSearch");
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
                if (ImGui::Button(UI::GenerateLabelID(buttonText), { width, itemHeight }))
                    ImGui::OpenPopup(entitySearchPopupId.c_str());
                ImGui::PopStyleColor();
                ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

                bool clear = false;
                if (AssetSearchPopup(entitySearchPopupId, assetType, assetHandle, &clear))
                {
                    if (clear)
                        assetHandle = AssetHandle<Asset>();
                    modified = true;
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* data = AcceptAssetPayload())
                {
                    Path path = GetPathFromPayload(data);
                    if (ProjectLibrary::Get().GetAssetType(path) == assetType)
                        assetHandle = static_asset_cast<AssetType>(ProjectLibrary::Get().Load(path));
                    modified = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (!UI::IsItemDisabled())
                UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            UI::Underline();

            return modified;
        }

        static bool PropertyScript(const String& label, String& name)
        {
            bool modified = false;

            ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 10.0f, ImGui::GetCursorPosY() + 9.0f });
            ImGui::Text(label.c_str());
            ImGui::NextColumn();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
            ImGui::PushItemWidth(-1);

            ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
            {
                ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
                float width = ImGui::GetContentRegionAvail().x;
                float itemHeight = 28.0f;

                String buttonText = "Null";
                if (!name.empty())
                    buttonText = name;
                String scriptSearchPopupId = UI::GenerateLabelID("ScriptSearch");
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(192, 192, 192, 255));
                if (ImGui::Button(UI::GenerateLabelID(buttonText), { width, itemHeight }))
                    ImGui::OpenPopup(scriptSearchPopupId.c_str());
                ImGui::PopStyleColor();
                ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

                bool clear = false;
                if (ScriptSearchPopup(scriptSearchPopupId, name, &clear))
                {
                    if (clear)
                        name = "";
                    modified = true;
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* data = AcceptAssetPayload())
                {
                    uint32_t id =
                      *(const uint32_t*)data->Data; // Here I would need to get the class name from the script
                    // entity = { (entt::entity)id, SceneManager::GetActiveScene().get() };
                    modified = true;
                }
                ImGui::EndDragDropTarget();
            }

            if (!UI::IsItemDisabled())
                UI::DrawItemActivityOutline(2.0f, true, IM_COL32(236, 158, 36, 255));

            ImGui::PopItemWidth();
            ImGui::NextColumn();
            UI::Underline();

            return modified;
        }

        static Entity GetEntityFromPayload(const ImGuiPayload* payload)
        {
            CW_ENGINE_ASSERT(payload->DataSize == sizeof(uint32_t));
            uint32_t id = *(const uint32_t*)payload->Data;
            Entity result{ (entt::entity)id, SceneManager::GetActiveScene().get() };
            return result;
        }

        static Path GetPathFromPayload(const ImGuiPayload* payload)
        {
            String path((const char*)payload->Data, payload->DataSize);
            return Path(path);
        }

        static const ImGuiPayload* AcceptEntityPayload() { return ImGui::AcceptDragDropPayload("Entity_ID"); }

        static const ImGuiPayload* AcceptAssetPayload() { return ImGui::AcceptDragDropPayload("ASSET_ITEM"); }

        static void SetEntityPayload(Entity entity)
        {
            uint32_t tmp = (uint32_t)entity.GetHandle();
            ImGui::SetDragDropPayload("Entity_ID", &tmp, sizeof(uint32_t));
        }

        static void SetAssetPayload(const Path& path)
        {
            String str = path.string();
            const char* itemPath = str.c_str(); // Safe since imgui does a memcpy here.
            ImGui::SetDragDropPayload("ASSET_ITEM", itemPath, str.size() * sizeof(char));
        }

        static bool DrawFloatControl(const char* label, float& value, float minValue = 0.0f, float maxValue = 1.0f,
                                     bool asSlider = false);

    private:
    };

} // namespace Crowny