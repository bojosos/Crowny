#include <catch2/catch_test_macros.hpp>

#include "Crowny/Scripting/Serialization/SerializableField.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
#include "Crowny/Scripting/Serialization/SerializableObjectInfo.h"
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
