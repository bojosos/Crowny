#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/NodeGraph/NodeGraph.h"

namespace Crowny
{
    class NodeGraphAsset : public Asset
    {
    public:
        NodeGraphAsset() = default;
        NodeGraphAsset(Ref<NodeGraph> graph) : m_Graph(std::move(graph)) {}

        AssetType GetAssetType() const override { return AssetType::NodeGraph; }
        static AssetType GetStaticType() { return AssetType::NodeGraph; }

        Ref<NodeGraph> GetGraph() const { return m_Graph; }
        void SetGraph(Ref<NodeGraph> graph) { m_Graph = std::move(graph); }

    private:
        Ref<NodeGraph> m_Graph;
        CW_SERIALIZABLE(NodeGraphAsset);
    };

} // namespace Crowny
