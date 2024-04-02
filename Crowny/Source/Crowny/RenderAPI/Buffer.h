#pragma once

namespace Crowny
{
    enum class ShaderDataType
    {
        None = 0,
        Byte,
        Byte2,
        Byte3,
        Byte4,
        UByte4,
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

    static uint32_t ShaderDataTypeSize(ShaderDataType type)
    {
        switch (type)
        {
        case ShaderDataType::Byte:
            return 1 * 1;
        case ShaderDataType::Byte2:
            return 2 * 1;
        case ShaderDataType::Byte3:
            return 3 * 1;
        case ShaderDataType::Byte4:
            return 4 * 1;
        case ShaderDataType::UByte4:
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
        // case ShaderDataType::Bool:     return 1;
        case ShaderDataType::None: {
            CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
            return 0;
        };
        }

        return 0;
    }

    enum class VertexAttribute
    {
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
        BlendIndices
    };

    struct BufferElement
    {
        String Name;
        VertexAttribute Attribute;
        ShaderDataType Type;
        uint32_t Size;
        uint32_t Offset;
        bool Normalized;

        BufferElement() = default;

        BufferElement(ShaderDataType type, VertexAttribute attribute, bool normalized = false)
          : Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized), Attribute(attribute)
        {
        }

        BufferElement(ShaderDataType type, const String& name, bool normalized = false)
          : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized),
            Attribute(VertexAttribute::Position)
        {
        }

        uint32_t GetComponentCount() const
        {
            switch (Type)
            {
            case ShaderDataType::Byte:
                return 1;
            case ShaderDataType::Byte2:
                return 2;
            case ShaderDataType::Byte3:
                return 3;
            case ShaderDataType::Byte4:
                return 4;
            case ShaderDataType::UByte4:
                return 4;
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
            // case ShaderDataType::Bool:    return 1;
            case ShaderDataType::None: {
                CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!");
                return 0;
            };
            }

            return 0;
        }
        CW_SIMPLESERIALZABLE(BufferElement);
    };

    class BufferLayout
    {
    public:
        BufferLayout() {}

        BufferLayout(std::initializer_list<BufferElement> elements) : m_Elements(elements)
        {
            CalculateOffsetsAndStride();
        }

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

        uint32_t GetStride() const { return m_Stride; }
        const Vector<BufferElement>& GetElements() const { return m_Elements; }

        Vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
        Vector<BufferElement>::iterator end() { return m_Elements.end(); }
        Vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        Vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

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

    private:
        CW_SERIALIZABLE(BufferLayout);
        void CalculateOffsetsAndStride()
        {
            uint32_t offset = 0;
            m_Stride = 0;
            for (auto& element : m_Elements)
            {
                element.Offset = offset;
                offset += element.Size;
                m_Stride += element.Size;
            }
        }

    private:
        Vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };

} // namespace Crowny