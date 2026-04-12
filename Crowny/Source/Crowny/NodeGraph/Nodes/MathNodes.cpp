#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/MathNodes.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    // Helper to perform arithmetic on PinValue variants
    static PinValue AddValues(const PinValue& a, const PinValue& b)
    {
        if (std::holds_alternative<float>(a) && std::holds_alternative<float>(b))
            return std::get<float>(a) + std::get<float>(b);
        if (std::holds_alternative<int32_t>(a) && std::holds_alternative<int32_t>(b))
            return std::get<int32_t>(a) + std::get<int32_t>(b);
        if (std::holds_alternative<glm::vec2>(a) && std::holds_alternative<glm::vec2>(b))
            return std::get<glm::vec2>(a) + std::get<glm::vec2>(b);
        if (std::holds_alternative<glm::vec3>(a) && std::holds_alternative<glm::vec3>(b))
            return std::get<glm::vec3>(a) + std::get<glm::vec3>(b);
        if (std::holds_alternative<glm::vec4>(a) && std::holds_alternative<glm::vec4>(b))
            return std::get<glm::vec4>(a) + std::get<glm::vec4>(b);

        // Float + Vec broadcast
        if (std::holds_alternative<float>(a) && std::holds_alternative<glm::vec3>(b))
            return std::get<glm::vec3>(b) + std::get<float>(a);
        if (std::holds_alternative<glm::vec3>(a) && std::holds_alternative<float>(b))
            return std::get<glm::vec3>(a) + std::get<float>(b);

        return 0.0f;
    }

    static PinValue MultiplyValues(const PinValue& a, const PinValue& b)
    {
        if (std::holds_alternative<float>(a) && std::holds_alternative<float>(b))
            return std::get<float>(a) * std::get<float>(b);
        if (std::holds_alternative<int32_t>(a) && std::holds_alternative<int32_t>(b))
            return std::get<int32_t>(a) * std::get<int32_t>(b);
        if (std::holds_alternative<glm::vec2>(a) && std::holds_alternative<glm::vec2>(b))
            return std::get<glm::vec2>(a) * std::get<glm::vec2>(b);
        if (std::holds_alternative<glm::vec3>(a) && std::holds_alternative<glm::vec3>(b))
            return std::get<glm::vec3>(a) * std::get<glm::vec3>(b);
        if (std::holds_alternative<glm::vec4>(a) && std::holds_alternative<glm::vec4>(b))
            return std::get<glm::vec4>(a) * std::get<glm::vec4>(b);

        // Scalar * Vec
        if (std::holds_alternative<float>(a) && std::holds_alternative<glm::vec3>(b))
            return std::get<glm::vec3>(b) * std::get<float>(a);
        if (std::holds_alternative<glm::vec3>(a) && std::holds_alternative<float>(b))
            return std::get<glm::vec3>(a) * std::get<float>(b);

        return 0.0f;
    }

    // ---- FloatNode ----

    FloatNode::FloatNode(UUID id) : Node(id, "FloatNode")
    {
        AddInput("Value", PinDataType::Float, 0.0f);
        AddOutput("Value", PinDataType::Float);
    }

    void FloatNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        float v = GetInputValue<float>("Value", evaluator);
        SetOutputValue("Value", v, evaluator);
    }

    // ---- IntNode ----

    IntNode::IntNode(UUID id) : Node(id, "IntNode")
    {
        AddInput("Value", PinDataType::Int, 0);
        AddOutput("Value", PinDataType::Int);
    }

    void IntNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        int32_t v = GetInputValue<int32_t>("Value", evaluator);
        SetOutputValue("Value", v, evaluator);
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
        float x = GetInputValue<float>("X", evaluator);
        float y = GetInputValue<float>("Y", evaluator);
        float z = GetInputValue<float>("Z", evaluator);
        SetOutputValue("Vector", glm::vec3(x, y, z), evaluator);
    }

    // ---- AddNode ----

    AddNode::AddNode(UUID id) : Node(id, "AddNode")
    {
        AddInput("A", PinDataType::Any, 0.0f);
        AddInput("B", PinDataType::Any, 0.0f);
        AddOutput("Result", PinDataType::Any);
    }

    void AddNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        PinValue a = evaluator.PullInput(FindInputPin("A"));
        PinValue b = evaluator.PullInput(FindInputPin("B"));
        PinValue result = AddValues(a, b);
        evaluator.SetOutputValue(FindOutputPin("Result")->GetID(), result);
    }

    // ---- MultiplyNode ----

    MultiplyNode::MultiplyNode(UUID id) : Node(id, "MultiplyNode")
    {
        AddInput("A", PinDataType::Any, 1.0f);
        AddInput("B", PinDataType::Any, 1.0f);
        AddOutput("Result", PinDataType::Any);
    }

    void MultiplyNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        PinValue a = evaluator.PullInput(FindInputPin("A"));
        PinValue b = evaluator.PullInput(FindInputPin("B"));
        PinValue result = MultiplyValues(a, b);
        evaluator.SetOutputValue(FindOutputPin("Result")->GetID(), result);
    }

    // ---- RemapNode ----

    RemapNode::RemapNode(UUID id) : Node(id, "RemapNode")
    {
        AddInput("Value", PinDataType::Float, 0.0f);
        AddInput("FromMin", PinDataType::Float, 0.0f);
        AddInput("FromMax", PinDataType::Float, 1.0f);
        AddInput("ToMin", PinDataType::Float, 0.0f);
        AddInput("ToMax", PinDataType::Float, 1.0f);
        AddOutput("Result", PinDataType::Float);
    }

    void RemapNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        float value = GetInputValue<float>("Value", evaluator);
        float fromMin = GetInputValue<float>("FromMin", evaluator);
        float fromMax = GetInputValue<float>("FromMax", evaluator);
        float toMin = GetInputValue<float>("ToMin", evaluator);
        float toMax = GetInputValue<float>("ToMax", evaluator);

        float range = fromMax - fromMin;
        float t = (range != 0.0f) ? (value - fromMin) / range : 0.0f;
        float result = toMin + t * (toMax - toMin);
        SetOutputValue("Result", result, evaluator);
    }

    // ---- SplitVec3Node ----

    SplitVec3Node::SplitVec3Node(UUID id) : Node(id, "SplitVec3Node")
    {
        AddInput("Vector", PinDataType::Vec3, glm::vec3(0.0f));
        AddOutput("X", PinDataType::Float);
        AddOutput("Y", PinDataType::Float);
        AddOutput("Z", PinDataType::Float);
    }

    void SplitVec3Node::Evaluate(NodeGraphEvaluator& evaluator)
    {
        glm::vec3 v = GetInputValue<glm::vec3>("Vector", evaluator);
        SetOutputValue("X", v.x, evaluator);
        SetOutputValue("Y", v.y, evaluator);
        SetOutputValue("Z", v.z, evaluator);
    }

} // namespace Crowny
