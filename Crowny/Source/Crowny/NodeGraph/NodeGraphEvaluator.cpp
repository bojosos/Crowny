#include "cwpch.h"

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    NodeGraphEvaluator::NodeGraphEvaluator(NodeGraph& graph) : m_Graph(graph) {}

    NodeGraphEvaluator::NodeGraphEvaluator(NodeGraph& graph, const UnorderedMap<UUID, PinValue>& inputValues)
      : m_Graph(graph), m_InputValues(inputValues)
    {
    }

    Ref<MeshData> NodeGraphEvaluator::EvaluateGeometry()
    {
        m_Cache.clear();
        m_EvaluatedNodes.clear();
        m_InProgressNodes.clear();
        m_HasError = false;
        m_Error.clear();

        Node* outputNode = m_Graph.FindOutputNode();
        if (!outputNode)
        {
            SetError("No output node found in graph");
            return nullptr;
        }

        EvaluateNode(outputNode);

        if (m_HasError)
            return nullptr;

        const Pin* geometryPin = outputNode->FindInputPin("Geometry");
        if (!geometryPin)
        {
            SetError("Output node has no 'Geometry' input pin");
            return nullptr;
        }

        const PinValue result = PullInput(geometryPin);
        if (std::holds_alternative<Ref<MeshData>>(result))
            return std::get<Ref<MeshData>>(result);

        SetError("Output node did not produce MeshData");
        return nullptr;
    }

    PinValue NodeGraphEvaluator::PullInput(const Pin* inputPin)
    {
        if (!inputPin || m_HasError)
            return DefaultPinValue(inputPin ? inputPin->GetDataType() : PinDataType::Float);

        if (!inputPin->IsConnected())
            return inputPin->GetDefaultValue();

        const Pin* connectedOutput = inputPin->GetConnectedPin();
        if (!connectedOutput)
            return inputPin->GetDefaultValue();

        // Check cache first
        auto cacheIt = m_Cache.find(connectedOutput->GetID());
        if (cacheIt != m_Cache.end())
            return cacheIt->second;

        // Evaluate the upstream node
        Node* upstreamNode = connectedOutput->GetOwner();
        if (upstreamNode)
            EvaluateNode(upstreamNode);

        // Retrieve from cache after evaluation
        cacheIt = m_Cache.find(connectedOutput->GetID());
        if (cacheIt != m_Cache.end())
            return cacheIt->second;

        return inputPin->GetDefaultValue();
    }

    void NodeGraphEvaluator::SetOutputValue(UUID pinId, const PinValue& value) { m_Cache[pinId] = value; }

    PinValue NodeGraphEvaluator::GetOutputValue(UUID pinId) const
    {
        const auto it = m_Cache.find(pinId);
        if (it != m_Cache.end())
            return it->second;
        return 0.0f;
    }

    const PinValue& NodeGraphEvaluator::GetInputValue(UUID inputId) const
    {
        const auto it = m_InputValues.find(inputId);
        if (it != m_InputValues.end())
            return it->second;

        const auto* input = m_Graph.GetInput(inputId);
        if (input)
            return input->DefaultValue;

        static PinValue empty = 0.0f;
        return empty;
    }

    void NodeGraphEvaluator::EvaluateNode(Node* node)
    {
        if (!node || m_HasError)
            return;

        const UUID nodeId = node->GetID();

        if (m_EvaluatedNodes.count(nodeId))
            return;

        if (m_InProgressNodes.count(nodeId))
        {
            SetError(String("Cycle detected at node: ") + node->GetDisplayName().c_str());
            return;
        }

        m_InProgressNodes.insert(nodeId);
        node->Evaluate(*this);
        m_InProgressNodes.erase(nodeId);
        m_EvaluatedNodes.insert(nodeId);
    }

    void NodeGraphEvaluator::SetError(const String& error)
    {
        if (!m_HasError)
        {
            m_HasError = true;
            m_Error = error;
            CW_ENGINE_ERROR("NodeGraph evaluation error: {0}", error);
        }
    }

} // namespace Crowny
