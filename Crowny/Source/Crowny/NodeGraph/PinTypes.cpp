#include "cwpch.h"

#include "Crowny/NodeGraph/PinTypes.h"

namespace Crowny
{
    bool IsPinDataTypeValid(PinDataType type) { return static_cast<uint32_t>(type) <= static_cast<uint32_t>(PinDataType::Any); }

    bool ArePinTypesCompatible(PinDataType output, PinDataType input)
    {
        if (!IsPinDataTypeValid(output) || !IsPinDataTypeValid(input))
            return false;
        if (output == input)
            return true;

        if (output == PinDataType::Any || input == PinDataType::Any)
            return true;

        // Float can broadcast to vector types
        if (output == PinDataType::Float)
        {
            return input == PinDataType::Vec2 || input == PinDataType::Vec3 || input == PinDataType::Vec4;
        }

        // Int can convert to float
        if (output == PinDataType::Int && input == PinDataType::Float)
            return true;

        return false;
    }

    bool ConvertPinValue(const PinValue& value, PinDataType targetType, PinValue& convertedValue)
    {
        if (!IsPinDataTypeValid(targetType))
            return false;
        if (targetType == PinDataType::Any)
        {
            convertedValue = value;
            return true;
        }

        const size_t targetIndex = static_cast<size_t>(targetType);
        if (value.index() == targetIndex)
        {
            convertedValue = value;
            return true;
        }
        if (targetType == PinDataType::Float && std::holds_alternative<int32_t>(value))
        {
            convertedValue = static_cast<float>(std::get<int32_t>(value));
            return true;
        }
        if (std::holds_alternative<float>(value))
        {
            const float scalar = std::get<float>(value);
            if (targetType == PinDataType::Vec2)
                convertedValue = glm::vec2(scalar);
            else if (targetType == PinDataType::Vec3)
                convertedValue = glm::vec3(scalar);
            else if (targetType == PinDataType::Vec4)
                convertedValue = glm::vec4(scalar);
            else
                return false;
            return true;
        }
        return false;
    }

    const char* PinDataTypeName(PinDataType type)
    {
        switch (type)
        {
        case PinDataType::Float:
            return "Float";
        case PinDataType::Int:
            return "Int";
        case PinDataType::Vec2:
            return "Vec2";
        case PinDataType::Vec3:
            return "Vec3";
        case PinDataType::Vec4:
            return "Vec4";
        case PinDataType::Bool:
            return "Bool";
        case PinDataType::MeshData:
            return "MeshData";
        case PinDataType::Any:
            return "Any";
        }
        return "Unknown";
    }

} // namespace Crowny
