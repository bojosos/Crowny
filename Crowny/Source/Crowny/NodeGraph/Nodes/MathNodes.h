#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class FloatNode : public Node
    {
    public:
        FloatNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Float"; }
        String GetCategory() const override { return "Math"; }
    };

    class IntNode : public Node
    {
    public:
        IntNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Int"; }
        String GetCategory() const override { return "Math"; }
    };

    class Vec3Node : public Node
    {
    public:
        Vec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Vec3"; }
        String GetCategory() const override { return "Math"; }
    };

    class AddNode : public Node
    {
    public:
        AddNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Add"; }
        String GetCategory() const override { return "Math"; }
    };

    class MultiplyNode : public Node
    {
    public:
        MultiplyNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Multiply"; }
        String GetCategory() const override { return "Math"; }
    };

    class RemapNode : public Node
    {
    public:
        RemapNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Remap"; }
        String GetCategory() const override { return "Math"; }
    };

    class SplitVec3Node : public Node
    {
    public:
        SplitVec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Split Vec3"; }
        String GetCategory() const override { return "Math"; }
    };

} // namespace Crowny
