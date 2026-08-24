#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{
    using namespace Literals;

    class FloatNode : public Node
    {
    public:
        FloatNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Float"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class IntNode : public Node
    {
    public:
        IntNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Int"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class BoolNode : public Node
    {
    public:
        BoolNode(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Bool"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class Vec2Node : public Node
    {
    public:
        Vec2Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Vec2"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class Vec3Node : public Node
    {
    public:
        Vec3Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Vec3"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class Vec4Node : public Node
    {
    public:
        Vec4Node(UUID id);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return "Vec4"_sid; }
        StringID GetCategory() const override { return "Input"_sid; }
    };

    class GraphInputNode : public Node
    {
    public:
        GraphInputNode(UUID id);
        virtual void Evaluate(NodeGraphEvaluator& evaluator) override;
        virtual StringID GetDisplayName() const override { return "Graph Input"_sid; }
        virtual StringID GetCategory() const override { return "Input"_sid; }

        void SetInputID(UUID inputId)
        {
            if (m_InputID == inputId)
                return;
            m_InputID = inputId;
            NotifyChanged();
        }
        UUID GetInputID() const { return m_InputID; }

    private:
        UUID m_InputID;
    };

} // namespace Crowny
