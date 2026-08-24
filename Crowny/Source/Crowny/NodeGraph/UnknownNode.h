#pragma once

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    using namespace Literals;
    class UnknownNode final : public Node
    {
    public:
        UnknownNode(UUID id, StringID missingTypeName);

        Ref<Pin> AddSerializedPin(UUID id, StringID name, Pin::Direction direction, PinDataType type, const PinValue& defaultValue);
        void Evaluate(NodeGraphEvaluator& evaluator) override;
        StringID GetDisplayName() const override { return m_DisplayName; }
        StringID GetCategory() const override { return "Missing"_sid; }

    private:
        StringID m_DisplayName;
    };
} // namespace Crowny
