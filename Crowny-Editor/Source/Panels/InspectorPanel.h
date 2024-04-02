#pragma once

#include "ComponentEditor.h"
#include "ImGuiPanel.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Renderer/PBRMaterial.h"

namespace Crowny
{
    class Entity;

    enum class InspectorMode
    {
        Default,
        GameObject,
        Material,
        PhysicsMaterial,
        AudioClipImport,
        FontImport,
        ScriptImport,
        TextureImport,
        ShaderImport,
        MeshImport,
        Prefab,

        TextImport
    };

    class InspectorPanel : public ImGuiPanel
    {
    public:
        InspectorPanel(const String& name);
        ~InspectorPanel() = default;

        virtual void Render() override;

        static const Ref<PBRMaterial>& GetSelectedMaterial() { return s_SelectedMaterial; };
        static void SetSelectedMaterial(const Ref<PBRMaterial>& material) { s_SelectedMaterial = material; }

        void SetInspectorMode(InspectorMode mode);
        void SetSelectedAssetPath(const Path& filepath);
        void SetSelectedEntity(Entity e);

    private:
        void RenderMaterialInspector();
        void RenderPhysicsMaterialInspector();
        void RenderAudioClipImportInspector();
        void RenderFontImportInspector();
        void RenderScriptImportInspector();
        void RenderTextureImportInspector();
        void RenderTextImportInspector();
        void RenderShaderImportInspector();
        void RenderMeshImportInspector();
        void RenderPrefabInspector();

        void DrawApplyRevert(float xOffset, float width);
        void DrawHeader();

    private:
        static Ref<PBRMaterial> s_SelectedMaterial;

        InspectorMode m_InspectorMode = InspectorMode::GameObject;

        // For import options inspection
        bool m_HasPropertyChanged = false; // This should not be so "stateful".
        Ref<ImportOptions> m_ImportOptions;
        Ref<ImportOptions> m_OldImportOptions;
        Path m_InspectedAssetPath;

        String m_TemporaryImGuiString;

        UnorderedMap<Path, String, HashPath> m_CachedScriptText;

        // For normal Entity use
        Entity m_InspectedEntity;

        ComponentEditor m_ComponentEditor; // Helper object for rendering components of entities
    };
} // namespace Crowny