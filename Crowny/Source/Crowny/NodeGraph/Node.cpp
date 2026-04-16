#include "cwpch.h"

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    Node::Node(UUID id, StringID typeName) : m_ID(id), m_TypeName(typeName) {}

    void Node::SetParentGraph(NodeGraph* graph) { m_ParentGraph = graph; }

    void Node::NotifyChanged()
    {
        if (m_ParentGraph)
            m_ParentGraph->NotifyChanged();
    }

    Pin* Node::FindInputPin(StringID name) const
    {
        for (const auto& pin : m_Inputs)
        {
            if (pin->GetName() == name)
                return pin.get();
        }
        return nullptr;
    }
    Pin* Node::FindOutputPin(StringID name) const
    {
        for (const auto& pin : m_Outputs)
        {
            if (pin->GetName() == name)
                return pin.get();
        }
        return nullptr;
    }

    Pin* Node::FindPinByID(UUID pinId) const
    {
        for (const auto& pin : m_Inputs)
        {
            if (pin->GetID() == pinId)
                return pin.get();
        }
        for (const auto& pin : m_Outputs)
        {
            if (pin->GetID() == pinId)
                return pin.get();
        }
        return nullptr;
    }

    Ref<Pin> Node::AddInput(StringID name, PinDataType type, const PinValue& defaultVal)
    {
        auto pin = CreateRef<Pin>(UuidGenerator::Generate(), name, Pin::Direction::Input, type);
        pin->SetOwner(this);
        if (!std::holds_alternative<float>(defaultVal) || std::get<float>(defaultVal) != 0.0f || type != PinDataType::Float)
            pin->SetDefaultValue(defaultVal);
        else
            pin->SetDefaultValue(DefaultPinValue(type));
        m_Inputs.push_back(pin);
        return pin;
    }

    Ref<Pin> Node::AddOutput(StringID name, PinDataType type)
    {
        auto pin = CreateRef<Pin>(UuidGenerator::Generate(), name, Pin::Direction::Output, type);
        pin->SetOwner(this);
        m_Outputs.push_back(pin);
        return pin;
    }

    template <typename T> T Node::GetInputValue(StringID pinName, NodeGraphEvaluator& evaluator) const
    {
        Pin* pin = FindInputPin(pinName);
        CW_ENGINE_ASSERT(pin, String("Input pin not found: ") + pinName.c_str());

        PinValue value = evaluator.PullInput(pin);
        if (std::holds_alternative<T>(value))
            return std::get<T>(value);

        // Handle type conversions
        if constexpr (std::is_same_v<T, float>)
        {
            if (std::holds_alternative<int32_t>(value))
                return static_cast<float>(std::get<int32_t>(value));
        }
        if constexpr (std::is_same_v<T, glm::vec2>)
        {
            if (std::holds_alternative<float>(value))
                return glm::vec2(std::get<float>(value));
        }
        if constexpr (std::is_same_v<T, glm::vec3>)
        {
            if (std::holds_alternative<float>(value))
                return glm::vec3(std::get<float>(value));
        }
        if constexpr (std::is_same_v<T, glm::vec4>)
        {
            if (std::holds_alternative<float>(value))
                return glm::vec4(std::get<float>(value));
        }

        return T{};
    }

    template <typename T> void Node::SetOutputValue(StringID pinName, const T& value, NodeGraphEvaluator& evaluator)
    {
        Pin* pin = FindOutputPin(pinName);
        CW_ENGINE_ASSERT(pin, String("Output pin not found: ") + pinName.c_str());
        evaluator.SetOutputValue(pin->GetID(), PinValue(value));
    }

    // Explicit template instantiations
    template float Node::GetInputValue<float>(StringID, NodeGraphEvaluator&) const;
    template int32_t Node::GetInputValue<int32_t>(StringID, NodeGraphEvaluator&) const;
    template glm::vec2 Node::GetInputValue<glm::vec2>(StringID, NodeGraphEvaluator&) const;
    template glm::vec3 Node::GetInputValue<glm::vec3>(StringID, NodeGraphEvaluator&) const;
    template glm::vec4 Node::GetInputValue<glm::vec4>(StringID, NodeGraphEvaluator&) const;
    template bool Node::GetInputValue<bool>(StringID, NodeGraphEvaluator&) const;
    template Ref<MeshData> Node::GetInputValue<Ref<MeshData>>(StringID, NodeGraphEvaluator&) const;

    template void Node::SetOutputValue<float>(StringID, const float&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<int32_t>(StringID, const int32_t&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec2>(StringID, const glm::vec2&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec3>(StringID, const glm::vec3&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec4>(StringID, const glm::vec4&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<bool>(StringID, const bool&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<Ref<MeshData>>(StringID, const Ref<MeshData>&, NodeGraphEvaluator&);
    template <> void Node::SetOutputValue<PinValue>(StringID pinName, const PinValue& pinValue, NodeGraphEvaluator& evaluator)
    {
        Pin* pin = FindOutputPin(pinName);
        CW_ENGINE_ASSERT(pin, "Output pin not found: " + String(pinName.c_str()));
        evaluator.SetOutputValue(pin->GetID(), pinValue);
    }

} // namespace Crowny
