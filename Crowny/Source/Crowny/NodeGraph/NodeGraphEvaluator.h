#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/PinTypes.h"

namespace Crowny
{
    class NodeGraph;
    class Node;
    class Pin;
    class MeshData;

    class NodeGraphEvaluator
    {
    public:
        NodeGraphEvaluator(NodeGraph& graph);
        NodeGraphEvaluator(NodeGraph& graph, const UnorderedMap<UUID, PinValue>& inputValues);

        Ref<MeshData> EvaluateGeometry();

        PinValue PullInput(const Pin* inputPin);
        void SetOutputValue(UUID pinId, const PinValue& value);
        PinValue GetOutputValue(UUID pinId) const;

        PinValue GetInputValue(UUID inputId);

        bool HasError() const { return m_HasError; }
        const String& GetError() const { return m_Error; }
        void ReportError(const String& error);

    private:
        void EvaluateNode(Node* node);

        NodeGraph& m_Graph;
        UnorderedMap<UUID, PinValue> m_Cache;
        UnorderedSet<UUID> m_EvaluatedNodes;
        UnorderedSet<UUID> m_InProgressNodes; // cycle detection
        UnorderedMap<UUID, PinValue> m_InputValues;
        bool m_HasError = false;
        String m_Error;
    };

} // namespace Crowny
