#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{

    class AddNode : public Node
    {
    public:
        AddNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Add"; }
        StringID GetCategory() const override { return "Math"; }
    };

    class MultiplyNode : public Node
    {
    public:
        MultiplyNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Multiply"; }
        StringID GetCategory() const override { return "Math"; }
    };

    class RemapNode : public Node
    {
    public:
        RemapNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Remap"; }
        StringID GetCategory() const override { return "Math"; }
    };

    class SplitVec3Node : public Node
    {
    public:
        SplitVec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Split Vec3"; }
        StringID GetCategory() const override { return "Math"; }
    };

} // namespace Crowny
