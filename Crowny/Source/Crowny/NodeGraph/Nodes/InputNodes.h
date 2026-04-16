#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{

    class FloatNode : public Node
    {
    public:
        FloatNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Float"; }
        StringID GetCategory() const override { return "Input"; }
    };

    class IntNode : public Node
    {
    public:
        IntNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Int"; }
        StringID GetCategory() const override { return "Input"; }
    };

    class Vec3Node : public Node
    {
    public:
        Vec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Vec3"; }
        StringID GetCategory() const override { return "Input"; }
    };

    class GraphInputNode : public Node
    {
    public:
        GraphInputNode(UUID id);
        virtual void Evaluate(NodeGraphEvaluator& evaluator) override;
        virtual StringID GetDisplayName() const override { return "Graph Input"; }
        virtual StringID GetCategory() const override { return "Input"; }

        void SetInputID(UUID inputId)
        {
            m_InputID = inputId;
            NotifyChanged();
        }
        UUID GetInputID() const { return m_InputID; }

    private:
        UUID m_InputID;
    };

} // namespace Crowny
