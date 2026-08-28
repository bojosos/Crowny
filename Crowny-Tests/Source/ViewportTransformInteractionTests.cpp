#include "Editor/ViewportTransformInteraction.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Viewport transform interactions resolve commit and cancellation", "[Editor][Viewport][Undo]")
{
    CHECK(ResolveTransformInteractionCompletion(false, false, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(false, true, true) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, true, false) == TransformInteractionCompletion::None);
    CHECK(ResolveTransformInteractionCompletion(true, false, false) == TransformInteractionCompletion::Commit);
    CHECK(ResolveTransformInteractionCompletion(true, true, true) == TransformInteractionCompletion::Cancel);
    CHECK(ResolveTransformInteractionCompletion(true, false, true) == TransformInteractionCompletion::Cancel);
}
