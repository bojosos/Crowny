#pragma once

#include "Crowny/ImGui/ImGuiConsoleBuffer.h"
#include "Panels/ImGuiPanel.h"

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
        void RenderMessage(const ImGuiConsoleBuffer::Message& message);

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
        size_t m_SelectedMessageHash;
        ImGuiConsoleBuffer::Message::Level m_MessageBufferRenderFilter = ImGuiConsoleBuffer::Message::Level::Info;
        float m_DisplayScale = 1.0f;
        
        bool m_EnabledLevels[5] = { true, true, true, true, true };
        bool m_Collapse;
        bool m_AllowScrollingToBottom = true;
        bool m_RequestScrollToBottom = false;
    };

} // namespace Crowny