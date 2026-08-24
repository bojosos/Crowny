#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    using namespace Literals;
    class BoxNode : public Node
    {
    public:
        BoxNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Box"_sid; }
        StringID GetCategory() const override { return "Primitives"_sid; }
    };

    class SphereNode : public Node
    {
    public:
        SphereNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Sphere"_sid; }
        StringID GetCategory() const override { return "Primitives"_sid; }
    };

    class CylinderNode : public Node
    {
    public:
        CylinderNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Cylinder"_sid; }
        StringID GetCategory() const override { return "Primitives"_sid; }
    };

    class PlaneNode : public Node
    {
    public:
        PlaneNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Plane"_sid; }
        StringID GetCategory() const override { return "Primitives"_sid; }
    };

    class GridNode : public Node
    {
    public:
        GridNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Grid"_sid; }
        StringID GetCategory() const override { return "Primitives"_sid; }
    };

} // namespace Crowny
