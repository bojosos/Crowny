#pragma once

#ifdef CW_WITH_NODES

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Panels/EditorPanelRegistration.h"
#include "Panels/ImGuiPanel.h"

namespace Crowny
{
    class NodeEditorAdapter;
    class NodeEditorContext;
    class MeshData;

    class NodeEditorPanel : public ImGuiPanel
    {
    public:
        inline static constexpr EditorPanelRegistration<NodeEditorPanel> Registration{ "Node Editor", "View/Node Editor", "", false };

        NodeEditorPanel(const String& name);
        ~NodeEditorPanel();

        void Render() override;

        void SetGraph(AssetHandle<NodeGraphAsset> graphAsset);
        Ref<NodeGraph> GetGraph() const;

        void EvaluateGraph();

        Ref<MeshData> GetLastResult() const { return m_LastResult; }

    private:
        void RenderToolbar();
        void RenderNodeCanvas();
        void RenderContextMenu();
        void RenderProperties();
        void RenderStatus();

        bool SaveGraph();
        void ExportGraph();

        void CopySelectedNodes();
        void PasteNodes();

        Scope<NodeEditorAdapter> m_Adapter;
        Scope<NodeEditorContext> m_Context;

        Ref<MeshData> m_LastResult;
        bool m_NeedsEvaluation = true;
        uint32_t m_LastEvaluatedVersion = 0xFFFFFFFF;
        uint32_t m_LastSaveVersion = 0xFFFFFFFF;
        bool m_ShowProperties = true;
        String m_SaveError;
        String m_NewInputName = "New input";
        PinDataType m_NewInputType = PinDataType::Float;

        using clock = std::chrono::steady_clock;
        clock::time_point m_LastSaveTime = clock::now();
    };

} // namespace Crowny

#endif // CW_WITH_NODES
