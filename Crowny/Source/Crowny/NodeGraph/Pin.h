#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/NodeGraph/PinTypes.h"

namespace Crowny
{
    class Node;

    class Pin
    {
    public:
        enum class Direction
        {
            Input,
            Output
        };

        Pin(UUID id, const String& name, Direction direction, PinDataType dataType);

        UUID GetID() const { return m_ID; }
        void SetID(UUID id) { m_ID = id; }
        const String& GetName() const { return m_Name; }
        Direction GetDirection() const { return m_Direction; }
        PinDataType GetDataType() const { return m_DataType; }

        void SetDefaultValue(const PinValue& value);
        const PinValue& GetDefaultValue() const { return m_DefaultValue; }

        void SetConnectedPin(Pin* other) { m_ConnectedPin = other; }
        Pin* GetConnectedPin() const { return m_ConnectedPin; }
        bool IsConnected() const { return m_ConnectedPin != nullptr; }

        void SetOwner(Node* owner) { m_Owner = owner; }
        Node* GetOwner() const { return m_Owner; }

    private:
        UUID m_ID;
        String m_Name;
        Direction m_Direction = Direction::Input;
        PinDataType m_DataType = PinDataType::Float;
        PinValue m_DefaultValue;
        Pin* m_ConnectedPin = nullptr;
        Node* m_Owner = nullptr;
    };

} // namespace Crowny
