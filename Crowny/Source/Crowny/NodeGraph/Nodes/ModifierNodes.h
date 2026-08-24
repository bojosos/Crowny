#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    using namespace Literals;
    class TransformGeometryNode : public Node
    {
    public:
        TransformGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Transform"_sid; }
        StringID GetCategory() const override { return "Modifiers"_sid; }
    };

    class MergeGeometryNode : public Node
    {
    public:
        MergeGeometryNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Merge"_sid; }
        StringID GetCategory() const override { return "Modifiers"_sid; }
    };

    class NoiseDisplaceNode : public Node
    {
    public:
        NoiseDisplaceNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Noise Displace"_sid; }
        StringID GetCategory() const override { return "Modifiers"_sid; }
    };

    class RecalculateNormalsNode : public Node
    {
    public:
        RecalculateNormalsNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Recalculate Normals"_sid; }
        StringID GetCategory() const override { return "Modifiers"_sid; }
    };

    class SubdivideNode : public Node
    {
    public:
        SubdivideNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Subdivide"_sid; }
        StringID GetCategory() const override { return "Modifiers"_sid; }
    };

} // namespace Crowny
