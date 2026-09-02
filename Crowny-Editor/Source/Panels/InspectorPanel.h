#pragma once

#include "EntityInspector.h"
#include "EditorPanelRegistration.h"
#include "ImGuiPanel.h"
#include "MaterialInspectorSchemaCache.h"

#include "Editor/AssetSaveTracker.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/Renderer/MaterialPresetLibrary.h"

#include <functional>

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
        inline static constexpr EditorPanelRegistration<InspectorPanel> Registration{ "Inspector", "View/Inspector" };

        InspectorPanel(const String& name);
        ~InspectorPanel() override;

        virtual void Render() override;

        void SetInspectorMode(InspectorMode mode);
        void SetSelectedAssetPath(const Path& filepath);
        void SetSelectedEntity(Entity e);
        void SetSelectedEntities(Entity primary, const Vector<Entity>& entities);
        void ResetUndoTransactions(bool finishInteraction);

        // Called with the graph that should be opened in the node editor panel.
        // Set by EditorLayer so the inspector widget can open the panel without a hard dependency.
        void SetOpenNodeEditorCallback(std::function<void(AssetHandle<NodeGraphAsset>)> cb) { m_OpenNodeEditorCallback = std::move(cb); }

    private:
        void RenderMaterialInspector();
        bool DrawMaterialShaderPicker(Material& material);
        bool DrawMaterialPresetRow(Material& material);
        void RefreshMaterialShaderOptions();
        void RefreshMaterialPresetOptions();
        void SaveMaterialAsPreset(const Material& material);
        void RenderPhysicsMaterialInspector();
        void RenderAudioClipImportInspector();
        void RenderFontImportInspector();
        void RenderScriptImportInspector();
        void RenderTextureImportInspector();
        void RenderTextImportInspector();
        void RenderShaderImportInspector();
        void RenderMeshImportInspector();
        void RenderPrefabInspector();

        void HandleInspectorDragDrop(const Vector<Entity>& selectedEntities);

        template <typename T> T* BeginImportInspector();
        void EndImportInspector(float xOffset, float width);

        void DrawApplyRevert(float xOffset, float width);
        void DrawHeader();
        void ObserveAssetEdit(const Ref<Asset>& asset, bool changed);
        void SaveReadyAssets();
        void FlushPendingAssetSaves();

    private:
        InspectorMode m_InspectorMode = InspectorMode::GameObject;

        // For import options inspection
        bool m_HasPropertyChanged = false; // Reset on mode/asset switch; set by import inspectors via |=
        Ref<ImportOptions> m_ImportOptions;
        Ref<ImportOptions> m_OldImportOptions;
        Path m_InspectedAssetPath;

        String m_TemporaryImGuiString;

        UnorderedMap<Path, String, HashPath> m_CachedScriptText;

        // For normal Entity use
        Entity m_InspectedEntity;
        Vector<Entity> m_InspectedEntities;

        EntityInspector m_EntityInspector; // Renders the header, component list and Add Component popup for entities

        MaterialInspectorSchemaCache m_MaterialSchemaCache;

        struct MaterialPickerState
        {
            bool BuiltInShadersLoaded = false;
            Vector<MaterialShaderOption> BuiltInShaders;
            Vector<MaterialShaderOption> ShaderOptions; // Built-in + project, rebuilt when the picker opens
            bool ShowInternalShaders = false;
            String ShaderSearch;
            Vector<MaterialPresetEntry> PresetOptions; // Compatible presets, rebuilt when the picker opens
            String PresetSearch;
            String StatusMessage;
            bool StatusIsError = false;
            double StatusExpiry = 0.0;
        } m_MaterialPicker;
        AssetSaveTracker m_AssetSaveTracker;

        std::function<void(AssetHandle<NodeGraphAsset>)> m_OpenNodeEditorCallback;
    };
} // namespace Crowny
