#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/Connection.h"
#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/PinTypes.h"

namespace Crowny
{
    class MeshData;

    struct GraphInput
    {
        UUID ID;
        StringID Name;
        PinDataType DataType;
        PinValue DefaultValue;
    };

    class NodeGraph : public RefCounted
    {
    public:
        enum class Domain : uint32_t
        {
            Geometry,
            Material,
            Animation,
            Script
        };

        NodeGraph();
        NodeGraph(UUID id);

        // Node management
        Node* AddNode(Ref<Node> node);
        void RemoveNode(UUID nodeId);
        Node* GetNode(UUID nodeId) const;
        const UnorderedMap<UUID, Ref<Node>>& GetNodes() const { return m_Nodes; }

        // Connection management
        bool Connect(UUID outputNodeId, StringID outputPinName, UUID inputNodeId, StringID inputPinName);
        bool ConnectByPinID(UUID outputPinId, UUID inputPinId);
        void Disconnect(UUID connectionId);
        void DisconnectPin(UUID pinId);
        const Vector<Connection>& GetConnections() const { return m_Connections; }
        bool CanConnect(const Pin* output, const Pin* input) const;

        // Find the terminal output node
        Node* FindOutputNode() const;

        // Evaluation
        Ref<MeshData> EvaluateGeometry();
        Ref<MeshData> EvaluateGeometry(const UnorderedMap<UUID, PinValue>& inputValues);

        // Inputs
        void AddInput(StringID name, PinDataType type, const PinValue& defaultValue = {});
        void RemoveInput(UUID inputId);
        void RenameInput(UUID inputId, StringID newName);
        const Vector<GraphInput>& GetInputs() const { return m_Inputs; }
        const GraphInput* GetInput(UUID inputId) const;

        // Metadata
        UUID GetID() const { return m_ID; }
        void SetName(const String& name) { m_Name = name; }
        const String& GetName() const { return m_Name; }
        Domain GetDomain() const { return m_Domain; }
        void SetDomain(Domain domain) { m_Domain = domain; }

        uint32_t GetVersion() const { return m_Version; }
        void NotifyChanged() { m_Version++; }

    private:
        friend class NodeGraphSerializer;
        Pin* FindPinByID(UUID pinId) const;

        UUID m_ID;
        String m_Name = "Untitled Graph";
        Domain m_Domain = Domain::Geometry;
        UnorderedMap<UUID, Ref<Node>> m_Nodes;
        Vector<Connection> m_Connections;
        Vector<GraphInput> m_Inputs;
        uint32_t m_Version = 0;
    };

} // namespace Crowny
