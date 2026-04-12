#include "cwpch.h"

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    Pin::Pin(UUID id, const String& name, Direction direction, PinDataType dataType)
      : m_ID(id), m_Name(name), m_Direction(direction), m_DataType(dataType), m_DefaultValue(DefaultPinValue(dataType))
    {
    }

    void Pin::SetDefaultValue(const PinValue& value)
    {
        m_DefaultValue = value;
        if (m_Owner)
            m_Owner->NotifyChanged();
    }

} // namespace Crowny
