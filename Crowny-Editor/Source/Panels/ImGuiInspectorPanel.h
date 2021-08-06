#pragma once

#include "ImGuiComponentEditor.h"
#include "ImGuiPanel.h"

namespace Crowny
{
    class Entity;

    class ImGuiInspectorPanel : public ImGuiPanel
    {
    public:
        ImGuiInspectorPanel(const std::string& name);
        ~ImGuiInspectorPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

    private:
        ImGuiComponentEditor m_ComponentEditor;
    };
} // namespace Crowny