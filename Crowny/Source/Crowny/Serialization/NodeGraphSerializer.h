#pragma once

#include "Crowny/NodeGraph/NodeGraphAsset.h"

namespace Crowny
{
    class NodeGraphSerializer
    {
    public:
        NodeGraphSerializer(Ref<NodeGraph>& graph);

        bool Serialize(const Path& filepath);
        bool Deserialize(const Path& filepath);

        String SerializeToString();
        bool DeserializeFromString(const String& yamlString);

    private:
        Ref<NodeGraph>& m_Graph;
    };

} // namespace Crowny
