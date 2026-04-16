#include <catch2/catch_test_macros.hpp>

#include "cwpch.h"
#include "Crowny/RenderAPI/Buffer.h"

using namespace Crowny;

TEST_CASE("BufferElement", "[RenderAPI][Buffer]")
{
    SECTION("Size Calculation")
    {
        CHECK(ShaderDataTypeSize(ShaderDataType::Float) == 4);
        CHECK(ShaderDataTypeSize(ShaderDataType::Float2) == 8);
        CHECK(ShaderDataTypeSize(ShaderDataType::Float3) == 12);
        CHECK(ShaderDataTypeSize(ShaderDataType::Float4) == 16);
        CHECK(ShaderDataTypeSize(ShaderDataType::Mat4) == 64);
        CHECK(ShaderDataTypeSize(ShaderDataType::Int) == 4);
        CHECK(ShaderDataTypeSize(ShaderDataType::Bool) == 1);
    }

    SECTION("Constructor")
    {
        BufferElement element(ShaderDataType::Float3, VertexAttribute::Position, true);
        CHECK(element.Type == ShaderDataType::Float3);
        CHECK(element.Attribute == VertexAttribute::Position);
        CHECK(element.Size == 12);
        CHECK(element.Normalized == true);
        CHECK(element.Offset == 0);
    }
}

TEST_CASE("BufferLayout", "[RenderAPI][Buffer]")
{
    SECTION("Empty Layout")
    {
        BufferLayout layout;
        CHECK(layout.GetStride() == 0);
        CHECK(layout.GetElements().size() == 0);
    }

    SECTION("Layout Calculation")
    {
        BufferLayout layout = {
            { ShaderDataType::Float3, VertexAttribute::Position },
            { ShaderDataType::Float2, VertexAttribute::TexCoord0 },
            { ShaderDataType::Float4, VertexAttribute::Color }
        };

        // 12 (Pos) + 8 (UV) + 16 (Color) = 36
        CHECK(layout.GetStride() == 36);
        
        const auto& elements = layout.GetElements();
        REQUIRE(elements.size() == 3);

        CHECK(elements[0].Offset == 0);
        CHECK(elements[0].Size == 12);

        CHECK(elements[1].Offset == 12);
        CHECK(elements[1].Size == 8);

        CHECK(elements[2].Offset == 20);
        CHECK(elements[2].Size == 16);
    }

    SECTION("Add Element Dynamically")
    {
        BufferLayout layout;
        layout.AddBufferElement({ ShaderDataType::Float3, VertexAttribute::Position });
        CHECK(layout.GetStride() == 12);

        layout.AddBufferElement({ ShaderDataType::Float3, VertexAttribute::Normal });
        CHECK(layout.GetStride() == 24);
        CHECK(layout.GetElements()[1].Offset == 12);
    }

    SECTION("Attribute Queries")
    {
        BufferLayout layout = {
            { ShaderDataType::Float3, VertexAttribute::Position },
            { ShaderDataType::Float2, VertexAttribute::TexCoord0 }
        };

        CHECK(layout.GetOffset(VertexAttribute::Position) == 0);
        CHECK(layout.GetOffset(VertexAttribute::TexCoord0) == 12);
        
        CHECK(layout.GetElementSize(VertexAttribute::Position) == 12);
        CHECK(layout.GetElementSize(VertexAttribute::TexCoord0) == 8);
    }

    SECTION("Iteration")
    {
        BufferLayout layout = {
            { ShaderDataType::Float3, VertexAttribute::Position },
            { ShaderDataType::Float3, VertexAttribute::Normal }
        };

        uint32_t count = 0;
        for (const auto& element : layout)
        {
            count++;
        }
        CHECK(count == 2);
    }
}
