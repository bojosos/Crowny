#pragma once

#include "Crowny/Common/Assert.h"
#include "Crowny/Common/Common.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Utils/SmallVector.h"

namespace Crowny
{
    enum class ShaderDataType
    {
        None = 0,
        Bool,
        SByte,
        SByte2,
        SByte3,
        SByte4,
        UByte4,
        Color, // 8 bits per channel color
        Float,
        Float2,
        Float3,
        Float4,
        Mat3,
        Mat4,
        Int,
        Int2,
        Int3,
        Int4
    };

    static String ShaderDataTypeToString(ShaderDataType type)
    {
        switch (type)
        {
        case ShaderDataType::Bool:
            return "Bool";
        case ShaderDataType::SByte:
            return "SByte";
        case ShaderDataType::SByte2:
            return "SByte2";
        case ShaderDataType::SByte3:
            return "SByte3";
        case ShaderDataType::SByte4:
            return "SByte4";
        case ShaderDataType::UByte4:
            return "UByte4";
        case ShaderDataType::Color:
            return "Color";
        case ShaderDataType::Float:
            return "Float";
        case ShaderDataType::Float2:
            return "Float2";
        case ShaderDataType::Float3:
            return "Float3";
        case ShaderDataType::Float4:
            return "Float4";
        case ShaderDataType::Mat3:
            return "Mat3";
        case ShaderDataType::Mat4:
            return "Mat4";
        case ShaderDataType::Int:
            return "Int";
        case ShaderDataType::Int2:
            return "Int2";
        case ShaderDataType::Int3:
            return "Int3";
        case ShaderDataType::Int4:
            return "Int4";
        case ShaderDataType::None: {
            CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
            return "yep";
        };
        }
        return "yep";
    }

    static uint32_t ShaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
        case ShaderDataType::Bool:
            return 1 * 1;
        case ShaderDataType::SByte:
            return 1 * 1;
        case ShaderDataType::SByte2:
            return 2 * 1;
        case ShaderDataType::SByte3:
            return 3 * 1;
        case ShaderDataType::SByte4:
            return 4 * 1;
        case ShaderDataType::UByte4:
            return 4 * 1;
        case ShaderDataType::Color: // U32
            return 4 * 1;
        case ShaderDataType::Float:
            return 4;
        case ShaderDataType::Float2:
            return 4 * 2;
        case ShaderDataType::Float3:
            return 4 * 3;
        case ShaderDataType::Float4:
            return 4 * 4;
        case ShaderDataType::Mat3:
            return 4 * 3 * 3;
        case ShaderDataType::Mat4:
            return 4 * 4 * 4;
        case ShaderDataType::Int:
            return 4;
        case ShaderDataType::Int2:
            return 4 * 2;
        case ShaderDataType::Int3:
            return 4 * 3;
        case ShaderDataType::Int4:
            return 4 * 4;
        case ShaderDataType::None: {
            CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
            return 0;
        };
        }

        return 0;
    }

    enum class VertexAttribute
    {
        None,
        Position,
        Normal,
        Tangent,
        Bitangent,
        Color,
        TexCoord0,
        TexCoord1,
        TexCoord2,
        TexCoord3,
        TexCoord4,
        TexCoord5,
        TexCoord6,
        TexCoord7,
        BlendWeights,
        BlendIndices,
        PreviousPosition
    };

    struct BufferElement
    {
        String Name;
        VertexAttribute Attribute;
        ShaderDataType Type;
        uint32_t Size;
        uint32_t Offset;
        uint32_t StreamIdx;
        uint32_t InstanceRate;
        uint32_t Location = UINT32_MAX; // SPIR-V location for shader inputs, UINT32_MAX if unset
        bool Normalized;

        BufferElement()
          : StreamIdx(0), Size(0), Offset(0), Normalized(false), Attribute(VertexAttribute::None), Type(ShaderDataType::Float), InstanceRate(0)
        {
        }

        BufferElement(ShaderDataType type, VertexAttribute attribute, bool normalized = false)
          : Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized), Attribute(attribute), StreamIdx(0), InstanceRate(0)
        {
        }

        BufferElement(ShaderDataType type, const String& name, bool normalized = false)
          : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized), Attribute(VertexAttribute::Position),
            StreamIdx(0), InstanceRate(0)
        {
        }

        uint32_t GetComponentCount() const
        {
            switch (Type)
            {
            case ShaderDataType::Bool:
                return 1;
            case ShaderDataType::SByte:
                return 1;
            case ShaderDataType::SByte2:
                return 2;
            case ShaderDataType::SByte3:
                return 3;
            case ShaderDataType::SByte4:
                return 4;
            case ShaderDataType::UByte4:
                return 4;
            case ShaderDataType::Color:
                return 1;
            case ShaderDataType::Float:
                return 1;
            case ShaderDataType::Float2:
                return 2;
            case ShaderDataType::Float3:
                return 3;
            case ShaderDataType::Float4:
                return 4;
            case ShaderDataType::Mat3:
                return 3 * 3;
            case ShaderDataType::Mat4:
                return 4 * 4;
            case ShaderDataType::Int:
                return 1;
            case ShaderDataType::Int2:
                return 2;
            case ShaderDataType::Int3:
                return 3;
            case ShaderDataType::Int4:
                return 4;
            case ShaderDataType::None: {
                CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
                return 0;
            };
            }

            return 0;
        }
        CW_SIMPLESERIALIZABLE(BufferElement);
    };

    class BufferLayout : public RefCounted
    {
    public:
        BufferLayout() : m_Id(s_NextFreeId++) {}

        BufferLayout(std::initializer_list<BufferElement> elements) : m_Elements(elements), m_Id(s_NextFreeId++) { CalculateOffsetsAndStride(); }

        BufferLayout& operator=(std::initializer_list<BufferElement> elements)
        {
            m_Elements = elements;
            CalculateOffsetsAndStride();
            return *this;
        }

        void AddBufferElement(const BufferElement& element)
        {
            m_Elements.push_back(element);
            CalculateOffsetsAndStride();
        }

        uint32_t GetStride(uint32_t streamIdx = 0) const
        {
            return streamIdx < m_StreamStrides.size() ? m_StreamStrides[streamIdx] : 0;
        }
        uint32_t GetStreamCount() const { return m_StreamCount; }
        const SmallVector<BufferElement, 8>& GetElements() const { return m_Elements; }

        typename SmallVector<BufferElement, 8>::iterator begin() { return m_Elements.begin(); }
        typename SmallVector<BufferElement, 8>::iterator end() { return m_Elements.end(); }
        typename SmallVector<BufferElement, 8>::const_iterator begin() const { return m_Elements.begin(); }
        typename SmallVector<BufferElement, 8>::const_iterator end() const { return m_Elements.end(); }

        uint32_t GetOffset(VertexAttribute attribute) const
        {
            for (const auto& element : m_Elements)
                if (element.Attribute == attribute)
                    return element.Offset;
            return 0;
        }

        uint32_t GetElementSize(VertexAttribute attribute) const
        {
            for (const auto& element : m_Elements)
            {
                if (element.Attribute == attribute)
                    return element.Size;
            }
            CW_ENGINE_ASSERT(false);
            return 0;
        }

        bool HasAttribute(VertexAttribute attribute) const
        {
            for (const auto& element : m_Elements)
                if (element.Attribute == attribute)
                    return true;
            return false;
        }

        bool IsCompatible(const BufferLayout& shaderLayout) const
        {
            for (const auto& shaderElement : shaderLayout.m_Elements)
            {
                bool found = false;
                for (const auto& meshElement : m_Elements)
                {
                    if (meshElement.Attribute == shaderElement.Attribute)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return false;
            }
            return true;
        }

        SmallVector<BufferElement, 8> GetMissingElements(const BufferLayout& shaderLayout) const
        {
            SmallVector<BufferElement, 8> missing;
            for (const auto& shaderElement : shaderLayout.m_Elements)
            {
                bool found = false;
                for (const auto& meshElement : m_Elements)
                {
                    if (meshElement.Attribute == shaderElement.Attribute)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    missing.push_back(shaderElement);
            }
            return missing;
        }

        uint32_t GetId() const { return m_Id; }

    private:
        CW_SERIALIZABLE(BufferLayout);
        void CalculateOffsetsAndStride()
        {
            m_StreamStrides.fill(0);
            m_StreamCount = 0;
            m_Stride = 0;
            for (auto& element : m_Elements)
            {
                CW_ENGINE_ASSERT(element.StreamIdx < m_StreamStrides.size(), "Vertex stream index exceeds the layout limit");
                if (element.StreamIdx >= m_StreamStrides.size())
                    element.StreamIdx = 0;
                element.Offset = m_StreamStrides[element.StreamIdx];
                m_StreamStrides[element.StreamIdx] += element.Size;
                m_StreamCount = std::max(m_StreamCount, element.StreamIdx + 1u);
            }
            m_Stride = m_StreamStrides[0];
        }

    private:
        /* const */ uint32_t m_Id;
        static uint32_t s_NextFreeId; // TODO:
        SmallVector<BufferElement, 8> m_Elements;
        uint32_t m_Stride = 0;
        std::array<uint32_t, 8> m_StreamStrides{};
        uint32_t m_StreamCount = 0;
    };

} // namespace Crowny
