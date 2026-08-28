#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Scene/SceneManager.h"

#include <cstring>
#include <limits>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        UUID FromAbiUuid(const cw_managed_uuid& uuid)
        {
            const auto word = [&](uint32_t offset) {
                return static_cast<uint32_t>(uuid.bytes[offset]) << 24u | static_cast<uint32_t>(uuid.bytes[offset + 1]) << 16u |
                       static_cast<uint32_t>(uuid.bytes[offset + 2]) << 8u | static_cast<uint32_t>(uuid.bytes[offset + 3]);
            };
            return UUID(word(0), word(4), word(8), word(12));
        }

        cw_managed_uuid ToAbiUuid(const UUID& uuid)
        {
            cw_managed_uuid result{};
            const String text = uuid.ToString();
            uint32_t output = 0;
            uint8_t high = 0;
            bool haveHigh = false;
            for (const char character : text)
            {
                if (character == '-')
                    continue;
                const uint8_t value = character >= '0' && character <= '9' ? static_cast<uint8_t>(character - '0')
                                      : character >= 'a' && character <= 'f' ? static_cast<uint8_t>(character - 'a' + 10)
                                                                            : static_cast<uint8_t>(character - 'A' + 10);
                if (!haveHigh)
                {
                    high = value;
                    haveHigh = true;
                }
                else if (output < 16)
                {
                    result.bytes[output++] = static_cast<uint8_t>((high << 4u) | value);
                    haveHigh = false;
                }
            }
            return result;
        }

        Entity ResolveEntity(const cw_managed_uuid& entityId)
        {
            SceneManager* manager = SceneManager::TryGet();
            const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
            return scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
        }

        template <typename Callback> cw_managed_status Execute(void* context, Callback&& callback)
        {
            if (context == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            try
            {
                return callback();
            }
            catch (...)
            {
                return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
            }
        }

        String Decode(cw_managed_string_view value)
        {
            return value.data == nullptr ? String() : String(reinterpret_cast<const char*>(value.data), value.length);
        }

        cw_managed_status ResolveFontHandle(const cw_managed_uuid& fontId, AssetHandle<Font>& font)
        {
            AssetManager* assetManager = AssetManager::TryGet();
            if (assetManager == nullptr)
                return CW_MANAGED_STATUS_NOT_INITIALIZED;
            const UUID uuid = FromAbiUuid(fontId);
            if (uuid.Empty())
                return CW_MANAGED_STATUS_STALE_HANDLE;
            font = assetManager->LoadFromUUID<Font>(uuid);
            return font ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_STALE_HANDLE;
        }

        UUID GetSourceFontUuid(const AssetHandle<Font>& font, const Font* source)
        {
            if (source == nullptr)
                return {};
            if (font.Get() == source)
                return font.GetUUID();
            if (!font)
                return {};
            for (const AssetHandle<Font>& fallback : font->GetFallbackFonts())
            {
                if (fallback.Get() == source)
                    return fallback.GetUUID();
            }
            return {};
        }

        cw_managed_font_character_info ToAbiCharacterInfo(const AssetHandle<Font>& font, const CharacterInfo& source)
        {
            cw_managed_font_character_info result{};
            result.source_font = ToAbiUuid(GetSourceFontUuid(font, source.SourceFont));
            result.requested_code_point = static_cast<uint32_t>(source.RequestedCodePoint);
            result.resolved_code_point = static_cast<uint32_t>(source.ResolvedCodePoint);
            result.glyph_index = source.GlyphIndex;
            result.advance = source.Advance;
            result.plane_left = source.PlaneLeft;
            result.plane_bottom = source.PlaneBottom;
            result.plane_right = source.PlaneRight;
            result.plane_top = source.PlaneTop;
            result.atlas_left = source.AtlasLeft;
            result.atlas_bottom = source.AtlasBottom;
            result.atlas_right = source.AtlasRight;
            result.atlas_top = source.AtlasTop;
            result.whitespace = source.Whitespace ? 1 : 0;
            result.valid = source.Valid ? 1 : 0;
            return result;
        }

        bool HasComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_HAS_COMPONENT(managedName, nativeType)                                                                                    \
    if (name == managedName)                                                                                                         \
        return entity.HasComponent<nativeType>()
            CW_HAS_COMPONENT("Crowny.AudioListener", AudioListenerComponent);
            CW_HAS_COMPONENT("Crowny.AudioSource", AudioSourceComponent);
            CW_HAS_COMPONENT("Crowny.Camera", CameraComponent);
            CW_HAS_COMPONENT("Crowny.LightComponent", LightComponent);
            CW_HAS_COMPONENT("Crowny.Text", TextComponent);
            CW_HAS_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent);
            CW_HAS_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent);
            CW_HAS_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent);
            CW_HAS_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent);
            CW_HAS_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent);
            CW_HAS_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent);
            CW_HAS_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent);
            CW_HAS_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent);
            CW_HAS_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent);
#undef CW_HAS_COMPONENT
            return false;
        }

        bool AddComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_ADD_COMPONENT(managedName, nativeType)                                                                                    \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.AddOrGetComponent<nativeType>();                                                                                      \
        return true;                                                                                                                 \
    }
            CW_ADD_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_ADD_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_ADD_COMPONENT("Crowny.Camera", CameraComponent)
            CW_ADD_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_ADD_COMPONENT("Crowny.Text", TextComponent)
            CW_ADD_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_ADD_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_ADD_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_ADD_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_ADD_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_ADD_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_ADD_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_ADD_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_ADD_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_ADD_COMPONENT
            return false;
        }

        bool RemoveComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return false;
#define CW_REMOVE_COMPONENT(managedName, nativeType)                                                                                 \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.RemoveComponentIfExists<nativeType>();                                                                                \
        return true;                                                                                                                 \
    }
            CW_REMOVE_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_REMOVE_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_REMOVE_COMPONENT("Crowny.Camera", CameraComponent)
            CW_REMOVE_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_REMOVE_COMPONENT("Crowny.Text", TextComponent)
            CW_REMOVE_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_REMOVE_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_REMOVE_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_REMOVE_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_REMOVE_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_REMOVE_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_REMOVE_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_REMOVE_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_REMOVE_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_REMOVE_COMPONENT
            return false;
        }

        cw_managed_status CW_MANAGED_CALL EntityHasComponent(void* context, cw_managed_uuid entityId,
                                                              cw_managed_string_view typeName, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (typeName.data == nullptr && typeName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = HasComponent(entity, Decode(typeName)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL EntityAddComponent(void* context, cw_managed_uuid entityId,
                                                              cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if (typeName.data == nullptr && typeName.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return AddComponent(entity, Decode(typeName)) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

        cw_managed_status CW_MANAGED_CALL EntityRemoveComponent(void* context, cw_managed_uuid entityId,
                                                                 cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if (typeName.data == nullptr && typeName.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return RemoveComponent(entity, Decode(typeName)) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

        cw_managed_status CW_MANAGED_CALL GetEntityName(void* context, cw_managed_uuid entityId, cw_managed_string_view* name)
        {
            return Execute(context, [&]() {
                if (name == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                thread_local String storage;
                storage = entity.GetName();
                if (storage.size() > std::numeric_limits<uint32_t>::max())
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                name->data = reinterpret_cast<const uint8_t*>(storage.data());
                name->length = static_cast<uint32_t>(storage.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SetEntityName(void* context, cw_managed_uuid entityId, cw_managed_string_view name)
        {
            return Execute(context, [&]() {
                if (name.data == nullptr && name.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                entity.GetComponent<TagComponent>().Tag = Decode(name);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FindEntityByName(void* context, cw_managed_string_view name, cw_managed_uuid* entityId)
        {
            return Execute(context, [&]() {
                if (entityId == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
                if (scene == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const Entity entity = scene->FindEntityByName(Decode(name));
                *entityId = entity ? ToAbiUuid(entity.GetUuid()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL GetEntityParent(void* context, cw_managed_uuid entityId, cw_managed_uuid* parentId)
        {
            return Execute(context, [&]() {
                if (parentId == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Entity parent = entity.GetParent();
                *parentId = parent ? ToAbiUuid(parent.GetUuid()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SetEntityParent(void* context, cw_managed_uuid entityId, cw_managed_uuid parentId)
        {
            return Execute(context, [&]() {
                Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const UUID parentUuid = FromAbiUuid(parentId);
                if (parentUuid.Empty())
                    return CW_MANAGED_STATUS_OK;
                const Entity parent = ResolveEntity(parentId);
                if (!parent)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return entity.SetParent(parent) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

        cw_managed_status CW_MANAGED_CALL DestroyEntity(void* context, cw_managed_uuid entityId)
        {
            return Execute(context, [&]() {
                SceneManager* manager = SceneManager::TryGet();
                const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
                const Entity entity = scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                scene->DestroyEntity(entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_TRANSFORM_GET_VEC3(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() -> cw_managed_status {                                                                        \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::vec3 value = expression;                                                                                      \
            *result = { value.x, value.y, value.z };                                                                                 \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TRANSFORM_SET_VEC3(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)            \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::vec3 vector(value->x, value->y, value->z);                                                                    \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_VEC3(TransformGetPosition, entity.GetWorldPosition())
        CW_TRANSFORM_SET_VEC3(TransformSetPosition, entity.SetWorldPosition(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalPosition, entity.GetLocalPosition())
        CW_TRANSFORM_SET_VEC3(TransformSetLocalPosition, entity.SetPosition(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetScale, entity.GetWorldScale())
        CW_TRANSFORM_SET_VEC3(TransformSetScale, entity.SetWorldScale(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalScale, entity.GetLocalScale())
        CW_TRANSFORM_SET_VEC3(TransformSetLocalScale, entity.SetScale(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetEulerAngles, glm::degrees(glm::eulerAngles(entity.GetWorldRotation())))
        CW_TRANSFORM_SET_VEC3(TransformSetEulerAngles, entity.SetWorldRotation(glm::quat(glm::radians(vector))))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalEulerAngles, glm::degrees(glm::eulerAngles(entity.GetLocalRotation())))
        CW_TRANSFORM_SET_VEC3(TransformSetLocalEulerAngles, entity.SetRotation(glm::quat(glm::radians(vector))))
#undef CW_TRANSFORM_SET_VEC3
#undef CW_TRANSFORM_GET_VEC3

#define CW_TRANSFORM_GET_QUAT(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_quat* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::quat value = expression;                                                                                      \
            *result = { value.x, value.y, value.z, value.w };                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TRANSFORM_SET_QUAT(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_quat* value)            \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::quat rotation(value->w, value->x, value->y, value->z);                                                        \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_QUAT(TransformGetRotation, entity.GetWorldRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetRotation, entity.SetWorldRotation(rotation))
        CW_TRANSFORM_GET_QUAT(TransformGetLocalRotation, entity.GetLocalRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetLocalRotation, entity.SetRotation(rotation))
#undef CW_TRANSFORM_SET_QUAT
#undef CW_TRANSFORM_GET_QUAT

#define CW_TRANSFORM_GET_MATRIX(functionName, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_mat4* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::mat4 value = expression;                                                                                      \
            std::memcpy(result->values, glm::value_ptr(value), sizeof(value));                                                       \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_MATRIX(TransformGetLocalToWorldMatrix, entity.GetWorldMatrix())
        CW_TRANSFORM_GET_MATRIX(TransformGetWorldToLocalMatrix, glm::inverse(entity.GetWorldMatrix()))
#undef CW_TRANSFORM_GET_MATRIX

        cw_managed_status CW_MANAGED_CALL TransformIsDirty(void* context, cw_managed_uuid entityId, int32_t flag, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr || flag < 0 || flag > 1)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const TransformComponent& transform = entity.GetComponent<TransformComponent>();
                *result = flag == 0 ? !transform.IsCachedLocalTransformValid() : !transform.IsCachedWorldTransformValid();
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_INPUT_BUTTON(functionName, expression)                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t code, uint8_t* result)                                   \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression ? 1 : 0;                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_INPUT_BUTTON(InputGetKey, Input::IsKeyPressed(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyDown, Input::IsKeyDown(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyUp, Input::IsKeyUp(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButton, Input::IsMouseButtonPressed(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonDown, Input::IsMouseButtonDown(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonUp, Input::IsMouseButtonUp(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputIsGamepadConnected, Input::IsGamepadConnected(code))
#undef CW_INPUT_BUTTON

#define CW_GAMEPAD_BUTTON(functionName, expression)                                                                                 \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t gamepad, uint32_t code, uint8_t* result)                \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression ? 1 : 0;                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_GAMEPAD_BUTTON(InputGetGamepadButton,
                          Input::IsGamepadButtonPressed(gamepad, static_cast<GamepadButtonCode>(code)))
        CW_GAMEPAD_BUTTON(InputGetGamepadButtonDown,
                          Input::IsGamepadButtonDown(gamepad, static_cast<GamepadButtonCode>(code)))
        CW_GAMEPAD_BUTTON(InputGetGamepadButtonUp,
                          Input::IsGamepadButtonUp(gamepad, static_cast<GamepadButtonCode>(code)))
#undef CW_GAMEPAD_BUTTON

        cw_managed_status CW_MANAGED_CALL InputGetGamepadAxis(void* context, uint32_t gamepad, uint32_t code,
                                                               float* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::GetGamepadAxis(gamepad, static_cast<GamepadAxisCode>(code));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_GLOBAL_FLOAT(functionName, expression)                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, float* result)                                                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_GLOBAL_FLOAT(InputGetMouseScrollX, Input::GetMouseScrollX())
        CW_GLOBAL_FLOAT(InputGetMouseScrollY, Input::GetMouseScrollY())
        CW_GLOBAL_FLOAT(TimeGetDeltaTime, Time::GetDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetTime, Time::GetTime())
        CW_GLOBAL_FLOAT(TimeGetFixedDeltaTime, Time::GetFixedDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetSmoothDeltaTime, Time::GetSmoothDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetRealtimeSinceStartup, Time::GetRealtimeSinceStartup())
#undef CW_GLOBAL_FLOAT

        cw_managed_status CW_MANAGED_CALL TimeGetFrameCount(void* context, uint32_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = static_cast<uint32_t>(Time::GetFrameCount());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputGetMousePosition(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetMousePosition();
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputGetMouseDelta(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetMouseDelta();
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_ACTION_BUTTON(functionName, expression)                                                                                  \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_string_view actionName, uint8_t* result)              \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr || (actionName.data == nullptr && actionName.length != 0))                                        \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression ? 1 : 0;                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_ACTION_BUTTON(InputGetAction, Input::GetAction(Decode(actionName)))
        CW_ACTION_BUTTON(InputGetActionDown, Input::GetActionDown(Decode(actionName)))
        CW_ACTION_BUTTON(InputGetActionUp, Input::GetActionUp(Decode(actionName)))
#undef CW_ACTION_BUTTON

        cw_managed_status CW_MANAGED_CALL InputGetAxis(void* context, cw_managed_string_view actionName, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (actionName.data == nullptr && actionName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::GetAxis(Decode(actionName));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputGetActionVector(void* context, cw_managed_string_view actionName,
                                                                cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (actionName.data == nullptr && actionName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetActionVector(Decode(actionName));
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_ACTION_MAP(functionName, enabled)                                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_string_view mapName, uint8_t* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr || (mapName.data == nullptr && mapName.length != 0))                                              \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = Input::SetActionMapEnabled(Decode(mapName), enabled) ? 1 : 0;                                                  \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_ACTION_MAP(InputEnableActionMap, true)
        CW_ACTION_MAP(InputDisableActionMap, false)
#undef CW_ACTION_MAP

        cw_managed_status CW_MANAGED_CALL InputClearActionRebinds(void* context)
        {
            return Execute(context, [&]() {
                Input::ClearActionRebinds();
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_RIGIDBODY_GET(functionName, resultType, expression)                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                          \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_RIGIDBODY_SET(functionName, valueType, statement)                                                                         \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                        \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_RIGIDBODY_GET(Rigidbody2DGetMass, float, physics != nullptr ? physics->GetMass(entity) : rigidbody.GetMass())
        CW_RIGIDBODY_SET(Rigidbody2DSetMass, float, if (rigidbody.GetAutoMass()) return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                         rigidbody.SetMass(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetBodyType, int32_t, static_cast<int32_t>(rigidbody.GetBodyType()))
        CW_RIGIDBODY_SET(Rigidbody2DSetBodyType, int32_t, rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetSleepMode, int32_t, static_cast<int32_t>(rigidbody.GetSleepMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetSleepMode, int32_t, rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetCollisionDetectionMode, int32_t,
                         static_cast<int32_t>(rigidbody.GetCollisionDetectionMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetCollisionDetectionMode, int32_t,
                         rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetInterpolation, int32_t, static_cast<int32_t>(rigidbody.GetInterpolationMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetInterpolation, int32_t,
                         rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetAutoMass, uint8_t, rigidbody.GetAutoMass() ? 1 : 0)
        CW_RIGIDBODY_SET(Rigidbody2DSetAutoMass, uint8_t, rigidbody.SetAutoMass(value != 0, entity))
        CW_RIGIDBODY_GET(Rigidbody2DGetLayer, int32_t, static_cast<int32_t>(rigidbody.GetLayerMask()))
        CW_RIGIDBODY_SET(Rigidbody2DSetLayer, int32_t,
                         if (value < 0 || value >= static_cast<int32_t>(Physics2DLayerCount)) return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                         rigidbody.SetLayerMask(static_cast<uint32_t>(value), entity))
        CW_RIGIDBODY_GET(Rigidbody2DGetLinearDrag, float, rigidbody.GetLinearDrag())
        CW_RIGIDBODY_SET(Rigidbody2DSetLinearDrag, float, rigidbody.SetLinearDrag(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetAngularDrag, float, rigidbody.GetAngularDrag())
        CW_RIGIDBODY_SET(Rigidbody2DSetAngularDrag, float, rigidbody.SetAngularDrag(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetGravityScale, float, rigidbody.GetGravityScale())
        CW_RIGIDBODY_SET(Rigidbody2DSetGravityScale, float, rigidbody.SetGravityScale(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetInertia, float, physics != nullptr ? physics->GetInertia(entity) : rigidbody.GetInertia())
        CW_RIGIDBODY_SET(Rigidbody2DSetInertia, float, rigidbody.SetInertia(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetConstraints, uint32_t, static_cast<uint32_t>(rigidbody.GetConstraints()))
        CW_RIGIDBODY_SET(Rigidbody2DSetConstraints, uint32_t, rigidbody.SetConstraints(Rigidbody2DConstraints(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetRotation, float, physics != nullptr ? physics->GetRotation(entity) : 0.0f)
        CW_RIGIDBODY_GET(Rigidbody2DGetAngularVelocity, float, physics != nullptr ? physics->GetAngularVelocity(entity) : 0.0f)
        CW_RIGIDBODY_SET(Rigidbody2DSetAngularVelocity, float,
                         if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetAngularVelocity(entity, value))
        CW_RIGIDBODY_GET(Rigidbody2DGetAwake, uint8_t, physics != nullptr && physics->IsBodyAwake(entity) ? 1 : 0)
        CW_RIGIDBODY_SET(Rigidbody2DSetAwake, uint8_t,
                         if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetBodyAwake(entity, value != 0))
#undef CW_RIGIDBODY_SET
#undef CW_RIGIDBODY_GET

#define CW_RIGIDBODY_GET_VEC2(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)                \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                          \
            const glm::vec2 value = expression;                                                                                      \
            *result = { value.x, value.y };                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_RIGIDBODY_SET_VEC2(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)           \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            const glm::vec2 vector(value->x, value->y);                                                                              \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetCenterOfMass,
                              physics != nullptr ? physics->GetCenterOfMass(entity) : rigidbody.GetCenterOfMass())
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetCenterOfMass, rigidbody.SetCenterOfMass(vector))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetPosition,
                              physics != nullptr ? physics->GetPosition(entity) : glm::vec2(entity.GetWorldPosition()))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetLinearVelocity,
                              physics != nullptr ? physics->GetLinearVelocity(entity) : glm::vec2(0.0f))
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetLinearVelocity,
                              if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetLinearVelocity(entity, vector))
#undef CW_RIGIDBODY_SET_VEC2
#undef CW_RIGIDBODY_GET_VEC2

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForce(void* context, cw_managed_uuid entityId,
                                                              const cw_managed_vec2* force, int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (force == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddForce(entity, glm::vec2(force->x, force->y), static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForceAtPosition(void* context, cw_managed_uuid entityId,
                                                                        const cw_managed_vec2* force,
                                                                        const cw_managed_vec2* position, int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (force == nullptr || position == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddForceAt(entity, glm::vec2(force->x, force->y), glm::vec2(position->x, position->y),
                                    static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddTorque(void* context, cw_managed_uuid entityId, float torque,
                                                               int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddTorque(entity, torque, static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_AUDIO_GET(functionName, resultType, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_AUDIO_SET(functionName, valueType, statement)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                        \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_AUDIO_GET(AudioSourceGetVolume, float, source.GetVolume())
        CW_AUDIO_SET(AudioSourceSetVolume, float, source.SetVolume(value))
        CW_AUDIO_GET(AudioSourceGetPitch, float, source.GetPitch())
        CW_AUDIO_SET(AudioSourceSetPitch, float, source.SetPitch(value))
        CW_AUDIO_GET(AudioSourceGetMinDistance, float, source.GetMinDistance())
        CW_AUDIO_SET(AudioSourceSetMinDistance, float, source.SetMinDistance(value))
        CW_AUDIO_GET(AudioSourceGetMaxDistance, float, source.GetMaxDistance())
        CW_AUDIO_SET(AudioSourceSetMaxDistance, float, source.SetMaxDistance(value))
        CW_AUDIO_GET(AudioSourceGetLoop, uint8_t, source.GetLooping() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetLoop, uint8_t, source.SetLooping(value != 0))
        CW_AUDIO_GET(AudioSourceGetMuted, uint8_t, source.GetIsMuted() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetMuted, uint8_t, source.SetIsMuted(value != 0))
        CW_AUDIO_GET(AudioSourceGetPlayOnAwake, uint8_t, source.GetPlayOnAwake() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetPlayOnAwake, uint8_t, source.SetPlayOnAwake(value != 0))
        CW_AUDIO_GET(AudioSourceGetTime, float, source.GetTime())
        CW_AUDIO_SET(AudioSourceSetTime, float, source.SetTime(value))
        CW_AUDIO_GET(AudioSourceGetClip, cw_managed_uuid, ToAbiUuid(source.GetClip().GetUUID()))
        CW_AUDIO_SET(AudioSourceSetClip, cw_managed_uuid,
                     if (const UUID uuid = FromAbiUuid(value); uuid.Empty()) source.SetClip({});
                     else if (AssetManager::IsStartedUp()) source.SetClip(AssetManager::TryGet()->LoadFromUUID<AudioClip>(uuid));
                     else return CW_MANAGED_STATUS_NOT_INITIALIZED)
        CW_AUDIO_GET(AudioSourceGetState, int32_t, static_cast<int32_t>(source.GetState()))
#undef CW_AUDIO_SET
#undef CW_AUDIO_GET

#define CW_AUDIO_ACTION(functionName, statement)                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId)                                         \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_AUDIO_ACTION(AudioSourcePlay, source.Play())
        CW_AUDIO_ACTION(AudioSourcePause, source.Pause())
        CW_AUDIO_ACTION(AudioSourceStop, source.Stop())
#undef CW_AUDIO_ACTION

        cw_managed_status CW_MANAGED_CALL TextGetText(void* context, cw_managed_uuid entityId,
                                                       cw_managed_string_view* result)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                if (!entity || !entity.HasComponent<TextComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                thread_local String storage;
                storage = entity.GetComponent<TextComponent>().Text;
                if (storage.size() > std::numeric_limits<uint32_t>::max())
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                result->data = reinterpret_cast<const uint8_t*>(storage.data());
                result->length = static_cast<uint32_t>(storage.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL TextSetText(void* context, cw_managed_uuid entityId, cw_managed_string_view value)
        {
            return Execute(context, [&]() {
                if (value.data == nullptr && value.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Entity entity = ResolveEntity(entityId);
                if (!entity || !entity.HasComponent<TextComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                entity.GetComponent<TextComponent>().Text = Decode(value);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_TEXT_GET(functionName, resultType, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const TextComponent& text = entity.GetComponent<TextComponent>();                                                       \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TEXT_SET(functionName, valueType, statement)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                        \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            TextComponent& text = entity.GetComponent<TextComponent>();                                                             \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }

        CW_TEXT_GET(TextGetFont, cw_managed_uuid, ToAbiUuid(text.Font.GetUUID()))
        CW_TEXT_SET(TextSetFont, cw_managed_uuid,
                    if (const UUID uuid = FromAbiUuid(value); uuid.Empty()) text.Font = {};
                    else if (AssetManager::IsStartedUp()) text.Font = AssetManager::TryGet()->LoadFromUUID<Font>(uuid);
                    else return CW_MANAGED_STATUS_NOT_INITIALIZED)
        CW_TEXT_GET(TextGetSize, float, text.Size)
        CW_TEXT_SET(TextSetSize, float, text.Size = value)
        CW_TEXT_GET(TextGetAutoSize, uint8_t, text.AutoSize ? 1 : 0)
        CW_TEXT_SET(TextSetAutoSize, uint8_t, text.AutoSize = value != 0)
        CW_TEXT_GET(TextGetAutoSizeMin, float, text.AutoSizeMin)
        CW_TEXT_SET(TextSetAutoSizeMin, float,
                    text.AutoSizeMin = std::max(0.0f, value); text.AutoSizeMax = std::max(text.AutoSizeMin, text.AutoSizeMax))
        CW_TEXT_GET(TextGetAutoSizeMax, float, text.AutoSizeMax)
        CW_TEXT_SET(TextSetAutoSizeMax, float,
                    text.AutoSizeMax = std::max(0.0f, value); text.AutoSizeMin = std::min(text.AutoSizeMin, text.AutoSizeMax))
        CW_TEXT_GET(TextGetWrapping, uint8_t, text.Wrapping ? 1 : 0)
        CW_TEXT_SET(TextSetWrapping, uint8_t, text.Wrapping = value != 0)
        CW_TEXT_GET(TextGetWrapMode, int32_t, static_cast<int32_t>(text.WrapMode))
        CW_TEXT_SET(TextSetWrapMode, int32_t, text.WrapMode = static_cast<TextWrapMode>(value))
        CW_TEXT_GET(TextGetOverflow, int32_t, static_cast<int32_t>(text.Overflow))
        CW_TEXT_SET(TextSetOverflow, int32_t, text.Overflow = static_cast<TextOverflow>(value))
        CW_TEXT_GET(TextGetClipToBounds, uint8_t, text.ClipToBounds ? 1 : 0)
        CW_TEXT_SET(TextSetClipToBounds, uint8_t, text.ClipToBounds = value != 0)
        CW_TEXT_GET(TextGetMaxLines, uint32_t, text.MaxLines)
        CW_TEXT_SET(TextSetMaxLines, uint32_t, text.MaxLines = value)
        CW_TEXT_GET(TextGetHorizontalAlignment, int32_t, static_cast<int32_t>(text.HorizontalAlignment))
        CW_TEXT_SET(TextSetHorizontalAlignment, int32_t,
                    text.HorizontalAlignment = static_cast<TextHorizontalAlignment>(value))
        CW_TEXT_GET(TextGetVerticalAlignment, int32_t, static_cast<int32_t>(text.VerticalAlignment))
        CW_TEXT_SET(TextSetVerticalAlignment, int32_t, text.VerticalAlignment = static_cast<TextVerticalAlignment>(value))
        CW_TEXT_GET(TextGetFontStyle, uint32_t, static_cast<uint32_t>(text.FontStyle))
        CW_TEXT_SET(TextSetFontStyle, uint32_t, text.FontStyle = static_cast<TextFontStyleBits>(value))
        CW_TEXT_GET(TextGetOutlineWidth, float, text.Thickness)
        CW_TEXT_SET(TextSetOutlineWidth, float, text.Thickness = value)
        CW_TEXT_GET(TextGetShadowSoftness, float, text.ShadowSoftness)
        CW_TEXT_SET(TextSetShadowSoftness, float, text.ShadowSoftness = std::max(0.0f, value))
        CW_TEXT_GET(TextGetCharacterSpacing, float, text.CharacterSpacing)
        CW_TEXT_SET(TextSetCharacterSpacing, float, text.CharacterSpacing = value)
        CW_TEXT_GET(TextGetWordSpacing, float, text.WordSpacing)
        CW_TEXT_SET(TextSetWordSpacing, float, text.WordSpacing = value)
        CW_TEXT_GET(TextGetLineSpacing, float, text.LineSpacing)
        CW_TEXT_SET(TextSetLineSpacing, float, text.LineSpacing = value)
        CW_TEXT_GET(TextGetParagraphSpacing, float, text.ParagraphSpacing)
        CW_TEXT_SET(TextSetParagraphSpacing, float, text.ParagraphSpacing = value)
        CW_TEXT_GET(TextGetTabWidth, uint32_t, text.TabWidth)
        CW_TEXT_SET(TextSetTabWidth, uint32_t, text.TabWidth = std::max(1u, value))
        CW_TEXT_GET(TextGetUseCustomDecorationColor, uint8_t, text.UseCustomDecorationColor ? 1 : 0)
        CW_TEXT_SET(TextSetUseCustomDecorationColor, uint8_t, text.UseCustomDecorationColor = value != 0)
        CW_TEXT_GET(TextGetDecorationThickness, float, text.DecorationThickness)
        CW_TEXT_SET(TextSetDecorationThickness, float, text.DecorationThickness = std::max(0.0f, value))
        CW_TEXT_GET(TextGetUnderlineOffset, float, text.UnderlineOffset)
        CW_TEXT_SET(TextSetUnderlineOffset, float, text.UnderlineOffset = value)
        CW_TEXT_GET(TextGetStrikethroughOffset, float, text.StrikethroughOffset)
        CW_TEXT_SET(TextSetStrikethroughOffset, float, text.StrikethroughOffset = value)
        CW_TEXT_GET(TextGetUseKerning, uint8_t, text.UseKerning ? 1 : 0)
        CW_TEXT_SET(TextSetUseKerning, uint8_t, text.UseKerning = value != 0)
        CW_TEXT_GET(TextGetSortingLayer, int32_t, text.SortingLayer)
        CW_TEXT_SET(TextSetSortingLayer, int32_t, text.SortingLayer = value)
        CW_TEXT_GET(TextGetOrderInLayer, int32_t, text.OrderInLayer)
        CW_TEXT_SET(TextSetOrderInLayer, int32_t, text.OrderInLayer = value)
#undef CW_TEXT_SET
#undef CW_TEXT_GET

#define CW_TEXT_GET_VECTOR(functionName, abiType, expression)                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, abiType* result)                       \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const auto& value = expression;                                                                                          \
            *result = abiType{ value.x, value.y, value.z, value.w };                                                                 \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TEXT_SET_VEC4(functionName, field)                                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec4* value)          \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            entity.GetComponent<TextComponent>().field = { value->x, value->y, value->z, value->w };                                \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TEXT_GET_VECTOR(TextGetColor, cw_managed_vec4, entity.GetComponent<TextComponent>().Color)
        CW_TEXT_SET_VEC4(TextSetColor, Color)
        CW_TEXT_GET_VECTOR(TextGetOutlineColor, cw_managed_vec4, entity.GetComponent<TextComponent>().OutlineColor)
        CW_TEXT_SET_VEC4(TextSetOutlineColor, OutlineColor)
        CW_TEXT_GET_VECTOR(TextGetShadowColor, cw_managed_vec4, entity.GetComponent<TextComponent>().ShadowColor)
        CW_TEXT_SET_VEC4(TextSetShadowColor, ShadowColor)
        CW_TEXT_GET_VECTOR(TextGetDecorationColor, cw_managed_vec4, entity.GetComponent<TextComponent>().DecorationColor)
        CW_TEXT_SET_VEC4(TextSetDecorationColor, DecorationColor)
#undef CW_TEXT_SET_VEC4
#undef CW_TEXT_GET_VECTOR

#define CW_TEXT_GET_VEC2(functionName, expression)                                                                                   \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)                \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::vec2 value = expression;                                                                                      \
            *result = { value.x, value.y };                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TEXT_SET_VEC2(functionName, statement)                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)          \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<TextComponent>())                                                                  \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            TextComponent& text = entity.GetComponent<TextComponent>();                                                             \
            const glm::vec2 vector(value->x, value->y);                                                                              \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TEXT_GET_VEC2(TextGetLayoutSize, entity.GetComponent<TextComponent>().LayoutSize)
        CW_TEXT_SET_VEC2(TextSetLayoutSize, text.LayoutSize = glm::max(vector, glm::vec2(0.0f)))
        CW_TEXT_GET_VEC2(TextGetShadowOffset, entity.GetComponent<TextComponent>().ShadowOffset)
        CW_TEXT_SET_VEC2(TextSetShadowOffset, text.ShadowOffset = vector)
#undef CW_TEXT_SET_VEC2
#undef CW_TEXT_GET_VEC2

#define CW_FONT_GET(functionName, resultType, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid fontId, resultType* result)                       \
    {                                                                                                                                \
        return Execute(context, [&]() -> cw_managed_status {                                                                        \
            AssetHandle<Font> font;                                                                                                  \
            const cw_managed_status status = ResolveFontHandle(fontId, font);                                                       \
            if (status != CW_MANAGED_STATUS_OK)                                                                                      \
                return status;                                                                                                       \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_FONT_GET(FontGetIsValid, uint8_t, font->IsValid() ? 1 : 0)
        CW_FONT_GET(FontGetGlyphCount, uint32_t,
                    static_cast<uint32_t>(std::min(font->GetGlyphCount(),
                                                   static_cast<size_t>(std::numeric_limits<uint32_t>::max()))))
        CW_FONT_GET(FontGetTabWidth, uint32_t, font->GetTabWidth())
        CW_FONT_GET(FontGetAtlasWidth, uint32_t, font->GetAtlasWidth())
        CW_FONT_GET(FontGetAtlasHeight, uint32_t, font->GetAtlasHeight())
        CW_FONT_GET(FontGetAtlasPixelRange, float, font->GetAtlasPixelRange())
        CW_FONT_GET(FontGetFallbackCount, uint32_t, static_cast<uint32_t>(font->GetFallbackFonts().size()))
#undef CW_FONT_GET

        cw_managed_status CW_MANAGED_CALL FontHasGlyph(void* context, cw_managed_uuid fontId, uint32_t codePoint,
                                                        uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = font->HasGlyph(static_cast<char32_t>(codePoint)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontGetCharacterInfo(void* context, cw_managed_uuid fontId,
                                                                uint32_t codePoint, uint8_t useFallbacks,
                                                                cw_managed_font_character_info* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = ToAbiCharacterInfo(font, font->GetCharacterInfo(static_cast<char32_t>(codePoint), useFallbacks != 0));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontGetFallback(void* context, cw_managed_uuid fontId, uint32_t index,
                                                           cw_managed_uuid* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const auto& fallbacks = font->GetFallbackFonts();
                const UUID fallbackId = index < fallbacks.size() ? fallbacks[index].GetUUID() : UUID{};
                *result = ToAbiUuid(fallbackId);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontAddFallback(void* context, cw_managed_uuid fontId,
                                                           cw_managed_uuid fallbackId, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                AssetHandle<Font> font;
                cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                AssetHandle<Font> fallback;
                status = ResolveFontHandle(fallbackId, fallback);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = font->AddFallbackFont(fallback) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontClearFallbacks(void* context, cw_managed_uuid fontId)
        {
            return Execute(context, [&]() -> cw_managed_status {
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                font->ClearFallbackFonts();
                return CW_MANAGED_STATUS_OK;
            });
        }

        glm::mat4 ToMatrix(const cw_managed_mat4& value)
        {
            glm::mat4 result(1.0f);
            std::memcpy(glm::value_ptr(result), value.values, sizeof(result));
            return result;
        }

        void WriteMatrix(const glm::mat4& value, cw_managed_mat4& result)
        {
            std::memcpy(result.values, glm::value_ptr(value), sizeof(value));
        }

        cw_managed_status CW_MANAGED_CALL MathMatrixDeterminant(void* context, const cw_managed_mat4* matrix, float* result)
        {
            return Execute(context, [&]() {
                if (matrix == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = glm::determinant(ToMatrix(*matrix));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_MATRIX_OPERATION(functionName, expression)                                                                                \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, const cw_managed_mat4* matrix, cw_managed_mat4* result)           \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (matrix == nullptr || result == nullptr)                                                                              \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            WriteMatrix(expression, *result);                                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_MATRIX_OPERATION(MathMatrixInverse, glm::inverse(ToMatrix(*matrix)))
        CW_MATRIX_OPERATION(MathMatrixAffineInverse, glm::affineInverse(ToMatrix(*matrix)))
#undef CW_MATRIX_OPERATION

        cw_managed_status CW_MANAGED_CALL MathLookAt(void* context, const cw_managed_vec3* from, const cw_managed_vec3* to,
                                                      const cw_managed_vec3* up, cw_managed_mat4* result)
        {
            return Execute(context, [&]() {
                if (from == nullptr || to == nullptr || up == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                WriteMatrix(glm::lookAt(glm::vec3(from->x, from->y, from->z), glm::vec3(to->x, to->y, to->z),
                                        glm::vec3(up->x, up->y, up->z)),
                            *result);
                return CW_MANAGED_STATUS_OK;
            });
        }
    } // namespace

    void PopulateManagedHostBindings(cw_managed_host_api& api)
    {
        api.get_entity_name = &GetEntityName;
        api.set_entity_name = &SetEntityName;
        api.find_entity_by_name = &FindEntityByName;
        api.get_entity_parent = &GetEntityParent;
        api.set_entity_parent = &SetEntityParent;
        api.destroy_entity = &DestroyEntity;
#define CW_ASSIGN_HOST_FUNCTION(functionName, fieldName) api.fieldName = &functionName;
        CW_MANAGED_HOST_FUNCTION_LIST(CW_ASSIGN_HOST_FUNCTION)
#undef CW_ASSIGN_HOST_FUNCTION
    }
} // namespace Crowny
