#include "cwpch.h"

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    Pin::Pin(UUID id, StringID name, Direction direction, PinDataType dataType)
      : m_ID(id), m_Name(name), m_Direction(direction), m_DataType(dataType), m_DefaultValue(DefaultPinValue(dataType))
    {
    }

    bool Pin::SetDefaultValue(const PinValue& value)
    {
        PinValue convertedValue;
        if (!ConvertPinValue(value, m_DataType, convertedValue))
        {
            CW_ENGINE_ERROR("Cannot assign {0} default value to {1} pin '{2}'.", value.index(), PinDataTypeName(m_DataType), m_Name.c_str());
            return false;
        }
        if (m_DefaultValue == convertedValue)
            return true;
        m_DefaultValue = std::move(convertedValue);
        if (m_Owner)
            m_Owner->NotifyChanged();
        return true;
    }

} // namespace Crowny
