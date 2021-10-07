#pragma once

#include "ImGuiPanel.h"

namespace Crowny
{
    class OpenGLInformationPanel : public ImGuiPanel
    {
    public:
        OpenGLInformationPanel(const String& name);
        ~OpenGLInformationPanel() = default;

        virtual void Render() override;
    };
} // namespace Crowny