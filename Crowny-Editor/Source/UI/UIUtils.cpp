#include "cwepch.h"

#include "Editor/EditorDefaults.h"
#include "Editor/EditorAssets.h"
#include "UI/UIUtils.h"
#include "UI/Properties.h"
#include "Vendor/FontAwesome/IconsFontAwesome6.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{

    bool UIUtils::ShowYesNoMessageBox(const String& title, const String& message)
    {
        // if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            if (ImGui::Button("Yes"))
            {
                ImGui::CloseCurrentPopup();
                return true;
            }
            if (ImGui::Button("No"))
            {
                ImGui::CloseCurrentPopup();
                return false;
            }
            ImGui::EndPopup();
        }
        return false;
    }

    bool UIUtils::DrawFloatControl(const char* label, float& value, float minValue, float maxValue, bool asSlider)
    {
        if (asSlider)
            return ImGui::SliderFloat(label, &value, minValue, maxValue);
        else
            return UI::Property(label, value, DRAG_SENSITIVITY, minValue, maxValue);
    }
	
    bool UIUtils::SearchWidget(String& searchString, const char* hint , bool* grabFocus)
	{
		UI::PushID();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
		const bool layoutSuspended = []
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->DC.CurrentLayout)
			{
				ImGui::SuspendLayout();
				return true;
			}
			return false;
		}();

		bool modified = false;
		bool searching = false;

		const float areaPosX = ImGui::GetCursorPosX();
		const float framePaddingY = ImGui::GetStyle().FramePadding.y;

		UI::ScopedStyle rounding(ImGuiStyleVar_FrameRounding, 3.0f);
		UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(28.0f, framePaddingY));

		if (ImGui::InputText("##input", &searchString))
			modified = true;
		else if (ImGui::IsItemDeactivatedAfterEdit())
			modified = true;
		searching = searchString.size() != 0;

		if (grabFocus && *grabFocus)
		{
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
				&& !ImGui::IsAnyItemActive()
				&& !ImGui::IsMouseClicked(0))
			{
				ImGui::SetKeyboardFocusHere(-1);
			}

			if (ImGui::IsItemFocused())
				*grabFocus = false;
		}

		UI::DrawItemActivityOutline(3.0f, true, IM_COL32(236, 158, 36, 255));
			
		ImGui::SetItemAllowOverlap();

		ImGui::SameLine(areaPosX + 5.0f);

		if (layoutSuspended)
			ImGui::ResumeLayout();

		ImGui::BeginHorizontal("##Horizontal", ImGui::GetItemRectSize());
		const ImVec2 iconSize(ImGui::GetTextLineHeight() - 2, ImGui::GetTextLineHeight() - 2);

		// Search icon
		{
			const float iconYOffset = framePaddingY - 1.0f;
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + iconYOffset);
			// ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			// ImGui::Button(ICON_FA_MAGNIFYING_GLASS, iconSize);
			ImGui::Image(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().SearchIcon), iconSize, { 0.0f, 1.0f }, { 1.0f, 0.0f });
			// ImGui::PopFont();
			// UI::Image(s_SearchIcon, iconSize, ImVec2(0, 0), ImVec2(1, 1), ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - iconYOffset);

			// Hint
			if (!searching)
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - framePaddingY + 1.0f);
				// ScopedColor text(ImGuiCol_Text, Colours::Theme::textDarker);
				UI::ScopedColor text(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
				UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, framePaddingY));
				ImGui::TextUnformatted(hint);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
			}
		}

		ImGui::Spring();

		// Clear icon
		if (searching)
		{
			const float spacingX = 4.0f;
			const float lineHeight = ImGui::GetItemRectSize().y - framePaddingY / 2.0f;

			if (ImGui::InvisibleButton("##invisButton", ImVec2{lineHeight, lineHeight}))
			{
				searchString.clear();
				modified = true;
			}

			if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()))
				ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);

			UI::ScopedDisable();
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
			ImGui::Button(ICON_FA_XMARK, { lineHeight, lineHeight });
			ImGui::PopFont();

			/*UI::DrawButtonImage(FONT_ICON_XMAR, IM_COL32(160, 160, 160, 200),
				IM_COL32(170, 170, 170, 255),
				IM_COL32(160, 160, 160, 150),
				UI::RectExpanded(UI::GetItemRect(), -2.0f, -2.0f));*/

			ImGui::Spring(-1.0f, spacingX * 2.0f);
		}

		ImGui::EndHorizontal();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1.0f);
		UI::PopID();
		return modified;
	}
} // namespace Crowny