#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class BoxNode : public Node
    {
    public:
        BoxNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Box"; }
        StringID GetCategory() const override { return "Primitives"; }
    };

    class SphereNode : public Node
    {
    public:
        SphereNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Sphere"; }
        StringID GetCategory() const override { return "Primitives"; }
    };

    class PlaneNode : public Node
    {
    public:
        PlaneNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Plane"; }
        StringID GetCategory() const override { return "Primitives"; }
    };

    class GridNode : public Node
    {
    public:
        GridNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Grid"; }
        StringID GetCategory() const override { return "Primitives"; }
    };

} // namespace Crowny
