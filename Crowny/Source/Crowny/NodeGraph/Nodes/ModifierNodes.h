#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class TransformGeometryNode : public Node
    {
    public:
        TransformGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Transform"; }
        StringID GetCategory() const override { return "Modifiers"; }
    };

    class MergeGeometryNode : public Node
    {
    public:
        MergeGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Merge"; }
        StringID GetCategory() const override { return "Modifiers"; }
    };

    class NoiseDisplaceNode : public Node
    {
    public:
        NoiseDisplaceNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Noise Displace"; }
        StringID GetCategory() const override { return "Modifiers"; }
    };

    class RecalculateNormalsNode : public Node
    {
    public:
        RecalculateNormalsNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Recalculate Normals"; }
        StringID GetCategory() const override { return "Modifiers"; }
    };

    class SubdivideNode : public Node
    {
    public:
        SubdivideNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Subdivide"; }
        StringID GetCategory() const override { return "Modifiers"; }
    };

} // namespace Crowny
