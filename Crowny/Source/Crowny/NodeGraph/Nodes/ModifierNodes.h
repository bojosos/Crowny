#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class TransformGeometryNode : public Node
    {
    public:
        TransformGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Transform"; }
        String GetCategory() const override { return "Geometry/Modifiers"; }
    };

    class MergeGeometryNode : public Node
    {
    public:
        MergeGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Merge"; }
        String GetCategory() const override { return "Geometry/Modifiers"; }
    };

    class NoiseDisplaceNode : public Node
    {
    public:
        NoiseDisplaceNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Noise Displace"; }
        String GetCategory() const override { return "Geometry/Modifiers"; }
    };

    class RecalculateNormalsNode : public Node
    {
    public:
        RecalculateNormalsNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Recalculate Normals"; }
        String GetCategory() const override { return "Geometry/Modifiers"; }
    };

    class SubdivideNode : public Node
    {
    public:
        SubdivideNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Subdivide"; }
        String GetCategory() const override { return "Geometry/Modifiers"; }
    };

} // namespace Crowny
