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

    // ---- AddNode ----

    AddNode::AddNode(UUID id) : Node(id, "AddNode")
    {
        AddInput("A", PinDataType::Any, 0.0f);
        AddInput("B", PinDataType::Any, 0.0f);
        AddOutput("Result", PinDataType::Any);
    }

    void AddNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID aPin("A");
        static const StringID bPin("B");
        static const StringID resultPin("Result");

        const PinValue a = evaluator.PullInput(FindInputPin(aPin));
        const PinValue b = evaluator.PullInput(FindInputPin(bPin));
        const PinValue result = AddValues(a, b);
        evaluator.SetOutputValue(FindOutputPin(resultPin)->GetID(), result);
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
        static const StringID aPin("A");
        static const StringID bPin("B");
        static const StringID resultPin("Result");

        const PinValue a = evaluator.PullInput(FindInputPin(aPin));
        const PinValue b = evaluator.PullInput(FindInputPin(bPin));
        const PinValue result = MultiplyValues(a, b);
        evaluator.SetOutputValue(FindOutputPin(resultPin)->GetID(), result);
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
        static const StringID valuePin("Value");
        static const StringID fromMinPin("FromMin");
        static const StringID fromMaxPin("FromMax");
        static const StringID toMinPin("ToMin");
        static const StringID toMaxPin("ToMax");
        static const StringID resultPin("Result");

        const float value = GetInputValue<float>(valuePin, evaluator);
        const float fromMin = GetInputValue<float>(fromMinPin, evaluator);
        const float fromMax = GetInputValue<float>(fromMaxPin, evaluator);
        const float toMin = GetInputValue<float>(toMinPin, evaluator);
        const float toMax = GetInputValue<float>(toMaxPin, evaluator);

        const float range = fromMax - fromMin;
        const float t = (range != 0.0f) ? (value - fromMin) / range : 0.0f;
        const float result = toMin + t * (toMax - toMin);
        SetOutputValue(resultPin, result, evaluator);
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
        static const StringID vectorPin("Vector");
        static const StringID xPin("X");
        static const StringID yPin("Y");
        static const StringID zPin("Z");

        const glm::vec3 v = GetInputValue<glm::vec3>(vectorPin, evaluator);
        SetOutputValue(xPin, v.x, evaluator);
        SetOutputValue(yPin, v.y, evaluator);
        SetOutputValue(zPin, v.z, evaluator);
    }

} // namespace Crowny
