#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"

namespace Crowny
{

    // ---- FloatNode ----

    FloatNode::FloatNode(UUID id) : Node(id, "FloatNode")
    {
        AddInput("Value", PinDataType::Float, 0.0f);
        AddOutput("Value", PinDataType::Float);
    }

    void FloatNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");

        float v = GetInputValue<float>(valuePin, evaluator);
        SetOutputValue(valuePin, v, evaluator);
    }

    // ---- IntNode ----

    IntNode::IntNode(UUID id) : Node(id, "IntNode")
    {
        AddInput("Value", PinDataType::Int, 0);
        AddOutput("Value", PinDataType::Int);
    }

    void IntNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");

        int32_t v = GetInputValue<int32_t>(valuePin, evaluator);
        SetOutputValue(valuePin, v, evaluator);
    }

    // ---- Vec3Node ----

    Vec3Node::Vec3Node(UUID id) : Node(id, "Vec3Node")
    {
        AddInput("X", PinDataType::Float, 0.0f);
        AddInput("Y", PinDataType::Float, 0.0f);
        AddInput("Z", PinDataType::Float, 0.0f);
        AddOutput("Vector", PinDataType::Vec3);
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

    GraphInputNode::GraphInputNode(UUID id) : Node(id, "GraphInputNode") { AddOutput("Value", PinDataType::Any); }

    void GraphInputNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID valuePin("Value");
        SetOutputValue(valuePin, evaluator.GetInputValue(m_InputID), evaluator);
    }

} // namespace Crowny
