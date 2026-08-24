#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    using namespace Literals;

    class AddNode : public Node
    {
    public:
        AddNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Add"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class MultiplyNode : public Node
    {
    public:
        MultiplyNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Multiply"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class SubtractNode : public Node
    {
    public:
        SubtractNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Subtract"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class DivideNode : public Node
    {
    public:
        DivideNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Divide"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class RemapNode : public Node
    {
    public:
        RemapNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Remap"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class SplitVec3Node : public Node
    {
    public:
        SplitVec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Split Vec3"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

    class CombineVec3Node : public Node
    {
    public:
        CombineVec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Combine Vec3"_sid; }
        StringID GetCategory() const override { return "Vector"_sid; }
    };

    class DotNode : public Node
    {
    public:
        DotNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Dot Product"_sid; }
        StringID GetCategory() const override { return "Vector"_sid; }
    };

    class CrossNode : public Node
    {
    public:
        CrossNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Cross Product"_sid; }
        StringID GetCategory() const override { return "Vector"_sid; }
    };

    class NormalizeNode : public Node
    {
    public:
        NormalizeNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Normalize"_sid; }
        StringID GetCategory() const override { return "Vector"_sid; }
    };

    class ClampNode : public Node
    {
    public:
        ClampNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Clamp"_sid; }
        StringID GetCategory() const override { return "Math"_sid; }
    };

} // namespace Crowny
