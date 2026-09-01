#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    NodeGraph::NodeGraph() : m_ID(UuidGenerator::Generate()) {}

    NodeGraph::NodeGraph(UUID id) : m_ID(id) {}

    NodeGraph::~NodeGraph() = default;

    Node* NodeGraph::AddNode(Ref<Node> node)
    {
        if (!node || node->GetID().Empty() || m_Nodes.find(node->GetID()) != m_Nodes.end() || m_Pins.find(node->GetID()) != m_Pins.end() ||
            GetConnection(node->GetID()))
            return nullptr;

        const UUID id = node->GetID();
        Vector<Pin*> pins;
        pins.reserve(node->GetInputPins().size() + node->GetOutputPins().size());
        for (const auto& pin : node->GetInputPins())
            pins.push_back(pin.get());
        for (const auto& pin : node->GetOutputPins())
            pins.push_back(pin.get());
        UnorderedSet<UUID> nodePinIds;
        for (Pin* pin : pins)
        {
            if (!pin || pin->GetID().Empty() || !nodePinIds.insert(pin->GetID()).second || m_Pins.find(pin->GetID()) != m_Pins.end() ||
                m_Nodes.find(pin->GetID()) != m_Nodes.end() || GetConnection(pin->GetID()))
            {
                CW_ENGINE_ERROR("Cannot add node '{0}' because one of its pin IDs is empty or duplicated.", node->GetTypeName().c_str());
                return nullptr;
            }
        }

        node->SetParentGraph(this);
        m_Nodes[id] = std::move(node);
        for (Pin* pin : pins)
            m_Pins.emplace(pin->GetID(), pin);
        NotifyChanged();
        return m_Nodes[id].get();
    }

    bool NodeGraph::RemoveNode(UUID nodeId)
    {
        auto it = m_Nodes.find(nodeId);
        if (it == m_Nodes.end())
            return false;

        // Remove all connections involving this node
        m_Connections.erase(std::remove_if(m_Connections.begin(), m_Connections.end(),
                                           [&](const Connection& conn) { return conn.OutputNodeID == nodeId || conn.InputNodeID == nodeId; }),
                            m_Connections.end());

        // Disconnect pins
        const Node* node = it->second.get();
        for (const auto& pin : node->GetInputPins())
        {
            pin->SetConnectedPin(nullptr);
            m_Pins.erase(pin->GetID());
        }
        for (const auto& pin : node->GetOutputPins())
        {
            m_Pins.erase(pin->GetID());
            // Find all input pins connected to this output and disconnect them
            for (const auto& [id, otherNode] : m_Nodes)
            {
                for (const auto& otherPin : otherNode->GetInputPins())
                {
                    if (otherPin->GetConnectedPin() == pin.get())
                        otherPin->SetConnectedPin(nullptr);
                }
            }
        }
        it->second->SetParentGraph(nullptr);
        m_Nodes.erase(it);
        NotifyChanged();
        return true;
    }

    Node* NodeGraph::GetNode(UUID nodeId) const
    {
        const auto it = m_Nodes.find(nodeId);
        return it != m_Nodes.end() ? it->second.get() : nullptr;
    }

    Pin* NodeGraph::GetPin(UUID pinId) const { return FindPinByID(pinId); }

    bool NodeGraph::Connect(UUID outputNodeId, StringID outputPinName, UUID inputNodeId, StringID inputPinName)
    {
        const Node* outputNode = GetNode(outputNodeId);
        const Node* inputNode = GetNode(inputNodeId);
        if (!outputNode || !inputNode)
            return false;

        Pin* outputPin = outputNode->FindOutputPin(outputPinName);
        Pin* inputPin = inputNode->FindInputPin(inputPinName);
        if (!outputPin || !inputPin)
            return false;

        if (!CanConnect(outputPin, inputPin))
            return false;

        return ConnectByPinID(outputPin->GetID(), inputPin->GetID());
    }

    bool NodeGraph::ConnectByPinID(UUID outputPinId, UUID inputPinId) { return ConnectByPinID(outputPinId, inputPinId, UuidGenerator::Generate()); }

    bool NodeGraph::ConnectByPinID(UUID outputPinId, UUID inputPinId, UUID connectionId)
    {
        Pin* outputPin = FindPinByID(outputPinId);
        Pin* inputPin = FindPinByID(inputPinId);
        if (!outputPin || !inputPin || connectionId.Empty() || GetConnection(connectionId) || GetNode(connectionId) || GetPin(connectionId))
            return false;

        if (outputPin->GetDirection() != Pin::Direction::Output || inputPin->GetDirection() != Pin::Direction::Input)
            return false;

        if (!CanConnect(outputPin, inputPin))
            return false;

        for (const Connection& connection : m_Connections)
        {
            if (connection.OutputPinID == outputPinId && connection.InputPinID == inputPinId)
                return false;
        }

        if (inputPin->IsConnected())
            DisconnectPin(inputPin->GetID());

        inputPin->SetConnectedPin(outputPin);

        Connection conn;
        conn.ID = connectionId;
        conn.OutputNodeID = outputPin->GetOwner()->GetID();
        conn.OutputPinID = outputPinId;
        conn.InputNodeID = inputPin->GetOwner()->GetID();
        conn.InputPinID = inputPinId;
        m_Connections.push_back(conn);

        NotifyChanged();
        return true;
    }

    bool NodeGraph::Disconnect(UUID connectionId)
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
                return true;
            }
        }
        return false;
    }

    bool NodeGraph::DisconnectPin(UUID pinId)
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
        return changed;
    }

    const Connection* NodeGraph::GetConnection(UUID connectionId) const
    {
        const auto it = std::find_if(m_Connections.begin(), m_Connections.end(),
                                     [connectionId](const Connection& connection) { return connection.ID == connectionId; });
        return it == m_Connections.end() ? nullptr : &*it;
    }

    const Connection* NodeGraph::GetInputConnection(UUID inputPinId) const
    {
        const auto it = std::find_if(m_Connections.begin(), m_Connections.end(),
                                     [inputPinId](const Connection& connection) { return connection.InputPinID == inputPinId; });
        return it == m_Connections.end() ? nullptr : &*it;
    }

    bool NodeGraph::CanConnect(const Pin* output, const Pin* input) const
    {
        if (!output || !input)
            return false;
        if (output->GetDirection() != Pin::Direction::Output || input->GetDirection() != Pin::Direction::Input)
            return false;
        if (output->GetOwner() == input->GetOwner())
            return false;
        if (FindPinByID(output->GetID()) != output || FindPinByID(input->GetID()) != input)
            return false;
        if (!ArePinTypesCompatible(output->GetDataType(), input->GetDataType()))
            return false;
        return !WouldCreateCycle(output->GetOwner()->GetID(), input->GetOwner()->GetID(), input->GetID());
    }

    bool NodeGraph::WouldCreateCycle(UUID outputNodeId, UUID inputNodeId, UUID replacedInputPinId) const
    {
        Vector<UUID> pending{ inputNodeId };
        UnorderedSet<UUID> visited;
        while (!pending.empty())
        {
            const UUID nodeId = pending.back();
            pending.pop_back();
            if (nodeId == outputNodeId)
                return true;
            if (!visited.insert(nodeId).second)
                continue;
            for (const Connection& connection : m_Connections)
            {
                if (connection.OutputNodeID == nodeId && connection.InputPinID != replacedInputPinId)
                    pending.push_back(connection.InputNodeID);
            }
        }
        return false;
    }

    Node* NodeGraph::FindOutputNode() const
    {
        Node* outputNode = nullptr;
        UUID outputId = UUID::EMPTY;
        for (const auto& [id, node] : m_Nodes)
        {
            if (node->GetTypeName() == "GeometryOutputNode"_sid && (!outputNode || id < outputId))
            {
                outputNode = node.get();
                outputId = id;
            }
        }
        return outputNode;
    }

    Ref<MeshData> NodeGraph::EvaluateGeometry()
    {
        if (m_CachedEvaluationVersion == m_EvaluationVersion)
            return m_CachedGeometry;
        NodeGraphEvaluator& evaluator = GetEvaluator();
        m_CachedGeometry = evaluator.EvaluateGeometry();
        m_LastEvaluationError = evaluator.GetError();
        m_CachedEvaluationVersion = m_EvaluationVersion;
        return m_CachedGeometry;
    }

    Ref<MeshData> NodeGraph::EvaluateGeometry(const UnorderedMap<UUID, PinValue>& inputValues)
    {
        NodeGraphEvaluator& evaluator = GetEvaluator();
        Ref<MeshData> result = evaluator.EvaluateGeometry(inputValues);
        m_LastEvaluationError = evaluator.GetError();
        return result;
    }

    NodeGraphEvaluator& NodeGraph::GetEvaluator()
    {
        if (!m_Evaluator || m_EvaluatorVersion != m_EvaluationVersion)
        {
            m_Evaluator = CreateScope<NodeGraphEvaluator>(*this);
            m_EvaluatorVersion = m_EvaluationVersion;
        }
        return *m_Evaluator;
    }

    UUID NodeGraph::AddInput(StringID name, PinDataType type) { return AddInput(name, type, DefaultPinValue(type)); }

    UUID NodeGraph::AddInput(StringID name, PinDataType type, const PinValue& defaultValue)
    {
        PinValue convertedValue;
        const bool duplicateName = std::any_of(m_Inputs.begin(), m_Inputs.end(), [name](const GraphInput& input) { return input.Name == name; });
        if (name.IsEmpty() || duplicateName || !ConvertPinValue(defaultValue, type, convertedValue))
            return UUID::EMPTY;
        GraphInput input;
        input.ID = UuidGenerator::Generate();
        input.Name = name;
        input.DataType = type;
        input.DefaultValue = std::move(convertedValue);
        m_Inputs.push_back(input);
        NotifyChanged();
        return input.ID;
    }

    bool NodeGraph::RemoveInput(UUID inputId)
    {
        const auto oldSize = m_Inputs.size();
        m_Inputs.erase(std::remove_if(m_Inputs.begin(), m_Inputs.end(), [&](const GraphInput& input) { return input.ID == inputId; }),
                       m_Inputs.end());
        if (oldSize == m_Inputs.size())
            return false;
        NotifyChanged();
        return true;
    }

    bool NodeGraph::RenameInput(UUID inputId, StringID newName)
    {
        if (newName.IsEmpty() ||
            std::any_of(m_Inputs.begin(), m_Inputs.end(), [&](const GraphInput& input) { return input.ID != inputId && input.Name == newName; }))
            return false;
        for (auto& input : m_Inputs)
        {
            if (input.ID == inputId)
            {
                input.Name = newName;
                NotifyChanged();
                return true;
            }
        }
        return false;
    }

    bool NodeGraph::SetInputDefaultValue(UUID inputId, const PinValue& value)
    {
        for (GraphInput& input : m_Inputs)
        {
            if (input.ID != inputId)
                continue;
            PinValue convertedValue;
            if (!ConvertPinValue(value, input.DataType, convertedValue))
                return false;
            if (input.DefaultValue == convertedValue)
                return true;
            input.DefaultValue = std::move(convertedValue);
            NotifyChanged();
            return true;
        }
        return false;
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

    void NodeGraph::SetName(const String& name)
    {
        if (m_Name == name)
            return;
        m_Name = name;
        NotifyEditorChanged();
    }

    void NodeGraph::SetDomain(Domain domain)
    {
        if (m_Domain == domain)
            return;
        m_Domain = domain;
        NotifyChanged();
    }

    Pin* NodeGraph::FindPinByID(UUID pinId) const
    {
        const auto it = m_Pins.find(pinId);
        return it == m_Pins.end() ? nullptr : it->second;
    }

    void NodeGraph::NotifyChanged()
    {
        m_Version++;
        m_EvaluationVersion++;
        InvalidateEvaluation();
    }

    void NodeGraph::InvalidateEvaluation()
    {
        m_CachedEvaluationVersion = std::numeric_limits<uint32_t>::max();
        m_CachedGeometry = nullptr;
        m_LastEvaluationError.clear();
    }

} // namespace Crowny
