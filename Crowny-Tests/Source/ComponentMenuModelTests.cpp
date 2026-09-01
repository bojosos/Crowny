#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Panels/ComponentMenuModel.h"

using namespace Crowny;

namespace
{
    ComponentMenuModel MakeMenu()
    {
        ComponentMenuModel menu;
        menu.AddComponent(4u, "Mesh Renderer", "Rendering");
        menu.AddComponent(1u, "Transform", "");
        menu.AddComponent(3u, "Box Collider 3D", "Physics");
        menu.AddComponent(2u, "Camera", "Rendering");
        menu.SetScripts(100u, {
                                { "PlayerController", true },
                                { "HiddenButDeclared", false },
                                { "CameraController", true },
                              });
        return menu;
    }
} // namespace

TEST_CASE("Component menu model retains sorted catalogs and filtered results", "[Editor][ComponentMenu]")
{
    ComponentMenuModel menu = MakeMenu();
    const Vector<ComponentMenuModel::ComponentEntry>& components = menu.GetComponents();

    REQUIRE(components.size() == 4u);
    CHECK(components[0].Name == "Transform");
    CHECK(components[0].Group == "Core");
    CHECK(components[0].Detail == "Core component");
    CHECK(components[1].Name == "Box Collider 3D");
    CHECK(components[2].Name == "Camera");
    CHECK(components[3].Name == "Mesh Renderer");

    const ComponentMenuModel::SearchResults& rendering = menu.Search("rendering");
    REQUIRE(rendering.ComponentIndices.size() == 2u);
    CHECK(components[rendering.ComponentIndices[0]].Name == "Camera");
    CHECK(components[rendering.ComponentIndices[1]].Name == "Mesh Renderer");
    CHECK(rendering.ScriptIndices.empty());

    const ComponentMenuModel::SearchResults& controller = menu.Search("controller");
    REQUIRE(controller.ScriptIndices.size() == 2u);
    const Vector<ComponentMenuModel::ScriptEntry>& scripts = menu.GetScripts();
    CHECK(scripts[controller.ScriptIndices[0]].Name == "CameraController");
    CHECK(scripts[controller.ScriptIndices[1]].Name == "PlayerController");

    const ComponentMenuModel::SearchResults& hidden = menu.Search("HiddenButDeclared");
    CHECK(hidden.GetMatchCount() == 0u);
    CHECK(hidden.ScriptNameDeclared);
    CHECK(hidden.CreateScriptLabel == "Create \"HiddenButDeclared.cs\"");
}

TEST_CASE("Component menu model projects sorted component categories", "[Editor][ComponentMenu]")
{
    ComponentMenuModel menu = MakeMenu();
    const Vector<ComponentMenuModel::CategoryEntry>& categories = menu.GetCategories();

    REQUIRE(categories.size() == 3u);
    CHECK(categories[0].Name == "Core");
    CHECK(categories[0].FirstComponentIndex == 0u);
    CHECK(categories[0].ComponentCount == 1u);
    CHECK(categories[1].Name == "Physics");
    CHECK(categories[1].FirstComponentIndex == 1u);
    CHECK(categories[1].ComponentCount == 1u);
    CHECK(categories[2].Name == "Rendering");
    CHECK(categories[2].FirstComponentIndex == 2u);
    CHECK(categories[2].ComponentCount == 2u);
}

TEST_CASE("Component menu model invalidates results only when inputs change", "[Editor][ComponentMenu]")
{
    ComponentMenuModel menu = MakeMenu();
    REQUIRE(menu.HasScriptCatalog(100u));
    CHECK(menu.Search("player").GetMatchCount() == 1u);

    menu.AddComponent(5u, "Player Input", "Input");
    CHECK(menu.Search("player").GetMatchCount() == 2u);
    REQUIRE(menu.GetCategories().size() == 4u);
    CHECK(menu.GetCategories()[1].Name == "Input");
    CHECK(menu.GetCategories()[1].FirstComponentIndex == 1u);
    CHECK(menu.GetCategories()[1].ComponentCount == 1u);

    menu.SetScripts(101u, { { "WorldController", true } });
    CHECK(menu.HasScriptCatalog(101u));
    CHECK_FALSE(menu.HasScriptCatalog(100u));
    CHECK(menu.Search("player").GetMatchCount() == 1u);
    CHECK(menu.Search("world").GetMatchCount() == 1u);
}

TEST_CASE("Component menu model allocates nothing for a stable visible query", "[Editor][ComponentMenu][Memory][Frame]")
{
    ComponentMenuModel menu = MakeMenu();
    const ComponentMenuModel::SearchResults& warmResults = menu.Search("rendering");
    const size_t* const componentIndices = warmResults.ComponentIndices.data();
    const char* const createLabel = warmResults.CreateScriptLabel.data();

    size_t observedMatches = 0u;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 240u; frame++)
    {
        observedMatches += menu.Search("rendering").GetMatchCount();
        observedMatches += menu.GetComponents().size();
        observedMatches += menu.GetCategories().size();
        observedMatches += menu.GetScripts().size();
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(observedMatches == 240u * 12u);
    CHECK(menu.Search("rendering").ComponentIndices.data() == componentIndices);
    CHECK(menu.Search("rendering").CreateScriptLabel.data() == createLabel);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
