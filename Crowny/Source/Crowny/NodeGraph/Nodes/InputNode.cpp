#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNode.h"

namespace Crowny
{
    CW_REGISTER_NODE("GraphInputNode", "Input", GraphInputNode);

    GraphInputNode::GraphInputNode(UUID id) : Node(id, "GraphInputNode") { AddOutput("Value", PinDataType::Any); }

    void GraphInputNode::Evaluate(NodeGraphEvaluator& evaluator) { SetOutputValue("Value", evaluator.GetInputValue(m_InputID), evaluator); }

    String GraphInputNode::GetDisplayName() const
    {
        // This is tricky because we need the graph to get the name,
        // but Node doesn't easily store the graph until it's added.
        // For now, return a generic name, we can improve this.
        return "Graph Input";
    }

} // namespace Crowny
