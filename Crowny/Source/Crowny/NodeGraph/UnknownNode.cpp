#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/UnknownNode.h"

namespace Crowny
{
    UnknownNode::UnknownNode(UUID id, StringID missingTypeName)
      : Node(id, missingTypeName), m_DisplayName(String("Missing: ") + missingTypeName.c_str())
    {
    }

    Ref<Pin> UnknownNode::AddSerializedPin(UUID id, StringID name, Pin::Direction direction, PinDataType type, const PinValue& defaultValue)
    {
        return direction == Pin::Direction::Input ? AddInput(id, name, type, defaultValue) : AddOutput(id, name, type);
    }

    void UnknownNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        evaluator.ReportError(String("Missing node implementation: ") + GetTypeName().c_str());
    }
} // namespace Crowny
