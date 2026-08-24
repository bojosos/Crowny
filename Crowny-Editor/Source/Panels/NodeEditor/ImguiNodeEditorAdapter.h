#pragma once

#ifdef CW_WITH_NODES

#include "Panels/NodeEditor/NodeEditorAdapter.h"

#include <glm/glm.hpp>

namespace ax::NodeEditor
{
    struct EditorContext;
}

namespace Crowny
{
    class NodeGraph;
    class Node;

    class ImguiNodeEditorAdapter : public NodeEditorAdapter
    {
    public:
        ImguiNodeEditorAdapter();
        ~ImguiNodeEditorAdapter() override;

        void SyncFromGraph(const Ref<NodeGraph>& graph) override;
        void SyncToGraph(const Ref<NodeGraph>& graph) override;

        void Render() override;

        void RenderAddNodeMenu(const Ref<NodeGraph>& graph) override;

        UUID GetSelectedNodeID() const override { return m_SelectedNodeID; }
        Vector<UUID> GetSelectedNodes() const override;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
        UUID m_SelectedNodeID = UUID::EMPTY;
        Vector<UUID> m_SelectedNodes;

        bool m_NeedsSync = true;
        Ref<NodeGraph> m_CurrentGraph;
        String m_AddNodeSearch;
        bool m_FocusAddNodeSearch = false;
        glm::vec2 m_AddNodePosition = glm::vec2(0.0f);
    };

} // namespace Crowny

#endif // CW_WITH_NODES
