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

        void SetMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level level, bool enabled) { m_EnabledLevels[(uint32_t)level] = enabled; }
		bool IsMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level level) const { return m_EnabledLevels[(uint32_t)level]; }

		void SetCollapseEnabled(bool collapse) { m_Collapse = collapse; }
		void SetScrollToBottomEnabled(bool scroll) { m_AllowScrollingToBottom = scroll; }

		bool IsCollapseEnabled() const { return m_Collapse; }
	    bool IsScrollToBottomEnabled() const { return m_AllowScrollingToBottom; }


    private:
        Vector<uint32_t> m_MessageIndices;
        size_t m_SelectedMessageHash;
        float m_DisplayScale = 1.0f;

        bool m_EnabledLevels[5] = { true, true, true, true, true };
        bool m_Collapse = false;
        bool m_AllowScrollingToBottom = true;
        bool m_RequestScrollToBottom = false;
    };

} // namespace Crowny