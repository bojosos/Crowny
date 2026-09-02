#include "Editor/SelectionComponentOperations.h"

#include "Crowny/Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

namespace
{
    ManagedScript* FindScript(Entity entity, const ScriptTypeIdentity& identity)
    {
        if (!entity || !entity.HasComponent<ManagedScriptComponent>())
            return nullptr;
        for (ManagedScript& script : entity.GetComponent<ManagedScriptComponent>().Scripts)
        {
            if (script.GetTypeIdentity() == identity)
                return &script;
        }
        return nullptr;
    }

    void SetSignedValue(ManagedScript& script, String name, int64_t value)
    {
        ScriptState state = script.GetState();
        state.Root.Members.insert_or_assign(std::move(name), ScriptValue::Signed(value));
        REQUIRE(script.SetState(std::move(state)));
    }

    int64_t GetSignedValue(Entity entity, const ScriptTypeIdentity& identity, StringView name)
    {
        ManagedScript* script = FindScript(entity, identity);
        REQUIRE(script != nullptr);
        return script->GetState().Root.Members.at(String(name)).SignedValue;
    }
} // namespace

TEST_CASE("Adding a native component to a selection changes only missing entities", "[Editor][Inspector][Selection][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    MeshRendererComponent& existing = first.AddComponent<MeshRendererComponent>();
    existing.Materials.resize(2u);
    existing.LodBias = 3.5f;

    const Array<Entity, 4u> selection{ first, second, first, Entity{} };
    const SelectionComponentChange change = AddComponentToSelection<MeshRendererComponent>(selection);

    CHECK(change.TargetCount == 2u);
    CHECK(change.ChangedCount == 1u);
    REQUIRE(change.Action != nullptr);
    CHECK(change.Action->GetName() == "Add component");
    CHECK(first.GetComponent<MeshRendererComponent>().Materials.size() == 2u);
    CHECK(first.GetComponent<MeshRendererComponent>().LodBias == 3.5f);
    REQUIRE(second.HasComponent<MeshRendererComponent>());
    CHECK(second.GetComponent<MeshRendererComponent>().Materials.empty());
    CHECK(second.GetComponent<MeshRendererComponent>().LodBias == 0.0f);

    change.Action->Revert();
    CHECK(first.HasComponent<MeshRendererComponent>());
    CHECK_FALSE(second.HasComponent<MeshRendererComponent>());
    CHECK(first.GetComponent<MeshRendererComponent>().Materials.size() == 2u);

    change.Action->Commit();
    CHECK(first.HasComponent<MeshRendererComponent>());
    REQUIRE(second.HasComponent<MeshRendererComponent>());
    CHECK(second.GetComponent<MeshRendererComponent>().Materials.empty());
}

TEST_CASE("Selection component initialization applies only to newly added complex components", "[Editor][Inspector][Selection][Component][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity existing = scene->CreateEntity("Existing");
    Entity missing = scene->CreateEntity("Missing");
    existing.AddComponent<MeshRendererComponent>().LodBias = 4.0f;
    const Array<Entity, 2u> selection{ existing, missing };

    size_t initialized = 0u;
    const SelectionComponentChange change =
      AddComponentToSelection<MeshRendererComponent>(selection, [&](Entity entity, MeshRendererComponent& component) {
          CHECK(entity == missing);
          component.LodBias = 1.5f;
          component.Materials.resize(3u);
          ++initialized;
      });

    CHECK(initialized == 1u);
    CHECK(change.ChangedCount == 1u);
    REQUIRE(change.Action != nullptr);
    CHECK(existing.GetComponent<MeshRendererComponent>().LodBias == 4.0f);
    CHECK(missing.GetComponent<MeshRendererComponent>().LodBias == 1.5f);
    CHECK(missing.GetComponent<MeshRendererComponent>().Materials.size() == 3u);

    change.Action->Revert();
    CHECK(existing.HasComponent<MeshRendererComponent>());
    CHECK_FALSE(missing.HasComponent<MeshRendererComponent>());

    change.Action->Commit();
    REQUIRE(missing.HasComponent<MeshRendererComponent>());
    CHECK(missing.GetComponent<MeshRendererComponent>().LodBias == 1.5f);
    CHECK(missing.GetComponent<MeshRendererComponent>().Materials.size() == 3u);
}

TEST_CASE("Adding a managed script to a selection preserves existing script lists and state", "[Editor][Inspector][Selection][Scripting][Undo]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("Already attached");
    Entity second = scene->CreateEntity("Has another script");
    Entity third = scene->CreateEntity("No scripts");
    const ScriptTypeIdentity targetIdentity{ "Missing.Assembly", "Game", "TargetBehaviour" };
    const ScriptTypeIdentity otherIdentity{ "Missing.Assembly", "Game", "OtherBehaviour" };

    REQUIRE(scene->AddScriptComponent(first, targetIdentity, false));
    REQUIRE(scene->AddScriptComponent(second, otherIdentity, false));
    ManagedScript* targetScript = FindScript(first, targetIdentity);
    ManagedScript* otherScript = FindScript(second, otherIdentity);
    REQUIRE(targetScript != nullptr);
    REQUIRE(otherScript != nullptr);
    SetSignedValue(*targetScript, "Health", 27);
    SetSignedValue(*otherScript, "Order", 9);

    const Array<Entity, 5u> selection{ first, second, third, second, Entity{} };
    const SelectionComponentChange change = AddManagedScriptToSelection(selection, targetIdentity, false);

    CHECK(change.TargetCount == 3u);
    CHECK(change.ChangedCount == 2u);
    REQUIRE(change.Action != nullptr);
    CHECK(change.Action->GetName() == "Add scripts");
    CHECK(GetSignedValue(first, targetIdentity, "Health") == 27);
    CHECK(GetSignedValue(second, otherIdentity, "Order") == 9);
    CHECK(FindScript(second, targetIdentity) != nullptr);
    CHECK(FindScript(third, targetIdentity) != nullptr);

    change.Action->Revert();
    CHECK(GetSignedValue(first, targetIdentity, "Health") == 27);
    CHECK(GetSignedValue(second, otherIdentity, "Order") == 9);
    CHECK(FindScript(second, targetIdentity) == nullptr);
    CHECK_FALSE(third.HasComponent<ManagedScriptComponent>());

    change.Action->Commit();
    CHECK(GetSignedValue(first, targetIdentity, "Health") == 27);
    CHECK(GetSignedValue(second, otherIdentity, "Order") == 9);
    CHECK(FindScript(second, targetIdentity) != nullptr);
    CHECK(FindScript(third, targetIdentity) != nullptr);

    const SelectionComponentChange noChange = AddManagedScriptToSelection(selection, targetIdentity, false);
    CHECK(noChange.TargetCount == 3u);
    CHECK(noChange.ChangedCount == 0u);
    CHECK(noChange.Action == nullptr);
}

TEST_CASE("Invalid managed script identities do not mutate a selection", "[Editor][Inspector][Selection][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    const Array<Entity, 2u> selection{ first, second };

    const SelectionComponentChange change = AddManagedScriptToSelection(selection, ScriptTypeIdentity{}, false);

    CHECK(change.TargetCount == 0u);
    CHECK(change.ChangedCount == 0u);
    CHECK(change.Action == nullptr);
    CHECK_FALSE(first.HasComponent<ManagedScriptComponent>());
    CHECK_FALSE(second.HasComponent<ManagedScriptComponent>());
}
