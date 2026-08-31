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

        Ref<MeshData> EvaluateGeometry();
        Ref<MeshData> EvaluateGeometry(const UnorderedMap<UUID, PinValue>& inputValues);

        PinValue PullInput(const Pin* inputPin);
        void SetOutputValue(UUID pinId, const PinValue& value);
        PinValue GetOutputValue(UUID pinId) const;

        PinValue GetInputValue(UUID inputId);

        bool HasError() const { return m_HasError; }
        const String& GetError() const { return m_Error; }
        void ReportError(const String& error);

    private:
        struct CachedPinValue
        {
            PinValue Value;
            uint64_t Epoch = 0;
        };

        struct NodeEvaluationState
        {
            uint64_t EvaluatedEpoch = 0;
            uint64_t InProgressEpoch = 0;
        };

        Ref<MeshData> EvaluateGeometry(const UnorderedMap<UUID, PinValue>* inputValues);
        void EvaluateNode(Node* node);

        NodeGraph& m_Graph;
        // Entries persist across evaluations; epochs make reuse allocation-free without exposing stale values.
        UnorderedMap<UUID, CachedPinValue> m_Cache;
        UnorderedMap<UUID, NodeEvaluationState> m_NodeStates;
        const UnorderedMap<UUID, PinValue>* m_InputValues = nullptr;
        uint64_t m_EvaluationEpoch = 0;
        bool m_Evaluating = false;
        bool m_HasError = false;
        String m_Error;
    };

} // namespace Crowny
