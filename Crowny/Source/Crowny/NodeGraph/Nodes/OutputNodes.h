#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    using namespace Literals;
    class GeometryOutputNode : public Node
    {
    public:
        GeometryOutputNode(UUID id);

        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Geometry Output"_sid; }
        StringID GetCategory() const override { return "Output"_sid; }
    };

} // namespace Crowny
