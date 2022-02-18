#pragma once

#include "Panels/ImGuiPanel.h"

namespace Crowny
{
    class SettingsPanel : public ImGuiPanel
    {
    public:
        SettingsPanel(const String& name);
        ~SettingsPanel() = default;

        virtual void Render() override;
    };

} // namespace Crowny