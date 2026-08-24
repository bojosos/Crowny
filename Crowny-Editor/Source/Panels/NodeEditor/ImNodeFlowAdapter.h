#pragma once

#ifdef CW_WITH_NODES

#include "Crowny/Common/Uuid.h"

#include "Panels/NodeEditor/NodeEditorAdapter.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class NodeGraph;
    class Node;

    class ImNodeFlowAdapter : public NodeEditorAdapter
    {
    public:
        ImNodeFlowAdapter();
        ~ImNodeFlowAdapter() override;

        void SyncFromGraph(const Ref<NodeGraph>& graph) override;
        void SyncToGraph(const Ref<NodeGraph>& graph) override;

        void Render() override;

        void RenderAddNodeMenu(const Ref<NodeGraph>& graph) override;

        UUID GetSelectedNodeID() const override { return m_SelectedNodeID; }
        Vector<UUID> GetSelectedNodes() const override;

    private:
        struct Impl;
        Scope<Impl> m_Impl;
        UUID m_SelectedNodeID;
        bool m_NeedsSync = true;
        String m_AddNodeSearch;
        bool m_FocusAddNodeSearch = false;
        glm::vec2 m_AddNodePosition = glm::vec2(0.0f);
    };

} // namespace Crowny

#endif // CW_WITH_NODES
