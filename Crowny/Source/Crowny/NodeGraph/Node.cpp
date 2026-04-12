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

    Pin* Node::FindInputPin(const String& name) const
    {
        for (const auto& pin : m_Inputs)
        {
            if (pin->GetName() == name)
                return pin.get();
        }
        return nullptr;
    }
    Pin* Node::FindOutputPin(const String& name) const
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

    Ref<Pin> Node::AddInput(const String& name, PinDataType type, const PinValue& defaultVal)
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

    Ref<Pin> Node::AddOutput(const String& name, PinDataType type)
    {
        auto pin = CreateRef<Pin>(UuidGenerator::Generate(), name, Pin::Direction::Output, type);
        pin->SetOwner(this);
        m_Outputs.push_back(pin);
        return pin;
    }

    template <typename T> T Node::GetInputValue(const String& pinName, NodeGraphEvaluator& evaluator) const
    {
        Pin* pin = FindInputPin(pinName);
        CW_ENGINE_ASSERT(pin, "Input pin not found: " + pinName);

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

    template <typename T> void Node::SetOutputValue(const String& pinName, const T& value, NodeGraphEvaluator& evaluator)
    {
        Pin* pin = FindOutputPin(pinName);
        CW_ENGINE_ASSERT(pin, "Output pin not found: " + pinName);
        evaluator.SetOutputValue(pin->GetID(), PinValue(value));
    }

    // Explicit template instantiations
    template float Node::GetInputValue<float>(const String&, NodeGraphEvaluator&) const;
    template int32_t Node::GetInputValue<int32_t>(const String&, NodeGraphEvaluator&) const;
    template glm::vec2 Node::GetInputValue<glm::vec2>(const String&, NodeGraphEvaluator&) const;
    template glm::vec3 Node::GetInputValue<glm::vec3>(const String&, NodeGraphEvaluator&) const;
    template glm::vec4 Node::GetInputValue<glm::vec4>(const String&, NodeGraphEvaluator&) const;
    template bool Node::GetInputValue<bool>(const String&, NodeGraphEvaluator&) const;
    template Ref<MeshData> Node::GetInputValue<Ref<MeshData>>(const String&, NodeGraphEvaluator&) const;

    template void Node::SetOutputValue<float>(const String&, const float&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<int32_t>(const String&, const int32_t&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec2>(const String&, const glm::vec2&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec3>(const String&, const glm::vec3&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<glm::vec4>(const String&, const glm::vec4&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<bool>(const String&, const bool&, NodeGraphEvaluator&);
    template void Node::SetOutputValue<Ref<MeshData>>(const String&, const Ref<MeshData>&, NodeGraphEvaluator&);

} // namespace Crowny
