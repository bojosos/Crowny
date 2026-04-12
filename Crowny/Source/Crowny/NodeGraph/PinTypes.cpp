#include "cwpch.h"

#include "Crowny/NodeGraph/PinTypes.h"

namespace Crowny
{
    bool ArePinTypesCompatible(PinDataType output, PinDataType input)
    {
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
