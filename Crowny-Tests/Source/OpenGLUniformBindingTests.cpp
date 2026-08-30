#include <catch2/catch_test_macros.hpp>

#include "Platform/OpenGL/OpenGLUniformParams.h"

using namespace Crowny;

TEST_CASE("OpenGL fixed texture arrays clear every reflected slot", "[Renderer][OpenGL][Uniforms]")
{
    const OpenGLTextureBindingPlan partial = BuildOpenGLTextureBindingPlan(17, 32, 4, false, 2, false);
    CHECK(partial.FirstUnit == 17);
    CHECK(partial.UnitCount == 4);
    CHECK(partial.AssignedCount == 2);

    const OpenGLTextureBindingPlan single = BuildOpenGLTextureBindingPlan(3, 32, 4, false, 0, true);
    CHECK(single.UnitCount == 4);
    CHECK(single.AssignedCount == 1);

    const OpenGLTextureBindingPlan empty = BuildOpenGLTextureBindingPlan(5, 32, 4, false, 0, false);
    CHECK(empty.UnitCount == 4);
    CHECK(empty.AssignedCount == 0);
}

TEST_CASE("OpenGL runtime texture arrays bind assigned slots without scanning unused units", "[Renderer][OpenGL][Uniforms]")
{
    const OpenGLTextureBindingPlan populated = BuildOpenGLTextureBindingPlan(16, 32, 1, true, 7, false);
    CHECK(populated.UnitCount == 7);
    CHECK(populated.AssignedCount == 7);

    const OpenGLTextureBindingPlan empty = BuildOpenGLTextureBindingPlan(16, 32, 1, true, 0, false);
    CHECK(empty.UnitCount == 1);
    CHECK(empty.AssignedCount == 0);
}

TEST_CASE("OpenGL texture binding plans stay inside the device unit range", "[Renderer][OpenGL][Uniforms]")
{
    const OpenGLTextureBindingPlan clipped = BuildOpenGLTextureBindingPlan(30, 32, 8, false, 8, false);
    CHECK(clipped.UnitCount == 2);
    CHECK(clipped.AssignedCount == 2);

    const OpenGLTextureBindingPlan unavailable = BuildOpenGLTextureBindingPlan(32, 32, 1, false, 1, false);
    CHECK(unavailable.UnitCount == 0);
    CHECK(unavailable.AssignedCount == 0);
}
