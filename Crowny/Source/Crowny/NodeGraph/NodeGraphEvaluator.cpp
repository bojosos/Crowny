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
            ReportError("No output node found in graph");
            return nullptr;
        }

        const Pin* geometryPin = outputNode->FindInputPin("Geometry");
        if (!geometryPin)
        {
            ReportError("Output node has no 'Geometry' input pin");
            return nullptr;
        }

        const PinValue result = PullInput(geometryPin);
        if (std::holds_alternative<Ref<MeshData>>(result))
            return std::get<Ref<MeshData>>(result);

        if (!m_HasError)
            ReportError("Output node did not produce MeshData");
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

        const auto convertForInput = [&](const PinValue& value) {
            PinValue converted;
            if (ConvertPinValue(value, inputPin->GetDataType(), converted))
                return converted;
            ReportError(String("Pin '") + inputPin->GetName().c_str() + "' received an incompatible value");
            return DefaultPinValue(inputPin->GetDataType());
        };

        auto cacheIt = m_Cache.find(connectedOutput->GetID());
        if (cacheIt != m_Cache.end())
            return convertForInput(cacheIt->second);

        // Evaluate the upstream node
        Node* upstreamNode = connectedOutput->GetOwner();
        if (!upstreamNode)
        {
            ReportError("Connected output pin has no owning node");
            return DefaultPinValue(inputPin->GetDataType());
        }
        EvaluateNode(upstreamNode);

        // Retrieve from cache after evaluation
        cacheIt = m_Cache.find(connectedOutput->GetID());
        if (cacheIt != m_Cache.end())
            return convertForInput(cacheIt->second);

        if (!m_HasError)
            ReportError(String("Node '") + upstreamNode->GetDisplayName().c_str() + "' did not set output '" + connectedOutput->GetName().c_str() +
                        "'");
        return DefaultPinValue(inputPin->GetDataType());
    }

    void NodeGraphEvaluator::SetOutputValue(UUID pinId, const PinValue& value)
    {
        const Pin* outputPin = m_Graph.GetPin(pinId);
        if (!outputPin || outputPin->GetDirection() != Pin::Direction::Output)
        {
            ReportError("A node attempted to write to an unknown or non-output pin");
            return;
        }
        PinValue converted;
        if (!ConvertPinValue(value, outputPin->GetDataType(), converted))
        {
            ReportError(String("Node '") + outputPin->GetOwner()->GetDisplayName().c_str() + "' produced an incompatible value");
            return;
        }
        m_Cache[pinId] = std::move(converted);
    }

    PinValue NodeGraphEvaluator::GetOutputValue(UUID pinId) const
    {
        const auto it = m_Cache.find(pinId);
        if (it != m_Cache.end())
            return it->second;
        return 0.0f;
    }

    PinValue NodeGraphEvaluator::GetInputValue(UUID inputId)
    {
        const GraphInput* input = m_Graph.GetInput(inputId);
        if (!input)
        {
            ReportError("A Graph Input node references a missing graph input");
            return 0.0f;
        }
        const auto it = m_InputValues.find(inputId);
        if (it != m_InputValues.end())
        {
            PinValue converted;
            if (ConvertPinValue(it->second, input->DataType, converted))
                return converted;
            ReportError(String("Graph input '") + input->Name.c_str() + "' received an incompatible value");
            return DefaultPinValue(input->DataType);
        }

        return input->DefaultValue;
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
            ReportError(String("Cycle detected at node: ") + node->GetDisplayName().c_str());
            return;
        }

        m_InProgressNodes.insert(nodeId);
        try
        {
            node->Evaluate(*this);
        }
        catch (const std::exception& exception)
        {
            ReportError(String("Node '") + node->GetDisplayName().c_str() + "' failed: " + exception.what());
        }
        catch (...)
        {
            ReportError(String("Node '") + node->GetDisplayName().c_str() + "' failed with an unknown error");
        }
        m_InProgressNodes.erase(nodeId);
        if (!m_HasError)
            m_EvaluatedNodes.insert(nodeId);
    }

    void NodeGraphEvaluator::ReportError(const String& error)
    {
        if (!m_HasError)
        {
            m_HasError = true;
            m_Error = error;
            CW_ENGINE_ERROR("NodeGraph evaluation error: {0}", error);
        }
    }

} // namespace Crowny
