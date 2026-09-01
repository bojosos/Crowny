#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/Connection.h"
#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/PinTypes.h"

#include <limits>

namespace Crowny
{
    class MeshData;
    class NodeGraphEvaluator;

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
        ~NodeGraph();

        // Node management
        Node* AddNode(Ref<Node> node);
        bool RemoveNode(UUID nodeId);
        Node* GetNode(UUID nodeId) const;
        Pin* GetPin(UUID pinId) const;
        const UnorderedMap<UUID, Ref<Node>>& GetNodes() const { return m_Nodes; }

        // Connection management
        bool Connect(UUID outputNodeId, StringID outputPinName, UUID inputNodeId, StringID inputPinName);
        bool ConnectByPinID(UUID outputPinId, UUID inputPinId);
        bool ConnectByPinID(UUID outputPinId, UUID inputPinId, UUID connectionId);
        bool Disconnect(UUID connectionId);
        bool DisconnectPin(UUID pinId);
        const Vector<Connection>& GetConnections() const { return m_Connections; }
        const Connection* GetConnection(UUID connectionId) const;
        const Connection* GetInputConnection(UUID inputPinId) const;
        bool CanConnect(const Pin* output, const Pin* input) const;

        // Find the terminal output node
        Node* FindOutputNode() const;

        // Evaluation
        Ref<MeshData> EvaluateGeometry();
        Ref<MeshData> EvaluateGeometry(const UnorderedMap<UUID, PinValue>& inputValues);
        const String& GetLastEvaluationError() const { return m_LastEvaluationError; }

        // Inputs
        UUID AddInput(StringID name, PinDataType type);
        UUID AddInput(StringID name, PinDataType type, const PinValue& defaultValue);
        bool RemoveInput(UUID inputId);
        bool RenameInput(UUID inputId, StringID newName);
        bool SetInputDefaultValue(UUID inputId, const PinValue& value);
        const Vector<GraphInput>& GetInputs() const { return m_Inputs; }
        const GraphInput* GetInput(UUID inputId) const;

        // Metadata
        UUID GetID() const { return m_ID; }
        void SetName(const String& name);
        const String& GetName() const { return m_Name; }
        Domain GetDomain() const { return m_Domain; }
        void SetDomain(Domain domain);

        uint32_t GetVersion() const { return m_Version; }
        uint32_t GetEvaluationVersion() const { return m_EvaluationVersion; }
        void NotifyChanged();
        void NotifyEditorChanged() { m_Version++; }

    private:
        friend class NodeGraphSerializer;
        Pin* FindPinByID(UUID pinId) const;
        bool WouldCreateCycle(UUID outputNodeId, UUID inputNodeId, UUID replacedInputPinId) const;
        void InvalidateEvaluation();
        NodeGraphEvaluator& GetEvaluator();

        UUID m_ID;
        String m_Name = "Untitled Graph";
        Domain m_Domain = Domain::Geometry;
        UnorderedMap<UUID, Ref<Node>> m_Nodes;
        UnorderedMap<UUID, Pin*> m_Pins;
        Vector<Connection> m_Connections;
        Vector<GraphInput> m_Inputs;
        uint32_t m_Version = 0;
        uint32_t m_EvaluationVersion = 0;
        uint32_t m_CachedEvaluationVersion = std::numeric_limits<uint32_t>::max();
        Ref<MeshData> m_CachedGeometry;
        String m_LastEvaluationError;
        Scope<NodeGraphEvaluator> m_Evaluator;
        uint32_t m_EvaluatorVersion = std::numeric_limits<uint32_t>::max();
    };

} // namespace Crowny
