#include "cwpch.h"

#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/MathNodes.h"
#include "Crowny/NodeGraph/Pin.h"

#include <limits>

namespace Crowny
{
    template <typename Operation> static bool ApplyNumericBinary(const PinValue& a, const PinValue& b, PinValue& result, Operation operation)
    {
        if (a.index() == b.index())
        {
            if (std::holds_alternative<float>(a))
                result = operation(std::get<float>(a), std::get<float>(b));
            else if (std::holds_alternative<int32_t>(a))
                result = operation(std::get<int32_t>(a), std::get<int32_t>(b));
            else if (std::holds_alternative<glm::vec2>(a))
                result = operation(std::get<glm::vec2>(a), std::get<glm::vec2>(b));
            else if (std::holds_alternative<glm::vec3>(a))
                result = operation(std::get<glm::vec3>(a), std::get<glm::vec3>(b));
            else if (std::holds_alternative<glm::vec4>(a))
                result = operation(std::get<glm::vec4>(a), std::get<glm::vec4>(b));
            else
                return false;
            return true;
        }

        const auto applyScalar = [&](const auto& vectorValue, float scalar, bool scalarFirst) {
            using VectorType = std::decay_t<decltype(vectorValue)>;
            result = scalarFirst ? operation(VectorType(scalar), vectorValue) : operation(vectorValue, VectorType(scalar));
        };
        if (std::holds_alternative<float>(a))
        {
            if (std::holds_alternative<glm::vec2>(b))
                applyScalar(std::get<glm::vec2>(b), std::get<float>(a), true);
            else if (std::holds_alternative<glm::vec3>(b))
                applyScalar(std::get<glm::vec3>(b), std::get<float>(a), true);
            else if (std::holds_alternative<glm::vec4>(b))
                applyScalar(std::get<glm::vec4>(b), std::get<float>(a), true);
            else if (std::holds_alternative<int32_t>(b))
                result = operation(std::get<float>(a), static_cast<float>(std::get<int32_t>(b)));
            else
                return false;
            return true;
        }
        if (std::holds_alternative<float>(b))
        {
            if (std::holds_alternative<glm::vec2>(a))
                applyScalar(std::get<glm::vec2>(a), std::get<float>(b), false);
            else if (std::holds_alternative<glm::vec3>(a))
                applyScalar(std::get<glm::vec3>(a), std::get<float>(b), false);
            else if (std::holds_alternative<glm::vec4>(a))
                applyScalar(std::get<glm::vec4>(a), std::get<float>(b), false);
            else if (std::holds_alternative<int32_t>(a))
                result = operation(static_cast<float>(std::get<int32_t>(a)), std::get<float>(b));
            else
                return false;
            return true;
        }
        return false;
    }

    static bool HasZeroComponent(const PinValue& value)
    {
        if (std::holds_alternative<float>(value))
            return std::get<float>(value) == 0.0f;
        if (std::holds_alternative<int32_t>(value))
            return std::get<int32_t>(value) == 0;
        if (std::holds_alternative<glm::vec2>(value))
            return glm::any(glm::equal(std::get<glm::vec2>(value), glm::vec2(0.0f)));
        if (std::holds_alternative<glm::vec3>(value))
            return glm::any(glm::equal(std::get<glm::vec3>(value), glm::vec3(0.0f)));
        if (std::holds_alternative<glm::vec4>(value))
            return glm::any(glm::equal(std::get<glm::vec4>(value), glm::vec4(0.0f)));
        return false;
    }

    // ---- AddNode ----

    AddNode::AddNode(UUID id) : Node(id, "AddNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Any, 0.0f);
        AddInput("B"_sid, PinDataType::Any, 0.0f);
        AddOutput("Result"_sid, PinDataType::Any);
    }

    void AddNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID aPin("A");
        static const StringID bPin("B");
        static const StringID resultPin("Result");

        const PinValue a = evaluator.PullInput(FindInputPin(aPin));
        const PinValue b = evaluator.PullInput(FindInputPin(bPin));
        PinValue result;
        if (!ApplyNumericBinary(a, b, result, [](const auto& lhs, const auto& rhs) { return lhs + rhs; }))
            evaluator.ReportError("Add requires compatible numeric values");
        else
            evaluator.SetOutputValue(FindOutputPin(resultPin)->GetID(), result);
    }

    // ---- MultiplyNode ----

    MultiplyNode::MultiplyNode(UUID id) : Node(id, "MultiplyNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Any, 1.0f);
        AddInput("B"_sid, PinDataType::Any, 1.0f);
        AddOutput("Result"_sid, PinDataType::Any);
    }

    void MultiplyNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        static const StringID aPin("A");
        static const StringID bPin("B");
        static const StringID resultPin("Result");

        const PinValue a = evaluator.PullInput(FindInputPin(aPin));
        const PinValue b = evaluator.PullInput(FindInputPin(bPin));
        PinValue result;
        if (!ApplyNumericBinary(a, b, result, [](const auto& lhs, const auto& rhs) { return lhs * rhs; }))
            evaluator.ReportError("Multiply requires compatible numeric values");
        else
            evaluator.SetOutputValue(FindOutputPin(resultPin)->GetID(), result);
    }

    SubtractNode::SubtractNode(UUID id) : Node(id, "SubtractNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Any, 0.0f);
        AddInput("B"_sid, PinDataType::Any, 0.0f);
        AddOutput("Result"_sid, PinDataType::Any);
    }

    void SubtractNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        PinValue result;
        if (!ApplyNumericBinary(evaluator.PullInput(FindInputPin("A")), evaluator.PullInput(FindInputPin("B")), result,
                                [](const auto& lhs, const auto& rhs) { return lhs - rhs; }))
            evaluator.ReportError("Subtract requires compatible numeric values");
        else
            evaluator.SetOutputValue(FindOutputPin("Result")->GetID(), result);
    }

    DivideNode::DivideNode(UUID id) : Node(id, "DivideNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Any, 1.0f);
        AddInput("B"_sid, PinDataType::Any, 1.0f);
        AddOutput("Result"_sid, PinDataType::Any);
    }

    void DivideNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        const PinValue a = evaluator.PullInput(FindInputPin("A"));
        const PinValue b = evaluator.PullInput(FindInputPin("B"));
        if (HasZeroComponent(b))
        {
            evaluator.ReportError("Divide received a zero divisor");
            return;
        }
        PinValue result;
        if (!ApplyNumericBinary(a, b, result, [](const auto& lhs, const auto& rhs) { return lhs / rhs; }))
            evaluator.ReportError("Divide requires compatible numeric values");
        else
            evaluator.SetOutputValue(FindOutputPin("Result")->GetID(), result);
    }

    // ---- RemapNode ----

    RemapNode::RemapNode(UUID id) : Node(id, "RemapNode"_sid)
    {
        AddInput("Value"_sid, PinDataType::Float, 0.0f);
        AddInput("FromMin"_sid, PinDataType::Float, 0.0f);
        AddInput("FromMax"_sid, PinDataType::Float, 1.0f);
        AddInput("ToMin"_sid, PinDataType::Float, 0.0f);
        AddInput("ToMax"_sid, PinDataType::Float, 1.0f);
        AddOutput("Result"_sid, PinDataType::Float);
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
        if (range == 0.0f)
        {
            evaluator.ReportError("Remap source range cannot be zero");
            return;
        }
        const float t = (value - fromMin) / range;
        const float result = toMin + t * (toMax - toMin);
        SetOutputValue(resultPin, result, evaluator);
    }

    // ---- SplitVec3Node ----

    SplitVec3Node::SplitVec3Node(UUID id) : Node(id, "SplitVec3Node"_sid)
    {
        AddInput("Vector"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddOutput("X"_sid, PinDataType::Float);
        AddOutput("Y"_sid, PinDataType::Float);
        AddOutput("Z"_sid, PinDataType::Float);
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

    CombineVec3Node::CombineVec3Node(UUID id) : Node(id, "CombineVec3Node"_sid)
    {
        AddInput("X"_sid, PinDataType::Float, 0.0f);
        AddInput("Y"_sid, PinDataType::Float, 0.0f);
        AddInput("Z"_sid, PinDataType::Float, 0.0f);
        AddOutput("Vector"_sid, PinDataType::Vec3);
    }

    void CombineVec3Node::Evaluate(NodeGraphEvaluator& evaluator)
    {
        SetOutputValue("Vector",
                       glm::vec3(GetInputValue<float>("X", evaluator), GetInputValue<float>("Y", evaluator), GetInputValue<float>("Z", evaluator)),
                       evaluator);
    }

    DotNode::DotNode(UUID id) : Node(id, "DotNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddInput("B"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddOutput("Result"_sid, PinDataType::Float);
    }

    void DotNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        SetOutputValue("Result", glm::dot(GetInputValue<glm::vec3>("A", evaluator), GetInputValue<glm::vec3>("B", evaluator)), evaluator);
    }

    CrossNode::CrossNode(UUID id) : Node(id, "CrossNode"_sid)
    {
        AddInput("A"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddInput("B"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddOutput("Result"_sid, PinDataType::Vec3);
    }

    void CrossNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        SetOutputValue("Result", glm::cross(GetInputValue<glm::vec3>("A", evaluator), GetInputValue<glm::vec3>("B", evaluator)), evaluator);
    }

    NormalizeNode::NormalizeNode(UUID id) : Node(id, "NormalizeNode"_sid)
    {
        AddInput("Vector"_sid, PinDataType::Vec3, glm::vec3(0.0f));
        AddOutput("Result"_sid, PinDataType::Vec3);
    }

    void NormalizeNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        const glm::vec3 value = GetInputValue<glm::vec3>("Vector", evaluator);
        const float lengthSquared = glm::dot(value, value);
        if (lengthSquared <= std::numeric_limits<float>::epsilon())
        {
            evaluator.ReportError("Normalize received a zero-length vector");
            return;
        }
        SetOutputValue("Result", value / std::sqrt(lengthSquared), evaluator);
    }

    ClampNode::ClampNode(UUID id) : Node(id, "ClampNode"_sid)
    {
        AddInput("Value"_sid, PinDataType::Float, 0.0f);
        AddInput("Min"_sid, PinDataType::Float, 0.0f);
        AddInput("Max"_sid, PinDataType::Float, 1.0f);
        AddOutput("Result"_sid, PinDataType::Float);
    }

    void ClampNode::Evaluate(NodeGraphEvaluator& evaluator)
    {
        const float minimum = GetInputValue<float>("Min", evaluator);
        const float maximum = GetInputValue<float>("Max", evaluator);
        if (minimum > maximum)
        {
            evaluator.ReportError("Clamp minimum is greater than maximum");
            return;
        }
        SetOutputValue("Result", glm::clamp(GetInputValue<float>("Value", evaluator), minimum, maximum), evaluator);
    }

} // namespace Crowny
