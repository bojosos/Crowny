#pragma once

#include "Panels/ImGuiPanel.h"

namespace Crowny
{

    class TextureEditor : public ImGuiPanel
    {
    public:
        TextureEditor(const String& name) : ImGuiPanel(name) {}
        virtual ~TextureEditor() = default;

        virtual void Render() override;

    public:
        static void SetTexture(const Ref<Texture>& texture);

    private:
        static Ref<Texture> s_Texture;
    };
} // namespace Crowny