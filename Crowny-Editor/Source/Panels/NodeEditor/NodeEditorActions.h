#pragma once

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Editor/EditorLayer.h"

namespace Crowny
{
    class NodeAddedAction : public UndoAction
    {
    public:
        NodeAddedAction(Ref<NodeGraph> graph, Ref<Node> node) : m_Graph(graph), m_Node(node) {}

        void Commit() override { m_Graph->AddNode(m_Node); }

        void Revert() override { m_Graph->RemoveNode(m_Node->GetID()); }

    private:
        Ref<NodeGraph> m_Graph;
        Ref<Node> m_Node;
    };

    class NodeRemovedAction : public UndoAction
    {
    public:
        NodeRemovedAction(Ref<NodeGraph> graph, Ref<Node> node) : m_Graph(graph), m_Node(node) {}

        void Commit() override { m_Graph->RemoveNode(m_Node->GetID()); }

        void Revert() override { m_Graph->AddNode(m_Node); }

    private:
        Ref<NodeGraph> m_Graph;
        Ref<Node> m_Node;
    };

    class NodesConnectedAction : public UndoAction
    {
    public:
        NodesConnectedAction(Ref<NodeGraph> graph, UUID outputPinId, UUID inputPinId)
          : m_Graph(graph), m_OutputPinId(outputPinId), m_InputPinId(inputPinId)
        {
        }

        void Commit() override { m_Graph->ConnectByPinID(m_OutputPinId, m_InputPinId); }

        void Revert() override
        {
            const auto& connections = m_Graph->GetConnections();
            for (const auto& conn : connections)
            {
                if (conn.OutputPinID == m_OutputPinId && conn.InputPinID == m_InputPinId)
                {
                    m_Graph->Disconnect(conn.ID);
                    break;
                }
            }
        }

    private:
        Ref<NodeGraph> m_Graph;
        UUID m_OutputPinId;
        UUID m_InputPinId;
    };

    class NodesDisconnectedAction : public UndoAction
    {
    public:
        NodesDisconnectedAction(Ref<NodeGraph> graph, UUID outputPinId, UUID inputPinId)
          : m_Graph(graph), m_OutputPinId(outputPinId), m_InputPinId(inputPinId)
        {
        }

        void Commit() override
        {
            const auto& connections = m_Graph->GetConnections();
            for (const auto& conn : connections)
            {
                if (conn.OutputPinID == m_OutputPinId && conn.InputPinID == m_InputPinId)
                {
                    m_Graph->Disconnect(conn.ID);
                    break;
                }
            }
        }

        void Revert() override { m_Graph->ConnectByPinID(m_OutputPinId, m_InputPinId); }

    private:
        Ref<NodeGraph> m_Graph;
        UUID m_OutputPinId;
        UUID m_InputPinId;
    };

} // namespace Crowny