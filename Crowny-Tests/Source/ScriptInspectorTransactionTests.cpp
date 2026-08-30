#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Editor/ScriptInspectorTransaction.h"
#include "Panels/ScriptInspectorModel.h"

using namespace Crowny;

namespace
{
    ScriptState MakeScriptState(int64_t value)
    {
        const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "InspectorBehaviour" };
        ScriptState state;
        state.Identity = identity;
        state.Root = ScriptValue::Object({ { "Value", ScriptValue::Signed(value) } }, identity);
        return state;
    }

    int64_t GetValue(Entity entity)
    {
        return entity.GetComponent<ManagedScriptComponent>().Scripts.front().GetState().Root.Members.at("Value").SignedValue;
    }

    void SetValue(Entity entity, int64_t value)
    {
        ManagedScript& script = entity.GetComponent<ManagedScriptComponent>().Scripts.front();
        ScriptState state = script.GetState();
        state.Root.Members.at("Value") = ScriptValue::Signed(value);
        REQUIRE(script.SetState(std::move(state)));
    }
} // namespace

TEST_CASE("Managed inspector edits retained state without constructing a script", "[Editor][Scripting][Inspector]")
{
    const ScriptState retained = MakeScriptState(10);
    ManagedScript script(retained.Identity);
    REQUIRE(script.SetState(retained));

    REQUIRE_FALSE(script.GetRuntimeHandle().IsValid());
    ScriptInspectorModel model(script);
    REQUIRE(model.GetState().Identity == retained.Identity);
    REQUIRE(model.GetState().Root.Members.at("Value") == ScriptValue::Signed(10));
    model.GetState().Root.Members.at("Value") = ScriptValue::Signed(20);
    REQUIRE(model.Commit());

    CHECK_FALSE(script.GetRuntimeHandle().IsValid());
    CHECK(script.GetState().Root.Members.at("Value") == ScriptValue::Signed(20));
}

TEST_CASE("Managed inspector warm frames do not capture or allocate", "[Editor][Undo][Scripting][Memory]")
{
    constexpr size_t frameCount = 120u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    UndoRedo::StartUp();

    bool allScopesStarted = true;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (size_t frameIndex = 0u; frameIndex < frameCount; frameIndex++)
    {
        allScopesStarted &= UndoRedo::Get().BeginComponentScope(transaction);
        transaction->CompleteFrame();
        UndoRedo::Get().EndComponentScope();
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);
    UndoRedo::Shutdown();

    CHECK(allScopesStarted);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

TEST_CASE("Managed inspector captures redo state after the managed setter", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, false, false, false, true });
    SetValue(entity, 20);
    transaction->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    CHECK(UndoRedo::Get().GetUndoName() == "Edit script");
    UndoRedo::Get().Undo();
    CHECK(GetValue(entity) == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValue(entity) == 20);
    UndoRedo::Shutdown();
}

TEST_CASE("Managed inspector coalesces instantaneous changes from one frame", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, false, false, false, true });
    SetValue(entity, 20);
    UndoRedo::Get().OnItemInteract({ 43u, false, false, false, true });
    SetValue(entity, 30);
    transaction->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(GetValue(entity) == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValue(entity) == 30);
    UndoRedo::Shutdown();
}

TEST_CASE("Managed inspector finish and reset preserve redo state", "[Editor][Undo][Scripting]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptState script = MakeScriptState(10);
    REQUIRE(scene->AddScriptComponent(entity, script, false));

    Ref<ScriptInspectorTransaction> transaction = CreateRef<ScriptInspectorTransaction>();
    transaction->SetTarget(entity);
    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(transaction));
    UndoRedo::Get().OnItemInteract({ 42u, true, true, false, true });
    SetValue(entity, 25);
    UndoRedo::Get().EndComponentScope();
    UndoRedo::Get().FinishComponentScope(transaction);
    transaction->Reset();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(GetValue(entity) == 10);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(GetValue(entity) == 25);
    UndoRedo::Shutdown();
}
