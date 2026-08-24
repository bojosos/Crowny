#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/MaterialPropertyBlock.h"

using namespace Crowny;

TEST_CASE("Shader property IDs are cached and stable", "[Renderer][Material]")
{
    const MaterialPropertyID first = Shader::PropertyToID("baseColor");
    const MaterialPropertyID second = Shader::PropertyToID("baseColor");
    const MaterialPropertyID other = Shader::PropertyToID("roughness");

    CHECK(first.IsValid());
    CHECK(first == second);
    CHECK(first != other);
}

TEST_CASE("MaterialPropertyBlock stores renderer overrides by property ID", "[Renderer][Material]")
{
    MaterialPropertyBlock block;
    const MaterialPropertyID color = Shader::PropertyToID("baseColor");
    const MaterialPropertyID roughness = Shader::PropertyToID("roughness");

    block.Set(color, glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
    block.Set(roughness, 0.4f);

    REQUIRE(block.Get<glm::vec4>(color) != nullptr);
    CHECK(*block.Get<glm::vec4>(color) == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f));
    REQUIRE(block.Get<float>(roughness) != nullptr);
    CHECK(*block.Get<float>(roughness) == 0.4f);
    CHECK(block.Get<int32_t>(roughness) == nullptr);

    const uint32_t revision = block.GetRevision();
    CHECK(block.Remove(color));
    CHECK(block.GetRevision() == revision + 1);
    CHECK_FALSE(block.Has(color));
}
