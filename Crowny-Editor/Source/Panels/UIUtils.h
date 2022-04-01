#pragma once

#include "Crowny/Application/Application.h"

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Scene/SceneManager.h"

#include <imgui.h>

namespace Crowny
{

    class UIUtils
    {
    public:
        /**
         * @brief A simple Yes/No message box. You have to call ImGui::OpenPopup with the title.
         *
         * @param title Title and also Id that should be used in ImGui::OpenPopup
         * @param message
         * @return true
         * @return false
         */
        static bool ShowYesNoMessageBox(const String& title, const String& message);

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
            const char* itemPath = str.c_str();
            ImGui::SetDragDropPayload("ASSET_ITEM", itemPath, str.size() * sizeof(char));
        }

        static void PushFontAwesomeFont()
        {
            ImGui::PushFont(Application::Get().GetImGuiLayer()->GetFontAwesomeFont());
        }			

        static void PopFontAwesomeFont()
        {
			ImGui::PopFont();
        }

        struct ScopedFontAwesomeFont
        {
			ScopedFontAwesomeFont() { PushFontAwesomeFont(); }
			~ScopedFontAwesomeFont() { PopFontAwesomeFont(); }
        };

        struct ScopedDisable
        {
            ScopedDisable(bool disabled) : m_Disable(disabled)
            {
                if (m_Disable)
                    ImGui::BeginDisabled();
            }
            ~ScopedDisable()
            {
                if (m_Disable)
                    ImGui::EndDisabled();
            }
            bool m_Disable;
        };

        static bool DrawFloatControl(float& value, float minValue = 0.0f, float maxValue = 1.0f, bool asSlider = false);
    };

} // namespace Crowny