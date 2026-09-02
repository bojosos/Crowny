#include "cwepch.h"

#include "Editor/EditorAssets.h"
#include "Editor/EditorDefaults.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Renderer/PrimitiveMeshLibrary.h"

#include "Crowny/Ecs/Components.h"

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{

    void PrefabOverrideContext::MarkIfModified(bool modified, const char* propertyName)
    {
        if (modified && s_ActivePrefabComponent)
            s_ActivePrefabComponent->MarkOverridden(s_ActiveComponentName + "." + propertyName);
    }

    bool PrefabOverrideContext::IsOverridden(const char* propertyName)
    {
        if (!s_ActivePrefabComponent)
            return false;
        return s_ActivePrefabComponent->IsPropertyOverridden(s_ActiveComponentName, propertyName);
    }

    UIUtils::DialogResult UIUtils::ShowYesNoMessageBox(const String& title, const String& message, MessageBoxButtons buttons)
    {
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());

            switch (buttons)
            {
            case Crowny::UIUtils::MessageBoxButtons::OK:
                if (ImGui::Button("OK"))
                {
                    return DialogResult::OK;
                }
                break;
            case Crowny::UIUtils::MessageBoxButtons::OKCancel:
                if (ImGui::Button("OK"))
                {
                    return DialogResult::OK;
                }
                if (ImGui::Button("Cancel"))
                {
                    return DialogResult::Cancel;
                }
                break;
            case Crowny::UIUtils::MessageBoxButtons::AbortRetryIgnore:
                if (ImGui::Button("Abort"))
                {
                    return DialogResult::Abort;
                }
                if (ImGui::Button("Retry"))
                {
                    return DialogResult::Retry;
                }
                if (ImGui::Button("Ignore"))
                {
                    return DialogResult::Ignore;
                }
                break;
            case Crowny::UIUtils::MessageBoxButtons::YesNoCancel:
                if (ImGui::Button("Yes"))
                {
                    return DialogResult::Yes;
                }
                if (ImGui::Button("No"))
                {
                    return DialogResult::No;
                }
                if (ImGui::Button("Cancel"))
                {
                    return DialogResult::Cancel;
                }
                break;
            case Crowny::UIUtils::MessageBoxButtons::YesNo:
                if (ImGui::Button("Yes"))
                {
                    return DialogResult::Yes;
                }
                if (ImGui::Button("No"))
                {
                    return DialogResult::No;
                }
                break;
            case Crowny::UIUtils::MessageBoxButtons::RetryCancel:
                if (ImGui::Button("Retry"))
                {
                    return DialogResult::Retry;
                }
                if (ImGui::Button("Cancel"))
                {
                    return DialogResult::Cancel;
                }
                break;
            }

            ImGui::EndPopup();
        }
        return DialogResult::Cancel;
    }

    bool UIUtils::DrawFloatControl(const char* label, float& value, float minValue, float maxValue, bool asSlider)
    {
        if (asSlider)
        {
            const bool changed = ImGui::SliderFloat(label, &value, minValue, maxValue);
            UndoRedo::Get().OnItemInteract(changed);
            return changed;
        }
        else
            return UI::Property(label, value, DRAG_SENSITIVITY, minValue, maxValue);
    }

    String UIUtils::GetAssetDisplayName(const UUID& uuid)
    {
        String name = ProjectLibrary::Get().GetAssetName(uuid);
        if (!name.empty())
            return name;
        PrimitiveMeshType primitive;
        if (PrimitiveMeshLibrary::TryGetType(uuid, primitive))
            return String(PrimitiveMeshLibrary::GetName(primitive)) + " (built-in)";
        return uuid.ToString();
    }

    bool UIUtils::SearchWidget(String& searchString, const char* hint, bool* grabFocus)
    {
        UI::PushID();
        bool modified = false;
        const float framePaddingY = ImGui::GetStyle().FramePadding.y;

        UI::ScopedStyle rounding(ImGuiStyleVar_FrameRounding, 3.0f);
        UI::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(28.0f, framePaddingY));

        if (ImGui::InputTextWithHint("##input", hint, &searchString))
            modified = true;
        else if (ImGui::IsItemDeactivatedAfterEdit())
            modified = true;

        const ImVec2 inputMin = ImGui::GetItemRectMin();
        const ImVec2 inputMax = ImGui::GetItemRectMax();

        if (grabFocus && *grabFocus)
        {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0))
            {
                ImGui::SetKeyboardFocusHere(-1);
            }

            if (ImGui::IsItemFocused())
                *grabFocus = false;
        }

        UI::DrawItemActivityOutline(3.0f, true, IM_COL32(236, 158, 36, 255));

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float iconSize = ImGui::GetTextLineHeight() - 2.0f;
        const ImVec2 iconMin(inputMin.x + 7.0f, inputMin.y + (inputMax.y - inputMin.y - iconSize) * 0.5f);
        const ImVec2 iconMax = iconMin + ImVec2(iconSize, iconSize);
        drawList->AddImage(ImGuiVulkanTexture::Get(EditorAssets::Get().SearchIcon), iconMin, iconMax, { 0.0f, 1.0f }, { 1.0f, 0.0f });

        if (!searchString.empty())
        {
            const float clearSize = inputMax.y - inputMin.y;
            const ImVec2 clearMin(inputMax.x - clearSize, inputMin.y);
            const ImVec2 clearMax(inputMax.x, inputMax.y);
            const bool clearHovered = ImGui::IsMouseHoveringRect(clearMin, clearMax);
            const ImU32 clearColor = ImGui::GetColorU32(clearHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
            const float inset = clearSize * 0.34f;
            drawList->AddLine(clearMin + ImVec2(inset, inset), clearMax - ImVec2(inset, inset), clearColor, 1.5f);
            drawList->AddLine(ImVec2(clearMin.x + inset, clearMax.y - inset), ImVec2(clearMax.x - inset, clearMin.y + inset), clearColor, 1.5f);

            if (clearHovered)
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    searchString.clear();
                    modified = true;
                }
            }
        }

        UI::PopID();
        return modified;
    }
} // namespace Crowny
