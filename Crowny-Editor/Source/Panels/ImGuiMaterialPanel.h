#pragma once

#include "ImGuiPanel.h"

#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{

    class ImGuiMaterialPanel : public ImGuiPanel
    {
    public:
        ImGuiMaterialPanel(const std::string& name);
        ~ImGuiMaterialPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

        static void SetSelectedMaterial(const Ref<PBRMaterial>& material) { s_SelectedMaterial = material; }
        static Ref<PBRMaterial> GetSlectedMaterial() { return s_SelectedMaterial; }

    private:
        static Ref<PBRMaterial> s_SelectedMaterial;
    };

} // namespace Crowny