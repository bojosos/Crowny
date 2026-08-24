#pragma once

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Editor/EditorLayer.h"

#include <optional>

namespace Crowny
{
    class NodeAddedAction final : public UndoAction
    {
    public:
        NodeAddedAction(Ref<NodeGraph> graph, Ref<Node> node) : m_Graph(std::move(graph)), m_Node(std::move(node)) {}
        void Commit() override { m_Graph->AddNode(m_Node); }
        void Revert() override { m_Graph->RemoveNode(m_Node->GetID()); }

    private:
        Ref<NodeGraph> m_Graph;
        Ref<Node> m_Node;
    };

    class NodeRemovedAction final : public UndoAction
    {
    public:
        NodeRemovedAction(Ref<NodeGraph> graph, Ref<Node> node) : m_Graph(std::move(graph)), m_Node(std::move(node))
        {
            for (const Connection& connection : m_Graph->GetConnections())
            {
                if (connection.OutputNodeID == m_Node->GetID() || connection.InputNodeID == m_Node->GetID())
                    m_Connections.push_back(connection);
            }
        }

        void Commit() override { m_Graph->RemoveNode(m_Node->GetID()); }
        void Revert() override
        {
            if (!m_Graph->AddNode(m_Node))
                return;
            for (const Connection& connection : m_Connections)
                m_Graph->ConnectByPinID(connection.OutputPinID, connection.InputPinID, connection.ID);
        }

    private:
        Ref<NodeGraph> m_Graph;
        Ref<Node> m_Node;
        Vector<Connection> m_Connections;
    };

    class NodesConnectedAction final : public UndoAction
    {
    public:
        NodesConnectedAction(Ref<NodeGraph> graph, UUID outputPinId, UUID inputPinId)
          : m_Graph(std::move(graph)), m_OutputPinId(outputPinId), m_InputPinId(inputPinId), m_ConnectionId(UuidGenerator::Generate())
        {
            if (const Connection* displaced = m_Graph->GetInputConnection(inputPinId))
                m_DisplacedConnection = *displaced;
        }

        void Commit() override { m_Graph->ConnectByPinID(m_OutputPinId, m_InputPinId, m_ConnectionId); }
        void Revert() override
        {
            m_Graph->Disconnect(m_ConnectionId);
            if (m_DisplacedConnection)
                m_Graph->ConnectByPinID(m_DisplacedConnection->OutputPinID, m_DisplacedConnection->InputPinID, m_DisplacedConnection->ID);
        }

    private:
        Ref<NodeGraph> m_Graph;
        UUID m_OutputPinId;
        UUID m_InputPinId;
        UUID m_ConnectionId;
        std::optional<Connection> m_DisplacedConnection;
    };

    class NodesDisconnectedAction final : public UndoAction
    {
    public:
        NodesDisconnectedAction(Ref<NodeGraph> graph, const Connection& connection) : m_Graph(std::move(graph)), m_Connection(connection) {}

        void Commit() override { m_Graph->Disconnect(m_Connection.ID); }
        void Revert() override { m_Graph->ConnectByPinID(m_Connection.OutputPinID, m_Connection.InputPinID, m_Connection.ID); }

    private:
        Ref<NodeGraph> m_Graph;
        Connection m_Connection;
    };

    class PinDefaultValueAction final : public UndoAction
    {
    public:
        PinDefaultValueAction(Ref<NodeGraph> graph, UUID pinId, PinValue oldValue, PinValue newValue)
          : m_Graph(std::move(graph)), m_PinId(pinId), m_OldValue(std::move(oldValue)), m_NewValue(std::move(newValue))
        {
        }

        void Commit() override { Set(m_NewValue); }
        void Revert() override { Set(m_OldValue); }

    private:
        void Set(const PinValue& value)
        {
            if (Pin* pin = m_Graph->GetPin(m_PinId))
                pin->SetDefaultValue(value);
        }

        Ref<NodeGraph> m_Graph;
        UUID m_PinId;
        PinValue m_OldValue;
        PinValue m_NewValue;
    };

    class NodeMovedAction final : public UndoAction
    {
    public:
        NodeMovedAction(Ref<NodeGraph> graph, UUID nodeId, glm::vec2 oldPosition, glm::vec2 newPosition)
          : m_Graph(std::move(graph)), m_NodeId(nodeId), m_OldPosition(oldPosition), m_NewPosition(newPosition)
        {
        }

        void Commit() override { Set(m_NewPosition); }
        void Revert() override { Set(m_OldPosition); }

    private:
        void Set(const glm::vec2& position)
        {
            if (Node* node = m_Graph->GetNode(m_NodeId))
                node->SetEditorPosition(position);
        }

        Ref<NodeGraph> m_Graph;
        UUID m_NodeId;
        glm::vec2 m_OldPosition;
        glm::vec2 m_NewPosition;
    };
} // namespace Crowny
