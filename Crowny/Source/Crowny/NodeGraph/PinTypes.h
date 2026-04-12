#pragma once

#include <glm/glm.hpp>
#include <variant>

namespace Crowny
{
    class MeshData;

    enum class PinDataType : uint32_t
    {
        Float,
        Int,
        Vec2,
        Vec3,
        Vec4,
        Bool,
        MeshData,
        Any
    };

    using PinValue = std::variant<float, int32_t, glm::vec2, glm::vec3, glm::vec4, bool, Ref<MeshData>>;

    inline PinValue DefaultPinValue(PinDataType type)
    {
        switch (type)
        {
        case PinDataType::Float:
            return 0.0f;
        case PinDataType::Int:
            return 0;
        case PinDataType::Vec2:
            return glm::vec2(0.0f);
        case PinDataType::Vec3:
            return glm::vec3(0.0f);
        case PinDataType::Vec4:
            return glm::vec4(0.0f);
        case PinDataType::Bool:
            return false;
        case PinDataType::MeshData:
            return Ref<MeshData>(nullptr);
        case PinDataType::Any:
            return 0.0f;
        }
        return 0.0f;
    }

    bool ArePinTypesCompatible(PinDataType output, PinDataType input);

    const char* PinDataTypeName(PinDataType type);

} // namespace Crowny
