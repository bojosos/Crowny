#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/PinTypes.h"
#include "Crowny/Utils/SmallVector.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class NodeGraphEvaluator;
    class NodeGraph;
    class Pin;

    class Node : public RefCounted
    {
    public:
        Node(UUID id, StringID typeName);
        virtual ~Node() = default;

        UUID GetID() const { return m_ID; }
        StringID GetTypeName() const { return m_TypeName; }

        glm::vec2 GetEditorPosition() const { return m_EditorPosition; }
        void SetEditorPosition(const glm::vec2& pos) { m_EditorPosition = pos; }

        const SmallVector<Ref<Pin>, 4>& GetInputPins() const { return m_Inputs; }
        const SmallVector<Ref<Pin>, 4>& GetOutputPins() const { return m_Outputs; }

        Pin* FindInputPin(StringID name) const;
        Pin* FindOutputPin(StringID name) const;
        Pin* FindPinByID(UUID pinId) const;

        virtual void Evaluate(NodeGraphEvaluator& evaluator) = 0;

        virtual StringID GetDisplayName() const { return m_TypeName; }
        virtual StringID GetCategory() const { return "Uncategorized"; }

        void SetParentGraph(NodeGraph* graph);
        void NotifyChanged();

    protected:
        Ref<Pin> AddInput(StringID name, PinDataType type, const PinValue& defaultVal = {});
        Ref<Pin> AddOutput(StringID name, PinDataType type);

        template <typename T> T GetInputValue(StringID pinName, NodeGraphEvaluator& evaluator) const;

        template <typename T> void SetOutputValue(StringID pinName, const T& value, NodeGraphEvaluator& evaluator);

    private:
        UUID m_ID;
        StringID m_TypeName;
        glm::vec2 m_EditorPosition = { 0.0f, 0.0f };
        SmallVector<Ref<Pin>, 4> m_Inputs;
        SmallVector<Ref<Pin>, 4> m_Outputs;
        NodeGraph* m_ParentGraph = nullptr;
    };

} // namespace Crowny
