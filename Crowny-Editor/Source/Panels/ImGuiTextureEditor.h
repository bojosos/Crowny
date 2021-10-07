#pragma once

#include "Panels/ImGuiPanel.h"

#include <imgui.h>

namespace Crowny
{

    class ImGuiTextureEditor : public ImGuiPanel
    {
    public:
        ImGuiTextureEditor(const String& name) : ImGuiPanel(name) {}
        virtual ~ImGuiTextureEditor() = default;

        virtual void Render() override;

    public:
        static void SetTexture(const Ref<Texture>& texture);

    private:
        static Ref<Texture> s_Texture;
    };
} // namespace Crowny