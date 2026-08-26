#pragma once

#include "Crowny/Common/ConsoleBuffer.h"
#include "Panels/ConsoleViewModel.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

#include <limits>

namespace Crowny
{
    class ConsolePanel : public ImGuiPanel
    {
    public:
        inline static constexpr EditorPanelRegistration<ConsolePanel> Registration{ "Console", "View/Console" };

        ConsolePanel(const String& name);
        ~ConsolePanel() = default;

        virtual void Render() override;
        void RenderMessages();
        void RenderHeader();
        void RenderSettings();
        void RenderMessage(const ConsoleBuffer::Message& message);
        void RenderFooter();
        bool MatchesSearch(const ConsoleBuffer::Message& message) const;
        void CopySelectedMessage() const;
        void RefreshMessages();
        void RebuildSearchTerms();
        void RebuildFilteredIndices();

        static glm::vec4 GetRenderColor(ConsoleBuffer::Message::Level level)
        {
            switch (level)
            {
            case ConsoleBuffer::Message::Level::Info:
                return { 0.68f, 0.78f, 0.88f, 1.00f };
            case ConsoleBuffer::Message::Level::Warn:
                return { 0.95f, 0.68f, 0.20f, 1.00f };
            case ConsoleBuffer::Message::Level::Error:
                return { 0.95f, 0.35f, 0.30f, 1.00f };
            case ConsoleBuffer::Message::Level::Critical:
                return { 1.00f, 0.20f, 0.45f, 1.00f };
            default:
                return { 1.00f, 1.00f, 1.00f, 1.00f };
            }
        }

        void SetMessageLevelEnabled(ConsoleBuffer::Message::Level level, bool enabled);
        bool IsMessageLevelEnabled(ConsoleBuffer::Message::Level level) const { return m_EnabledLevels[(uint32_t)level]; }

        void SetCollapseEnabled(bool collapse);
        void SetScrollToBottomEnabled(bool scroll) { m_AllowScrollingToBottom = scroll; }

        bool IsCollapseEnabled() const { return m_Collapse; }
        bool IsScrollToBottomEnabled() const { return m_AllowScrollingToBottom; }

    private:
        Vector<uint32_t> m_MessageIndices;
        Vector<ConsoleBuffer::Message> m_MessageSnapshot;
        ConsoleBuffer::SearchQuery m_SearchQuery;
        uint64_t m_MessageRevision = std::numeric_limits<uint64_t>::max();
        uint64_t m_SelectedMessageId = 0;
        String m_SearchString;
        float m_DisplayScale = 1.0f;

        bool m_EnabledLevels[4] = { true, true, true, true };
        bool m_Collapse = false;
        bool m_AllowScrollingToBottom = true;
        bool m_RequestScrollToBottom = false;
        bool m_FilterDirty = true;
        float m_MessageHeight = 0.0f;
        ConsoleBuffer::Message m_SelectedMessage;
        ConsoleViewModel m_ViewModel;
    };

} // namespace Crowny
