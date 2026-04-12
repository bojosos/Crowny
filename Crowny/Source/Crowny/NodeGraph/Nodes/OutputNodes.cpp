#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/OutputNodes.h"

namespace Crowny
{
    GeometryOutputNode::GeometryOutputNode(UUID id) : Node(id, "GeometryOutputNode") { AddInput("Geometry", PinDataType::MeshData); }

    void GeometryOutputNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        // Terminal node -- just triggers upstream evaluation via PullInput in the evaluator
    }

} // namespace Crowny
