#pragma once

#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/PinTypes.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class NodeGraphEvaluator;
    class NodeGraph;
    class Pin;

    class Node
    {
    public:
        Node(UUID id, StringID typeName);
        virtual ~Node() = default;

        UUID GetID() const { return m_ID; }
        StringID GetTypeName() const { return m_TypeName; }

        glm::vec2 GetEditorPosition() const { return m_EditorPosition; }
        void SetEditorPosition(const glm::vec2& pos) { m_EditorPosition = pos; }

        const Vector<Ref<Pin>>& GetInputPins() const { return m_Inputs; }
        const Vector<Ref<Pin>>& GetOutputPins() const { return m_Outputs; }

        Pin* FindInputPin(const String& name) const;
        Pin* FindOutputPin(const String& name) const;
        Pin* FindPinByID(UUID pinId) const;

        virtual void Evaluate(NodeGraphEvaluator& evaluator) = 0;

        virtual String GetDisplayName() const { return m_TypeName.c_str(); }
        virtual String GetCategory() const { return "Uncategorized"; }

        void SetParentGraph(NodeGraph* graph);
        void NotifyChanged();

    protected:
        Ref<Pin> AddInput(const String& name, PinDataType type, const PinValue& defaultVal = {});
        Ref<Pin> AddOutput(const String& name, PinDataType type);

        template <typename T> T GetInputValue(const String& pinName, NodeGraphEvaluator& evaluator) const;

        template <typename T> void SetOutputValue(const String& pinName, const T& value, NodeGraphEvaluator& evaluator);

    private:
        UUID m_ID;
        StringID m_TypeName;
        glm::vec2 m_EditorPosition = { 0.0f, 0.0f };
        Vector<Ref<Pin>> m_Inputs;
        Vector<Ref<Pin>> m_Outputs;
        NodeGraph* m_ParentGraph = nullptr;
    };

} // namespace Crowny
