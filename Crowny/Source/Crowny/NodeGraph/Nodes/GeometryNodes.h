#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    class BoxNode : public Node
    {
    public:
        BoxNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Box"; }
        String GetCategory() const override { return "Geometry/Primitives"; }
    };

    class SphereNode : public Node
    {
    public:
        SphereNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Sphere"; }
        String GetCategory() const override { return "Geometry/Primitives"; }
    };

    class PlaneNode : public Node
    {
    public:
        PlaneNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Plane"; }
        String GetCategory() const override { return "Geometry/Primitives"; }
    };

    class GridNode : public Node
    {
    public:
        GridNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        String GetDisplayName() const override { return "Grid"; }
        String GetCategory() const override { return "Geometry/Primitives"; }
    };

} // namespace Crowny
