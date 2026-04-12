#pragma once

#include "Crowny/NodeGraph/NodeGraphAsset.h"

namespace Crowny
{
    class NodeGraphSerializer
    {
    public:
        NodeGraphSerializer(Ref<NodeGraph>& graph);

        void Serialize(const Path& filepath);
        void Deserialize(const Path& filepath);

        String SerializeToString();
        void DeserializeFromString(const String& yamlString);

    private:
        Ref<NodeGraph>& m_Graph;
    };

} // namespace Crowny