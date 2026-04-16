#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class GeometryOutputNode : public Node
    {
    public:
        GeometryOutputNode(UUID id);

        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Geometry Output"; }
        StringID GetCategory() const override { return "Output"; }
    };

} // namespace Crowny
