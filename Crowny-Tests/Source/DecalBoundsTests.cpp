#include "Crowny/Scene/Scene.h"
#include "Editor/DecalBoundsInteraction.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <imgui.h>

using namespace Crowny;

namespace
{
    struct DecalViewportScope
    {
        ImGuiContext* Previous = ImGui::GetCurrentContext();
        ImGuiContext* Context = ImGui::CreateContext();
        DecalViewportScope()
        {
            ImGui::SetCurrentContext(Context);
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = { 640, 480 };
            io.DeltaTime = 1.0f / 60;
            io.ConfigInputTrickleEventQueue = false;
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            UndoRedo::StartUp();
        }
        ~DecalViewportScope()
        {
            UndoRedo::Shutdown();
            ImGui::DestroyContext(Context);
            ImGui::SetCurrentContext(Previous);
        }
        void Frame(Entity entity, DecalBoundsInteraction& interaction, ImVec2 cursor, bool down, bool escape = false)
        {
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent(cursor.x, cursor.y);
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
            io.AddKeyEvent(ImGuiKey_Escape, escape);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({ 0, 0 });
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Decal viewport test", nullptr, ImGuiWindowFlags_NoDecoration);
            interaction.Draw(entity, glm::mat4(1), { 0, 0, 640, 480 }, true, 0);
            ImGui::End();
            ImGui::EndFrame();
        }
    };
} // namespace

TEST_CASE("Decal viewport commits cursor movement delivered with mouse release", "[decals][Editor][Undo]")
{
    const bool cylinder = GENERATE(false, true);
    DecalViewportScope viewport;
    auto scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Wrap");
    auto& decal = entity.AddComponent<DecalComponent>();
    decal.Projection = cylinder ? DecalProjection::Cylinder : DecalProjection::Box;
    decal.PreserveTexelDensity = true;
    const DecalSettings before = decal;
    const float handleY = cylinder ? 120.0f : 240.0f;
    const auto extent = [&]() { return cylinder ? decal.TopRadius : decal.Size.x; };
    const float originalExtent = extent();
    DecalBoundsInteraction interaction;
    viewport.Frame(entity, interaction, { 480, handleY }, false);
    viewport.Frame(entity, interaction, { 480, handleY }, true);
    REQUIRE(interaction.IsUsing());
    viewport.Frame(entity, interaction, { 544, handleY }, false);
    CHECK_FALSE(interaction.IsUsing());
    CHECK(extent() == Catch::Approx(originalExtent + 0.2f));
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(static_cast<const DecalSettings&>(decal) == before);
    CHECK_FALSE(UndoRedo::Get().CanUndo());

    viewport.Frame(entity, interaction, { 480, handleY }, true);
    REQUIRE(interaction.IsUsing());
    viewport.Frame(entity, interaction, { 544, handleY }, true);
    CHECK(extent() > originalExtent);
    viewport.Frame(entity, interaction, { 576, handleY }, false, true);
    CHECK(static_cast<const DecalSettings&>(decal) == before);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
}

TEST_CASE("Decal bounds commit dimensions and density UVs in one undo action", "[decals][Editor][Undo]")
{
    UndoRedo::StartUp();
    auto scene = CreateRef<Scene>();
    Entity parent = scene->CreateEntity("Scaled parent");
    parent.GetTransform().SetScale({ 2, 3, 4 });
    Entity entity = scene->CreateEntity("Label");
    entity.SetParent(parent);
    auto& decal = entity.AddComponent<DecalComponent>();
    decal.PreserveTexelDensity = true;
    const DecalSettings before = decal;
    const auto transform = entity.GetTransform().GetWorldMatrix(parent);
    DecalBoundsInteraction interaction;
    REQUIRE(interaction.Begin(entity, 1));
    interaction.Update(0.31f, false, 0.25f);
    CHECK(decal.Size.x == Catch::Approx(1.25f));
    CHECK(decal.Offset.x - decal.Size.x * 0.5f == Catch::Approx(-0.5f));
    CHECK(decal.UVScale.x == Catch::Approx(1.25f));
    interaction.Update(0.61f, false, 0.25f);
    interaction.Commit();
    const DecalSettings after = decal;
    CHECK(entity.GetTransform().GetWorldMatrix(parent) == transform);
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(static_cast<const DecalSettings&>(decal) == before);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(static_cast<const DecalSettings&>(decal) == after);
    REQUIRE(interaction.Begin(entity, 0));
    interaction.Update(2, false);
    interaction.Cancel();
    CHECK(static_cast<const DecalSettings&>(decal) == after);
    UndoRedo::Shutdown();
}

TEST_CASE("Cylinder bounds keep the opposite cap fixed and clamp the angular span", "[decals][Editor]")
{
    DecalSettings before;
    before.Projection = DecalProjection::Cylinder;
    before.PreserveTexelDensity = true;
    const auto height = DecalBoundsInteraction::Resize(before, 3, 1, false);
    CHECK(height.Offset.y - height.Height * 0.5f == Catch::Approx(-0.5f));
    CHECK(height.UVScale.y == Catch::Approx(2));
    const auto arc = DecalBoundsInteraction::Resize(before, 5, -180, false);
    CHECK(arc.Arc == 180);
    CHECK(arc.UVScale.x == Catch::Approx(0.5f));
    CHECK(DecalBoundsInteraction::Resize(before, 5, 1000, false).Arc == 360);
    CHECK(DecalBoundsInteraction::Resize(before, 0, -100, false).BottomRadius > 0);
}

TEST_CASE("Cylinder wrap handles follow the tapered sleeve and keep seam controls separate", "[decals][Editor]")
{
    DecalSettings decal;
    decal.Projection = DecalProjection::Cylinder;
    decal.BottomRadius = 0.6f;
    decal.TopRadius = 0.3f;
    decal.Height = 1.4f;
    for (float arc : { 90.0f, 180.0f, 360.0f })
    {
        decal.Arc = arc;
        decal.SeamRotation = 30.0f;
        const auto handles = DecalBoundsInteraction::CylinderHandles(decal);
        // Full wraps must not stack the seam and coverage controls at one point.
        CHECK(glm::distance(handles[5].Position, handles[6].Position) > decal.Height * 0.4f);
        for (size_t i = 0; i < handles.size(); ++i)
        {
            const auto p = handles[i].Position;
            CHECK(p.y >= -decal.Height * 0.5f);
            CHECK(p.y <= decal.Height * 0.5f);
            const float expected =
              glm::mix(decal.BottomRadius, decal.TopRadius, p.y / decal.Height + 0.5f) + (i == 4 ? decal.ShellThickness * 0.5f : 0.0f);
            CHECK(glm::length(glm::vec2(p.x, p.z)) == Catch::Approx(expected));
            CHECK(glm::length(handles[i].Direction) > 0);
            CHECK(handles[i].Label[0] != '\0');
        }
        CHECK(handles[2].Position.y == Catch::Approx(-decal.Height * 0.5f));
        CHECK(handles[3].Position.y == Catch::Approx(decal.Height * 0.5f));
    }
}
