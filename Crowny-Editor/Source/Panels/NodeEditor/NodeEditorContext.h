#pragma once

#ifdef CW_WITH_NODES

#include "Crowny/NodeGraph/NodeGraph.h"

namespace Crowny
{
    class NodeEditorContext
    {
    public:
        NodeEditorContext() = default;

        void SetGraph(AssetHandle<NodeGraphAsset> graphAsset);
        AssetHandle<NodeGraphAsset> GetAsset() const { return m_GraphAsset; }
        Ref<NodeGraph> GetGraph() const { return m_Graph; }

        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }
        void ClearDirty() { m_Dirty = false; }

        void SetSelectedNodeID(UUID id) { m_SelectedNodeID = id; }
        UUID GetSelectedNodeID() const { return m_SelectedNodeID; }

    private:
        AssetHandle<NodeGraphAsset> m_GraphAsset;
        Ref<NodeGraph> m_Graph;
        UUID m_SelectedNodeID;
        bool m_Dirty = false;
    };

} // namespace Crowny

#endif // CW_WITH_NODES
