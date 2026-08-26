#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
#include "Editor/ComponentUndoSnapshot.h"
#include "Editor/UndoRedo.h"

using namespace Crowny;

namespace
{
    struct TestScriptFields
    {
        Ref<SerializableObject> Object;
        Ref<SerializableMemberInfo> Field;
    };

    TestScriptFields MakeScriptFields(const ScriptTypeIdentity& identity, int32_t value)
    {
        Ref<SerializableTypeInfoObject> objectType = CreateRef<SerializableTypeInfoObject>();
        objectType->m_TypeNamespace = identity.Namespace;
        objectType->m_TypeName = identity.TypeName;
        objectType->m_TypeId = 1;
        objectType->m_Flags = ScriptFieldFlagBits::Serializable;

        Ref<SerializableObjectInfo> objectInfo = CreateRef<SerializableObjectInfo>();
        objectInfo->m_TypeInfo = objectType;

        Ref<SerializableTypeInfoPrimitive> fieldType = CreateRef<SerializableTypeInfoPrimitive>();
        fieldType->m_Type = ScriptPrimitiveType::I32;
        Ref<SerializableFieldInfo> field = CreateRef<SerializableFieldInfo>();
        field->m_Name = "Value";
        field->m_FieldId = 1;
        field->m_ParentTypeId = objectType->m_TypeId;
        field->m_TypeInfo = fieldType;
        field->m_Flags = ScriptFieldFlagBits::Serializable;
        objectInfo->m_Fields[field->m_FieldId] = field;
        objectInfo->m_FieldNameToId[field->m_Name] = field->m_FieldId;

        Ref<SerializableObject> object = CreateRef<SerializableObject>(objectInfo);
        Ref<SerializableFieldI32> data = CreateRef<SerializableFieldI32>();
        data->Value = value;
        object->SetFieldData(field, data);
        return { object, field };
    }

    int32_t ReadScriptValue(const MonoScript& script, const Ref<SerializableMemberInfo>& field)
    {
        const PersistedScriptState state = script.CapturePersistedState();
        REQUIRE(state.Fields != nullptr);
        Ref<SerializableFieldData> data = state.Fields->GetFieldData(field);
        REQUIRE(data != nullptr);
        return StaticRefCast<SerializableFieldI32>(data)->Value;
    }

    class IntegerAction final : public UndoAction
    {
    public:
        IntegerAction(int& value, int oldValue, int newValue) : UndoAction("Change value"), m_Value(value), m_OldValue(oldValue), m_NewValue(newValue)
        {
        }

        void Commit() override { m_Value = m_NewValue; }
        void Revert() override { m_Value = m_OldValue; }

    private:
        int& m_Value;
        int m_OldValue;
        int m_NewValue;
    };

    class IntegerSnapshotFactory final : public RetainedUndoActionFactory
    {
    public:
        explicit IntegerSnapshotFactory(int& value) : m_Value(value) {}

        void Capture() { m_OldValue = m_Value; }
        Ref<UndoAction> Build() const override
        {
            m_BuildCount++;
            return CreateRef<IntegerAction>(m_Value, m_OldValue, m_Value);
        }
        void Reset() override { m_OldValue = m_Value; }

        uint32_t GetBuildCount() const { return m_BuildCount; }

    private:
        int& m_Value;
        int m_OldValue = 0;
        mutable uint32_t m_BuildCount = 0u;
    };

    class SnapshotResource final : public RefCounted
    {
    };

    struct SnapshotResourceComponent : ComponentBase
    {
        Ref<SnapshotResource> Resource;
    };

    struct UndoValueComponent : ComponentBase
    {
        int Primary = 0;
        int Secondary = 0;
    };
} // namespace

TEST_CASE("Undo history invalidates redo after a new edit", "[Editor][Undo]")
{
    UndoRedo::StartUp();
    int value = 2;
    UndoRedo::Get().RegisterAction(CreateRef<IntegerAction>(value, 1, 2));

    CHECK(UndoRedo::Get().CanUndo());
    CHECK(UndoRedo::Get().GetUndoName() == "Change value");
    UndoRedo::Get().Undo();
    CHECK(value == 1);
    CHECK(UndoRedo::Get().CanRedo());

    value = 3;
    UndoRedo::Get().RegisterAction(CreateRef<IntegerAction>(value, 1, 3));
    CHECK_FALSE(UndoRedo::Get().CanRedo());
    UndoRedo::Get().Undo();
    CHECK(value == 1);
    UndoRedo::Get().Redo();
    CHECK(value == 3);
    UndoRedo::Shutdown();
}

TEST_CASE("Deleted entity undo restores its subtree and sibling position", "[Editor][Undo][Hierarchy]")
{
    Ref<Scene> scene = CreateRef<Scene>();
    Entity root = scene->GetRootEntity();
    Entity before = scene->CreateEntity("Before");
    Entity parent = scene->CreateEntity("Parent");
    Entity after = scene->CreateEntity("After");
    root.AddChild(before);
    root.AddChild(parent);
    root.AddChild(after);
    Entity child = scene->CreateEntity("Child");
    parent.AddChild(child);
    child.AddComponent<CameraComponent>();
    parent.SetPosition({ 2.0f, 3.0f, 4.0f });
    child.SetPosition({ 5.0f, 6.0f, 7.0f });

    const UUID parentUuid = parent.GetUuid();
    const UUID childUuid = child.GetUuid();
    EntityDeletedAction action(parent, scene);
    scene->DestroyEntity(parent);
    CHECK_FALSE(scene->TryGetEntityFromUuid(parentUuid));
    CHECK_FALSE(scene->TryGetEntityFromUuid(childUuid));

    action.Revert();
    Entity restoredParent = scene->TryGetEntityFromUuid(parentUuid);
    Entity restoredChild = scene->TryGetEntityFromUuid(childUuid);
    REQUIRE(restoredParent);
    REQUIRE(restoredChild);
    CHECK(restoredParent.GetParent() == root);
    CHECK(restoredParent.GetSiblingIndex() == 1u);
    CHECK(restoredChild.GetParent() == restoredParent);
    CHECK(restoredChild.HasComponent<CameraComponent>());
    CHECK(restoredParent.GetLocalPosition() == glm::vec3(2.0f, 3.0f, 4.0f));
    CHECK(restoredChild.GetLocalPosition() == glm::vec3(5.0f, 6.0f, 7.0f));

    action.Commit();
    CHECK_FALSE(scene->TryGetEntityFromUuid(parentUuid));
    CHECK_FALSE(scene->TryGetEntityFromUuid(childUuid));
}

TEST_CASE("Component edits resolve entities by UUID after entity restoration", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Target");
    scene->GetRootEntity().AddChild(entity);
    TransformComponent oldTransform = entity.GetComponent<TransformComponent>();
    entity.SetPosition({ 8.0f, 0.0f, 0.0f });
    TransformComponent newTransform = entity.GetComponent<TransformComponent>();
    ChangeComponentAction<TransformComponent> transformAction(entity, oldTransform, newTransform);
    EntityDeletedAction deleteAction(entity, scene);

    scene->DestroyEntity(entity);
    deleteAction.Revert();
    transformAction.Revert();
    Entity restored = scene->TryGetEntityFromUuid(deleteAction.GetFocusEntity().GetUuid());
    REQUIRE(restored);
    CHECK(restored.GetLocalPosition() == glm::vec3(0.0f));
    transformAction.Commit();
    CHECK(restored.GetLocalPosition() == glm::vec3(8.0f, 0.0f, 0.0f));
}

TEST_CASE("Component add and remove actions preserve component data", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>();
    Entity entity = scene->CreateEntity("Camera");
    CameraComponent& camera = entity.AddComponent<CameraComponent>();
    camera.Camera.SetOrthographicSize(23.0f);
    AddComponentAction<CameraComponent> addAction(entity);

    addAction.Revert();
    CHECK_FALSE(entity.HasComponent<CameraComponent>());
    addAction.Commit();
    REQUIRE(entity.HasComponent<CameraComponent>());
    CHECK(entity.GetComponent<CameraComponent>().Camera.GetOrthographicSize() == 23.0f);

    CameraComponent snapshot = entity.GetComponent<CameraComponent>();
    RemoveComponentAction<CameraComponent> removeAction(entity, snapshot);
    entity.RemoveComponent<CameraComponent>();
    removeAction.Revert();
    REQUIRE(entity.HasComponent<CameraComponent>());
    CHECK(entity.GetComponent<CameraComponent>().Camera.GetOrthographicSize() == 23.0f);
    removeAction.Commit();
    CHECK_FALSE(entity.HasComponent<CameraComponent>());
}

TEST_CASE("Retained component snapshots preserve multi-edit undo state", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    Vector<Entity> entities{ first, second };

    Ref<ComponentUndoSnapshot<TagComponent>> snapshots = CreateRef<ComponentUndoSnapshot<TagComponent>>();
    snapshots->Capture(entities);
    CHECK(snapshots->Size() == 2u);

    first.GetComponent<TagComponent>().Tag = "Changed first";
    second.GetComponent<TagComponent>().Tag = "Changed second";
    Ref<UndoAction> action = snapshots->Build();
    REQUIRE(action != nullptr);

    action->Revert();
    CHECK(first.GetName() == "First");
    CHECK(second.GetName() == "Second");
    action->Commit();
    CHECK(first.GetName() == "Changed first");
    CHECK(second.GetName() == "Changed second");
}

TEST_CASE("Retained undo scopes freeze the first drag snapshot", "[Editor][Undo][Component]")
{
    UndoRedo::StartUp();
    int value = 1;
    int unrelatedValue = 20;
    Ref<IntegerSnapshotFactory> snapshots = CreateRef<IntegerSnapshotFactory>(value);
    Ref<IntegerSnapshotFactory> unrelatedSnapshots = CreateRef<IntegerSnapshotFactory>(unrelatedValue);

    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture();
    value = 2;
    UndoRedo::Get().OnItemInteract({ 7u, true, true, false, true });
    UndoRedo::Get().EndComponentScope();

    CHECK_FALSE(UndoRedo::Get().BeginComponentScope(unrelatedSnapshots));
    value = 3;
    UndoRedo::Get().OnItemInteract({ 8u, true, true, false, true });
    UndoRedo::Get().OnItemInteract({ 7u, false, false, true, false });
    UndoRedo::Get().EndComponentScope();

    CHECK(snapshots->GetBuildCount() == 1u);
    CHECK(unrelatedSnapshots->GetBuildCount() == 0u);
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(value == 1);
    UndoRedo::Get().Redo();
    CHECK(value == 3);
    UndoRedo::Shutdown();
}

TEST_CASE("Immediate component edits create one undo action", "[Editor][Undo][Component]")
{
    UndoRedo::StartUp();
    int value = 4;
    Ref<IntegerSnapshotFactory> snapshots = CreateRef<IntegerSnapshotFactory>(value);
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture();
    value = 9;
    UndoRedo::Get().OnItemInteract({ 11u, false, false, false, true });
    UndoRedo::Get().EndComponentScope();

    CHECK(snapshots->GetBuildCount() == 1u);
    UndoRedo::Get().Undo();
    CHECK(value == 4);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(value == 9);
    UndoRedo::Shutdown();
}

TEST_CASE("Component snapshots finalize redo after the setter", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Target");
    entity.AddComponent<UndoValueComponent>().Primary = 4;
    const Vector<Entity> entities{ entity };
    Ref<ComponentUndoSnapshot<UndoValueComponent>> snapshots = CreateRef<ComponentUndoSnapshot<UndoValueComponent>>();

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture(entities);
    UndoRedo::Get().OnItemInteract({ 11u, false, false, false, true });
    entity.GetComponent<UndoValueComponent>().Primary = 9;
    snapshots->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 4);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 9);
    UndoRedo::Shutdown();
}

TEST_CASE("Component snapshots coalesce instantaneous changes from one frame", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Target");
    UndoValueComponent& value = entity.AddComponent<UndoValueComponent>();
    value.Primary = 1;
    value.Secondary = 2;
    const Vector<Entity> entities{ entity };
    Ref<ComponentUndoSnapshot<UndoValueComponent>> snapshots = CreateRef<ComponentUndoSnapshot<UndoValueComponent>>();

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture(entities);
    UndoRedo::Get().OnItemInteract({ 11u, false, false, false, true });
    value.Primary = 3;
    UndoRedo::Get().OnItemInteract({ 12u, false, false, false, true });
    value.Secondary = 4;
    snapshots->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 1);
    CHECK(entity.GetComponent<UndoValueComponent>().Secondary == 2);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 3);
    CHECK(entity.GetComponent<UndoValueComponent>().Secondary == 4);
    UndoRedo::Shutdown();
}

TEST_CASE("Component snapshots preserve distinct multi-edit undo values", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity first = scene->CreateEntity("First");
    Entity second = scene->CreateEntity("Second");
    first.AddComponent<UndoValueComponent>().Primary = 1;
    second.AddComponent<UndoValueComponent>().Primary = 2;
    const Vector<Entity> entities{ first, second };
    Ref<ComponentUndoSnapshot<UndoValueComponent>> snapshots = CreateRef<ComponentUndoSnapshot<UndoValueComponent>>();

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture(entities);
    UndoRedo::Get().OnItemInteract({ 11u, false, false, false, true });
    first.GetComponent<UndoValueComponent>().Primary = 8;
    second.GetComponent<UndoValueComponent>().Primary = 8;
    snapshots->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(first.GetComponent<UndoValueComponent>().Primary == 1);
    CHECK(second.GetComponent<UndoValueComponent>().Primary == 2);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(first.GetComponent<UndoValueComponent>().Primary == 8);
    CHECK(second.GetComponent<UndoValueComponent>().Primary == 8);
    UndoRedo::Shutdown();
}

TEST_CASE("Component snapshots capture the final drag value after the setter", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Target");
    entity.AddComponent<UndoValueComponent>().Primary = 1;
    const Vector<Entity> entities{ entity };
    Ref<ComponentUndoSnapshot<UndoValueComponent>> snapshots = CreateRef<ComponentUndoSnapshot<UndoValueComponent>>();

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture(entities);
    UndoRedo::Get().OnItemInteract({ 11u, true, true, false, true });
    entity.GetComponent<UndoValueComponent>().Primary = 2;
    snapshots->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    CHECK_FALSE(UndoRedo::Get().BeginComponentScope(snapshots));
    UndoRedo::Get().OnItemInteract({ 11u, false, false, true, true });
    entity.GetComponent<UndoValueComponent>().Primary = 3;
    snapshots->CompleteFrame();
    UndoRedo::Get().EndComponentScope();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 1);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 3);
    UndoRedo::Shutdown();
}

TEST_CASE("Component snapshot finish and reset preserve redo state", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Target");
    entity.AddComponent<UndoValueComponent>().Primary = 1;
    const Vector<Entity> entities{ entity };
    Ref<ComponentUndoSnapshot<UndoValueComponent>> snapshots = CreateRef<ComponentUndoSnapshot<UndoValueComponent>>();

    UndoRedo::StartUp();
    REQUIRE(UndoRedo::Get().BeginComponentScope(snapshots));
    snapshots->Capture(entities);
    UndoRedo::Get().OnItemInteract({ 11u, true, true, false, true });
    entity.GetComponent<UndoValueComponent>().Primary = 5;
    UndoRedo::Get().EndComponentScope();
    UndoRedo::Get().FinishComponentScope(snapshots);
    snapshots->Reset();

    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 1);
    CHECK_FALSE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Redo();
    CHECK(entity.GetComponent<UndoValueComponent>().Primary == 5);
    UndoRedo::Shutdown();
}

TEST_CASE("Component scope ownership preserves unrelated transactions", "[Editor][Undo][Component]")
{
    UndoRedo::StartUp();
    int value = 1;
    uint32_t buildCount = 0u;
    UndoRedo::Get().BeginComponentScope([&]() -> Ref<UndoAction> {
        buildCount++;
        return CreateRef<IntegerAction>(value, 1, value);
    });
    value = 2;
    UndoRedo::Get().OnItemInteract({ 17u, true, true, false, true });
    UndoRedo::Get().EndComponentScope();

    int unrelatedValue = 0;
    Ref<IntegerSnapshotFactory> unrelated = CreateRef<IntegerSnapshotFactory>(unrelatedValue);
    UndoRedo::Get().CancelComponentScope(unrelated);
    UndoRedo::Get().OnItemInteract({ 17u, false, false, true, false });

    CHECK(buildCount == 1u);
    REQUIRE(UndoRedo::Get().CanUndo());
    UndoRedo::Get().Undo();
    CHECK(value == 1);
    UndoRedo::Shutdown();
}

TEST_CASE("Reset component snapshots release retained resources", "[Editor][Undo][Component]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Resource holder");
    Ref<SnapshotResource> resource = CreateRef<SnapshotResource>();
    entity.AddComponent<SnapshotResourceComponent>().Resource = resource;
    CHECK(resource->GetRefCount() == 2u);

    Ref<ComponentUndoSnapshot<SnapshotResourceComponent>> snapshots = CreateRef<ComponentUndoSnapshot<SnapshotResourceComponent>>();
    snapshots->Capture(Vector<Entity>{ entity });
    CHECK(resource->GetRefCount() == 3u);
    entity.RemoveComponent<SnapshotResourceComponent>();
    CHECK(resource->GetRefCount() == 2u);
    snapshots->Reset();
    CHECK(resource->GetRefCount() == 1u);
}

TEST_CASE("Stable tag and mesh snapshot scopes allocate nothing after warm-up", "[Editor][Undo][Component][Memory]")
{
    constexpr size_t entityCount = 64u;
    constexpr size_t frameCount = 120u;
    Ref<Scene> scene = CreateRef<Scene>(false);
    Vector<Entity> entities;
    entities.reserve(entityCount);
    for (size_t entityIndex = 0u; entityIndex < entityCount; entityIndex++)
    {
        Entity entity = scene->CreateEntity("Retained component snapshot name longer than the short-string buffer");
        entity.AddComponent<MeshRendererComponent>().Materials.resize(8u);
        entities.push_back(entity);
    }

    Ref<ComponentUndoSnapshot<TagComponent>> tagSnapshots = CreateRef<ComponentUndoSnapshot<TagComponent>>();
    Ref<ComponentUndoSnapshot<MeshRendererComponent>> meshSnapshots = CreateRef<ComponentUndoSnapshot<MeshRendererComponent>>();
    tagSnapshots->Capture(entities);
    meshSnapshots->Capture(entities);
    const size_t retainedTagCapacity = tagSnapshots->Capacity();
    const size_t retainedMeshCapacity = meshSnapshots->Capacity();

    UndoRedo::StartUp();
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    bool allScopesStarted = true;
    for (size_t frameIndex = 0u; frameIndex < frameCount; frameIndex++)
    {
        allScopesStarted &= UndoRedo::Get().BeginComponentScope(tagSnapshots);
        tagSnapshots->Capture(entities);
        tagSnapshots->CompleteFrame();
        UndoRedo::Get().EndComponentScope();
        allScopesStarted &= UndoRedo::Get().BeginComponentScope(meshSnapshots);
        meshSnapshots->Capture(entities);
        meshSnapshots->CompleteFrame();
        UndoRedo::Get().EndComponentScope();
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);
    UndoRedo::Shutdown();

    CHECK(allScopesStarted);
    CHECK(tagSnapshots->Size() == entityCount);
    CHECK(meshSnapshots->Size() == entityCount);
    CHECK(tagSnapshots->Capacity() == retainedTagCapacity);
    CHECK(meshSnapshots->Capacity() == retainedMeshCapacity);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}

TEST_CASE("Script undo and redo use MonoScript persisted-state policy", "[Editor][Undo][Scripting][PersistedState]")
{
    Ref<Scene> scene = CreateRef<Scene>(false);
    Entity entity = scene->CreateEntity("Script host");
    const ScriptTypeIdentity identity{ "Missing.Assembly", "Missing.Namespace", "Behaviour" };
    TestScriptFields fields = MakeScriptFields(identity, 10);
    REQUIRE(scene->AddScriptComponent(entity, PersistedScriptState{ identity, fields.Object }, false));

    ChangeScriptComponentAction::State oldState = ChangeScriptComponentAction::Capture(entity);
    MonoScript& script = entity.GetComponent<MonoScriptComponent>().Scripts.front();
    Ref<SerializableFieldI32> liveValue = StaticRefCast<SerializableFieldI32>(script.CapturePersistedState().Fields->GetFieldData(fields.Field));
    REQUIRE(liveValue != nullptr);
    liveValue->Value = 20;
    ChangeScriptComponentAction action(entity, std::move(oldState));

    action.Revert();
    CHECK(ReadScriptValue(entity.GetComponent<MonoScriptComponent>().Scripts.front(), fields.Field) == 10);
    action.Commit();
    CHECK(ReadScriptValue(entity.GetComponent<MonoScriptComponent>().Scripts.front(), fields.Field) == 20);
}
