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
            CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!") return 0;
        };
        }

        return 0;
    }

    struct BufferElement
    {
        String Name;
        ShaderDataType Type;
        uint32_t Size;
        size_t Offset;
        bool Normalized;

        BufferElement() = default;

        BufferElement(ShaderDataType type, const String& name, bool normalized = false)
          : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Offset(0), Normalized(normalized)
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
                CW_ENGINE_ASSERT(false, "Unknown ShaderDataType!") return 0;
            };
            }

            return 0;
        }
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

        uint32_t GetStride() const { return m_Stride; }
        const Vector<BufferElement>& GetElements() const { return m_Elements; }

        Vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
        Vector<BufferElement>::iterator end() { return m_Elements.end(); }
        Vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        Vector<BufferElement>::const_iterator end() const { return m_Elements.end(); }

    private:
        void CalculateOffsetsAndStride()
        {
            size_t offset = 0;
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