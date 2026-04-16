#pragma once

#include "Crowny/Common/Flags.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/RenderAPI/Buffer.h"

#include <glm/glm.hpp>

namespace Crowny
{

    enum class ShaderParamFlag : uint32_t
    {
        None = 0,
        Internal = 1 << 0,
        HideInInspector = 1 << 1,
        Color = 1 << 2,
        HDR = 1 << 3,
    };

    using ShaderParamFlags = Flags<ShaderParamFlag>;
    CW_FLAGS_OPERATORS(ShaderParamFlag);

    enum class ShaderParamType
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        Bool,
        Mat3,
        Mat4,
        Color3,
        Color4,
        Texture2D,
        Texture3D,
        TextureCube,
        Sampler,
    };

    struct ShaderParameterDesc
    {
        String Identifier;
        String DisplayName;
        String BlockName;
        ShaderParamType Type;
        ShaderParamFlags Flags = ShaderParamFlag::None;
        uint32_t Offset = 0;
        uint32_t Set = 0;
        uint32_t Slot = 0;
        float RangeMin = 0.0f;
        float RangeMax = 0.0f;
        bool HasRange = false;
        uint32_t SortOrder = 0;
    };

    struct AnnotationSet
    {
        String DisplayName;
        bool IsColor = false;
        bool IsHDR = false;
        bool IsHidden = false;
        float RangeMin = 0.0f;
        float RangeMax = 0.0f;
        bool HasRange = false;
        String DefaultValueStr;
        bool HasDefault = false;

        template <typename Archive> void Serialize(Archive& archive)
        {
            archive(DisplayName, IsColor, IsHDR, IsHidden, RangeMin, RangeMax, HasRange, DefaultValueStr, HasDefault);
        }
    };

    template <typename T> struct ShaderDataTypeTrait;
    template <> struct ShaderDataTypeTrait<float> { static constexpr ShaderDataType Type = ShaderDataType::Float; };
    template <> struct ShaderDataTypeTrait<glm::vec2> { static constexpr ShaderDataType Type = ShaderDataType::Float2; };
    template <> struct ShaderDataTypeTrait<glm::vec3> { static constexpr ShaderDataType Type = ShaderDataType::Float3; };
    template <> struct ShaderDataTypeTrait<glm::vec4> { static constexpr ShaderDataType Type = ShaderDataType::Float4; };
    template <> struct ShaderDataTypeTrait<int> { static constexpr ShaderDataType Type = ShaderDataType::Int; };
    template <> struct ShaderDataTypeTrait<glm::ivec2> { static constexpr ShaderDataType Type = ShaderDataType::Int2; };
    template <> struct ShaderDataTypeTrait<glm::ivec3> { static constexpr ShaderDataType Type = ShaderDataType::Int3; };
    template <> struct ShaderDataTypeTrait<glm::ivec4> { static constexpr ShaderDataType Type = ShaderDataType::Int4; };
    template <> struct ShaderDataTypeTrait<bool> { static constexpr ShaderDataType Type = ShaderDataType::Bool; };
    template <> struct ShaderDataTypeTrait<glm::mat3> { static constexpr ShaderDataType Type = ShaderDataType::Mat3; };
    template <> struct ShaderDataTypeTrait<glm::mat4> { static constexpr ShaderDataType Type = ShaderDataType::Mat4; };

} // namespace Crowny
