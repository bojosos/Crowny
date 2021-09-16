#pragma once

#include "ImGuiComponentEditor.h"
#include "ImGuiPanel.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
    class Entity;

    enum class InspectorMode
    {
        GameObject,
        Material,
        PhysicsMaterial,
        AudioClipImport,
        FontImport,
        ScriptImport,
        TextureImport,
        ShaderImport,
        MeshImport,
        Prefab
    };

    class ImGuiInspectorPanel : public ImGuiPanel
    {
    public:
        ImGuiInspectorPanel(const std::string& name);
        ~ImGuiInspectorPanel() = default;

        virtual void Render() override;

        virtual void Show() override;
        virtual void Hide() override;

        static const Ref<PBRMaterial>& GetSelectedMaterial() { return s_SelectedMaterial; };
        static void SetSelectedMaterial(const Ref<PBRMaterial>& material) { s_SelectedMaterial = material; }

        void SetInspectorMode(InspectorMode mode);
        void SetSelectedAssetPath(const std::string& filepath);

    private:
        void RenderMaterialInspector();
        void RenderPhysicsMaterialInspector();
        void RenderAudioClipImportInspector();
        void RenderFontImportInspector();
        void RenderScriptImportInspector();
        void RenderTextureImportInspector();
        void RenderShaderImportInspector();
        void RenderMeshImportInspector();
        void RenderPrefabInspector();

    private:
        static Ref<PBRMaterial> s_SelectedMaterial;

        InspectorMode m_InspectorMode = InspectorMode::GameObject;
        Ref<ImportOptions> m_ImportOptions;
        Ref<ImportOptions> m_OldImportOptions;
        std::string m_InspectedAssetPath;
        ImGuiComponentEditor m_ComponentEditor;
    };
} // namespace Crowny