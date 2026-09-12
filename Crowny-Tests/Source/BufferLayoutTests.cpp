#include <catch2/catch_test_macros.hpp>

#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

using namespace Crowny;

TEST_CASE("Deserialized buffer layouts get distinct runtime cache identities", "[Renderer][BufferLayout][Serialization]")
{
    const auto loadWithSharedSavedId = [](const BufferLayout& source, BufferLayout& restored) {
        auto stream = CreateRef<MemoryDataStream>(256);
        {
            BinaryDataStreamOutputArchive output(stream);
            output(source);
        }
        // Independent import processes can assign the same ID to different layouts.
        const uint32_t savedId = 42;
        stream->Seek(0);
        stream->Write(&savedId, sizeof(savedId));
        stream->Seek(0);
        BinaryDataStreamInputArchive input(stream);
        input(restored);
    };
    BufferLayout positions{ { ShaderDataType::Float3, "position" } };
    BufferLayout positionsAndNormals{ { ShaderDataType::Float3, "position" }, { ShaderDataType::Float3, "normal" } };
    BufferLayout first;
    BufferLayout second;
    loadWithSharedSavedId(positions, first);
    loadWithSharedSavedId(positionsAndNormals, second);
    CHECK(first.GetId() != second.GetId());
    CHECK(first.GetStride() == 3 * sizeof(float));
    CHECK(second.GetStride() == 6 * sizeof(float));
    const uint32_t previousId = first.GetId();
    loadWithSharedSavedId(positionsAndNormals, first);
    CHECK(first.GetId() != previousId);
    CHECK(first.GetId() != second.GetId());
    CHECK(first.GetStride() == 6 * sizeof(float));
}

TEST_CASE("Buffer layouts calculate independent stream strides", "[Renderer][BufferLayout]")
{
    BufferElement position(ShaderDataType::Float2, "position");
    BufferElement uv(ShaderDataType::Float2, "uv");
    BufferElement transform0(ShaderDataType::Float4, "transform0");
    transform0.StreamIdx = 1;
    transform0.InstanceRate = 1;
    BufferElement transform1(ShaderDataType::Float4, "transform1");
    transform1.StreamIdx = 1;
    transform1.InstanceRate = 1;
    BufferElement color(ShaderDataType::Float4, "color");
    color.StreamIdx = 1;
    color.InstanceRate = 1;

    BufferLayout layout{ position, uv, transform0, transform1, color };
    REQUIRE(layout.GetStreamCount() == 2);
    CHECK(layout.GetStride(0) == 4 * sizeof(float));
    CHECK(layout.GetStride(1) == 12 * sizeof(float));
    CHECK(layout.GetElements()[0].Offset == 0);
    CHECK(layout.GetElements()[1].Offset == 2 * sizeof(float));
    CHECK(layout.GetElements()[2].Offset == 0);
    CHECK(layout.GetElements()[3].Offset == 4 * sizeof(float));
    CHECK(layout.GetElements()[4].Offset == 8 * sizeof(float));
}
