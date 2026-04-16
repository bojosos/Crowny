#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    NodeGraph::NodeGraph() : m_ID(UuidGenerator::Generate()) {}

    NodeGraph::NodeGraph(UUID id) : m_ID(id) {}

    Node* NodeGraph::AddNode(Ref<Node> node)
    {
        UUID id = node->GetID();
        node->SetParentGraph(this);
        m_Nodes[id] = std::move(node);
        NotifyChanged();
        return m_Nodes[id].get();
    }

    void NodeGraph::RemoveNode(UUID nodeId)
    {
        // Remove all connections involving this node
        m_Connections.erase(std::remove_if(m_Connections.begin(), m_Connections.end(),
                                           [&](const Connection& conn) { return conn.OutputNodeID == nodeId || conn.InputNodeID == nodeId; }),
                            m_Connections.end());

        // Disconnect pins
        auto it = m_Nodes.find(nodeId);
        if (it != m_Nodes.end())
        {
            Node* node = it->second.get();
            for (const auto& pin : node->GetInputPins())
                pin->SetConnectedPin(nullptr);
            for (const auto& pin : node->GetOutputPins())
            {
                // Find all input pins connected to this output and disconnect them
                for (auto& [id, otherNode] : m_Nodes)
                {
                    for (const auto& otherPin : otherNode->GetInputPins())
                    {
                        if (otherPin->GetConnectedPin() == pin.get())
                            otherPin->SetConnectedPin(nullptr);
                    }
                }
            }
            m_Nodes.erase(it);
        }
        NotifyChanged();
    }

    Node* NodeGraph::GetNode(UUID nodeId) const
    {
        auto it = m_Nodes.find(nodeId);
        return it != m_Nodes.end() ? it->second.get() : nullptr;
    }

    bool NodeGraph::Connect(UUID outputNodeId, StringID outputPinName, UUID inputNodeId, StringID inputPinName)
    {
        Node* outputNode = GetNode(outputNodeId);
        Node* inputNode = GetNode(inputNodeId);
        if (!outputNode || !inputNode)
            return false;

        Pin* outputPin = outputNode->FindOutputPin(outputPinName);
        Pin* inputPin = inputNode->FindInputPin(inputPinName);
        if (!outputPin || !inputPin)
            return false;

        if (!CanConnect(outputPin, inputPin))
            return false;

        // Disconnect any existing connection on the input pin
        if (inputPin->IsConnected())
            DisconnectPin(inputPin->GetID());

        inputPin->SetConnectedPin(outputPin);

        Connection conn;
        conn.ID = UuidGenerator::Generate();
        conn.OutputNodeID = outputNodeId;
        conn.OutputPinID = outputPin->GetID();
        conn.InputNodeID = inputNodeId;
        conn.InputPinID = inputPin->GetID();
        m_Connections.push_back(conn);

        NotifyChanged();
        return true;
    }

    bool NodeGraph::ConnectByPinID(UUID outputPinId, UUID inputPinId)
    {
        Pin* outputPin = FindPinByID(outputPinId);
        Pin* inputPin = FindPinByID(inputPinId);
        if (!outputPin || !inputPin)
            return false;

        if (outputPin->GetDirection() != Pin::Direction::Output || inputPin->GetDirection() != Pin::Direction::Input)
            return false;

        if (!CanConnect(outputPin, inputPin))
            return false;

        if (inputPin->IsConnected())
            DisconnectPin(inputPin->GetID());

        inputPin->SetConnectedPin(outputPin);

        Connection conn;
        conn.ID = UuidGenerator::Generate();
        conn.OutputNodeID = outputPin->GetOwner()->GetID();
        conn.OutputPinID = outputPinId;
        conn.InputNodeID = inputPin->GetOwner()->GetID();
        conn.InputPinID = inputPinId;
        m_Connections.push_back(conn);

        NotifyChanged();
        return true;
    }

    void NodeGraph::Disconnect(UUID connectionId)
    {
        for (auto it = m_Connections.begin(); it != m_Connections.end(); ++it)
        {
            if (it->ID == connectionId)
            {
                Pin* inputPin = FindPinByID(it->InputPinID);
                if (inputPin)
                    inputPin->SetConnectedPin(nullptr);
                m_Connections.erase(it);
                NotifyChanged();
                return;
            }
        }
    }

    void NodeGraph::DisconnectPin(UUID pinId)
    {
        bool changed = false;
        for (auto it = m_Connections.begin(); it != m_Connections.end();)
        {
            if (it->OutputPinID == pinId || it->InputPinID == pinId)
            {
                Pin* inputPin = FindPinByID(it->InputPinID);
                if (inputPin)
                    inputPin->SetConnectedPin(nullptr);
                it = m_Connections.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
        if (changed)
            NotifyChanged();
    }

    bool NodeGraph::CanConnect(Pin* output, Pin* input) const
    {
        if (!output || !input)
            return false;
        if (output->GetDirection() != Pin::Direction::Output || input->GetDirection() != Pin::Direction::Input)
            return false;
        if (output->GetOwner() == input->GetOwner())
            return false;
        return ArePinTypesCompatible(output->GetDataType(), input->GetDataType());
    }

    Node* NodeGraph::FindOutputNode() const
    {
        for (const auto& [id, node] : m_Nodes)
        {
            if (node->GetTypeName() == "GeometryOutputNode")
                return node.get();
        }
        return nullptr;
    }

    Ref<MeshData> NodeGraph::EvaluateGeometry()
    {
        NodeGraphEvaluator evaluator(*this);
        return evaluator.EvaluateGeometry();
    }

    Ref<MeshData> NodeGraph::EvaluateGeometry(const UnorderedMap<UUID, PinValue>& inputValues)
    {
        NodeGraphEvaluator evaluator(*this, inputValues);
        return evaluator.EvaluateGeometry();
    }

    void NodeGraph::AddInput(StringID name, PinDataType type, const PinValue& defaultValue)
    {
        GraphInput input;
        input.ID = UuidGenerator::Generate();
        input.Name = name;
        input.DataType = type;
        input.DefaultValue = defaultValue.index() == 0 && std::get<float>(defaultValue) == 0.0f ? DefaultPinValue(type) : defaultValue;
        m_Inputs.push_back(input);
        NotifyChanged();
    }

    void NodeGraph::RemoveInput(UUID inputId)
    {
        m_Inputs.erase(std::remove_if(m_Inputs.begin(), m_Inputs.end(), [&](const GraphInput& i) { return i.ID == inputId; }), m_Inputs.end());
        NotifyChanged();
    }

    void NodeGraph::RenameInput(UUID inputId, StringID newName)
    {
        for (auto& input : m_Inputs)
        {
            if (input.ID == inputId)
            {
                input.Name = newName;
                NotifyChanged();
                return;
            }
        }
    }

    const GraphInput* NodeGraph::GetInput(UUID inputId) const
    {
        for (const auto& input : m_Inputs)
        {
            if (input.ID == inputId)
                return &input;
        }
        return nullptr;
    }

    Pin* NodeGraph::FindPinByID(UUID pinId) const
    {
        for (const auto& [nodeId, node] : m_Nodes)
        {
            Pin* pin = node->FindPinByID(pinId);
            if (pin)
                return pin;
        }
        return nullptr;
    }

} // namespace Crowny
