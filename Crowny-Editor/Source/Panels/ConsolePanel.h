#pragma once

#include "Crowny/ImGui/ImGuiConsoleBuffer.h"
#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{
    class ConsolePanel : public ImGuiPanel
    {
    public:
        ConsolePanel(const String& name);
        ~ConsolePanel() = default;

        virtual void Render() override;
        void RenderMessages();
        void RenderHeader();
        void RenderSettings();
        void RenderMessage(const Ref<ImGuiConsoleBuffer::Message>& message);

        static glm::vec4 GetRenderColor(ImGuiConsoleBuffer::Message::Level level)
        {
            switch (level)
            {
            case ImGuiConsoleBuffer::Message::Level::Info:
                return { 0.00f, 0.50f, 0.00f, 1.00f }; // Green
            case ImGuiConsoleBuffer::Message::Level::Warn:
                return { 1.00f, 1.00f, 0.00f, 1.00f }; // Yellow
            case ImGuiConsoleBuffer::Message::Level::Error:
                return { 1.00f, 0.00f, 0.00f, 1.00f }; // Red
            case ImGuiConsoleBuffer::Message::Level::Critical:
                return { 1.00f, 0.00f, 0.00f, 1.00f }; // White-white
            default:
                return { 0.00f, 0.00f, 0.00f, 1.00f }; // Stupid warnings
            }
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        }

    private:
        Ref<ImGuiConsoleBuffer::Message> m_SelectedMessage;
        bool m_EnabledLevels[5] = { true, true, true, true, true };
        ImGuiConsoleBuffer::Message::Level m_MessageBufferRenderFilter = ImGuiConsoleBuffer::Message::Level::Info;
        float m_DisplayScale = 1.0f;
        bool m_AllowScrollingToBottom = true;
        bool m_RequestScrollToBottom = false;
    };

} // namespace Crowny