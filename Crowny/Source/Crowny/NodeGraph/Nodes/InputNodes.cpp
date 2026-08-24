#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"

namespace Crowny
{

    // ---- FloatNode ----

    FloatNode::FloatNode(UUID id) : Node(id, "FloatNode"_sid)
    {
        AddInput("Value"_sid, PinDataType::Float, 0.0f);
        AddOutput("Value"_sid, PinDataType::Float);
    }

    void FloatNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");

        float v = GetInputValue<float>(valuePin, evaluator);
        SetOutputValue(valuePin, v, evaluator);
    }

    // ---- IntNode ----

    IntNode::IntNode(UUID id) : Node(id, "IntNode"_sid)
    {
        AddInput("Value"_sid, PinDataType::Int, 0);
        AddOutput("Value"_sid, PinDataType::Int);
    }

    void IntNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");

        int32_t v = GetInputValue<int32_t>(valuePin, evaluator);
        SetOutputValue(valuePin, v, evaluator);
    }

    BoolNode::BoolNode(UUID id) : Node(id, "BoolNode"_sid)
    {
        AddInput("Value"_sid, PinDataType::Bool, false);
        AddOutput("Value"_sid, PinDataType::Bool);
    }

    void BoolNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        const bool value = GetInputValue<bool>("Value", evaluator);
        SetOutputValue("Value", value, evaluator);
    }

    Vec2Node::Vec2Node(UUID id) : Node(id, "Vec2Node"_sid)
    {
        AddInput("X"_sid, PinDataType::Float, 0.0f);
        AddInput("Y"_sid, PinDataType::Float, 0.0f);
        AddOutput("Vector"_sid, PinDataType::Vec2);
    }

    void Vec2Node::Evaluate(NodeGraphEvaluator& evaluator)
    {
        SetOutputValue("Vector", glm::vec2(GetInputValue<float>("X", evaluator), GetInputValue<float>("Y", evaluator)), evaluator);
    }

    // ---- Vec3Node ----

    Vec3Node::Vec3Node(UUID id) : Node(id, "Vec3Node"_sid)
    {
        AddInput("X"_sid, PinDataType::Float, 0.0f);
        AddInput("Y"_sid, PinDataType::Float, 0.0f);
        AddInput("Z"_sid, PinDataType::Float, 0.0f);
        AddOutput("Vector"_sid, PinDataType::Vec3);
    }

    void Vec3Node::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID xPin("X");
        static const StringID yPin("Y");
        static const StringID zPin("Z");
        static const StringID vectorPin("Vector");

        const float x = GetInputValue<float>(xPin, evaluator);
        const float y = GetInputValue<float>(yPin, evaluator);
        const float z = GetInputValue<float>(zPin, evaluator);
        SetOutputValue(vectorPin, glm::vec3(x, y, z), evaluator);
    }

    Vec4Node::Vec4Node(UUID id) : Node(id, "Vec4Node"_sid)
    {
        AddInput("X"_sid, PinDataType::Float, 0.0f);
        AddInput("Y"_sid, PinDataType::Float, 0.0f);
        AddInput("Z"_sid, PinDataType::Float, 0.0f);
        AddInput("W"_sid, PinDataType::Float, 0.0f);
        AddOutput("Vector"_sid, PinDataType::Vec4);
    }

    void Vec4Node::Evaluate(NodeGraphEvaluator& evaluator)
    {
        SetOutputValue("Vector",
                       glm::vec4(GetInputValue<float>("X", evaluator), GetInputValue<float>("Y", evaluator), GetInputValue<float>("Z", evaluator),
                                 GetInputValue<float>("W", evaluator)),
                       evaluator);
    }

    GraphInputNode::GraphInputNode(UUID id) : Node(id, "GraphInputNode"_sid) { AddOutput("Value"_sid, PinDataType::Any); }

    void GraphInputNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");
        if (m_InputID.Empty() || !GetParentGraph() || !GetParentGraph()->GetInput(m_InputID))
        {
            evaluator.ReportError("Graph Input does not reference a valid graph input");
            return;
        }
        SetOutputValue(valuePin, evaluator.GetInputValue(m_InputID), evaluator);
    }

} // namespace Crowny
