#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedAssetLeaseRegistry.h"
#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"
#include "Crowny/Scripting/Managed/ManagedComponentTypes.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioBus.h"
#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Common/ConsoleBuffer.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Noise.h"
#include "Crowny/Common/Random.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Physics/Physics3D.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/FontManager.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "Crowny/Renderer/TextLayout.h"
#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Utils/Compression.h"

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
                const uint8_t value = character >= '0' && character <= '9'   ? static_cast<uint8_t>(character - '0')
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

        template <typename T> AssetHandle<T> ResolveAsset(const cw_managed_uuid& assetId)
        {
            AssetManager* manager = AssetManager::TryGet();
            if (manager == nullptr)
                return {};
            const UUID uuid = FromAbiUuid(assetId);
            return uuid.Empty() ? AssetHandle<T>() : manager->LoadFromUUID<T>(uuid);
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
            return font ? font->FindFallbackFontUUID(source) : UUID{};
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

        template <typename T> T* ResolveComponent(const cw_managed_uuid& entityId)
        {
            Entity entity = ResolveEntity(entityId);
            return entity && entity.HasComponent<T>() ? &entity.GetComponent<T>() : nullptr;
        }

        cw_managed_status WriteBorrowedStringView(const String& value, cw_managed_string_view* result)
        {
            if (result == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            if (value.size() > std::numeric_limits<uint32_t>::max())
                return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
            *result = { reinterpret_cast<const uint8_t*>(value.data()), static_cast<uint32_t>(value.size()) };
            return CW_MANAGED_STATUS_OK;
        }

        Collider2D* ResolveCollider2D(Entity entity)
        {
            if (!entity)
                return nullptr;
            if (entity.HasComponent<BoxCollider2DComponent>())
                return &entity.GetComponent<BoxCollider2DComponent>();
            if (entity.HasComponent<CircleCollider2DComponent>())
                return &entity.GetComponent<CircleCollider2DComponent>();
            return nullptr;
        }

        Collider3D* ResolveCollider3D(Entity entity)
        {
            if (!entity)
                return nullptr;
            if (entity.HasComponent<BoxCollider3DComponent>())
                return &entity.GetComponent<BoxCollider3DComponent>();
            if (entity.HasComponent<SphereCollider3DComponent>())
                return &entity.GetComponent<SphereCollider3DComponent>();
            if (entity.HasComponent<CapsuleCollider3DComponent>())
                return &entity.GetComponent<CapsuleCollider3DComponent>();
            return nullptr;
        }

        cw_managed_physics_filter3d ToAbiFilter(const PhysicsFilter3D& filter) { return { filter.Layer, filter.Mask, filter.Group }; }

        PhysicsFilter3D FromAbiFilter(const cw_managed_physics_filter3d& filter) { return { filter.layer, filter.mask, filter.group }; }

        cw_managed_physics_material_override ToAbiMaterialOverride(const PhysicsMaterialOverride& materialOverride)
        {
            const PhysicsMaterialOverride normalized = NormalizePhysicsMaterialOverride(materialOverride);
            return { static_cast<uint32_t>(normalized.Fields),
                     normalized.Values.Density,
                     normalized.Values.Friction,
                     normalized.Values.Restitution,
                     normalized.Values.RestitutionThreshold,
                     static_cast<int32_t>(normalized.Values.FrictionCombine),
                     static_cast<int32_t>(normalized.Values.RestitutionCombine) };
        }

        PhysicsMaterialOverride FromAbiMaterialOverride(const cw_managed_physics_material_override& materialOverride)
        {
            PhysicsMaterialOverride result;
            result.Fields = PhysicsMaterialOverrideFlags(materialOverride.fields);
            result.Values.Density = materialOverride.density;
            result.Values.Friction = materialOverride.friction;
            result.Values.Restitution = materialOverride.restitution;
            result.Values.RestitutionThreshold = materialOverride.restitution_threshold;
            result.Values.FrictionCombine = static_cast<PhysicsCombineMode>(materialOverride.friction_combine);
            result.Values.RestitutionCombine = static_cast<PhysicsCombineMode>(materialOverride.restitution_combine);
            return NormalizePhysicsMaterialOverride(result);
        }

        template <typename Callback> cw_managed_status Execute(void* context, Callback&& callback)
        {
            if (context == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            try
            {
                ManagedAssetLeaseRegistry::Drain(context);
                return callback();
            }
            catch (...)
            {
                return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
            }
        }

        StringView DecodeView(cw_managed_string_view value)
        {
            return value.data == nullptr ? StringView() : StringView(reinterpret_cast<const char*>(value.data), value.length);
        }

        String Decode(cw_managed_string_view value) { return String(DecodeView(value)); }

        bool HasComponent(Entity entity, StringView name)
        {
#define CW_HAS_COMPONENT(managedName, nativeType, scriptType)                                                                                        \
    if (name == managedName)                                                                                                                         \
        return entity.HasComponent<nativeType>();
            CW_MANAGED_COMPONENT_TYPES(CW_HAS_COMPONENT)
#undef CW_HAS_COMPONENT
            return false;
        }

        bool AddComponent(Entity entity, StringView name)
        {
#define CW_ADD_COMPONENT(managedName, nativeType, scriptType)                                                                                        \
    if (name == managedName)                                                                                                                         \
    {                                                                                                                                                \
        entity.AddOrGetComponent<nativeType>();                                                                                                      \
        return true;                                                                                                                                 \
    }
            CW_MANAGED_COMPONENT_TYPES(CW_ADD_COMPONENT)
#undef CW_ADD_COMPONENT
            return false;
        }

        bool RemoveComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return false;
#define CW_REMOVE_COMPONENT(managedName, nativeType, scriptType)                                                                                     \
    if (name == managedName)                                                                                                                         \
    {                                                                                                                                                \
        entity.RemoveComponentIfExists<nativeType>();                                                                                                \
        return true;                                                                                                                                 \
    }
            CW_MANAGED_COMPONENT_TYPES(CW_REMOVE_COMPONENT)
#undef CW_REMOVE_COMPONENT
            return false;
        }

        cw_managed_status CW_MANAGED_CALL EntityHasComponent(void* context, cw_managed_uuid entityId, cw_managed_string_view typeName,
                                                             uint8_t* result)
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

        cw_managed_status CW_MANAGED_CALL EntityAddComponent(void* context, cw_managed_uuid entityId, cw_managed_string_view typeName)
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

        cw_managed_status CW_MANAGED_CALL EntityRemoveComponent(void* context, cw_managed_uuid entityId, cw_managed_string_view typeName)
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

        cw_managed_status CW_MANAGED_CALL AddScriptComponent(void* context, cw_managed_uuid entityId,
                                                             cw_managed_string_view assemblyName,
                                                             cw_managed_string_view namespaceName,
                                                             cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if ((assemblyName.data == nullptr && assemblyName.length != 0) ||
                    (namespaceName.data == nullptr && namespaceName.length != 0) ||
                    (typeName.data == nullptr && typeName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const ScriptTypeIdentity identity{ Decode(assemblyName), Decode(namespaceName), Decode(typeName) };
                return entity.GetScene()->AddScriptComponent(entity, identity) ? CW_MANAGED_STATUS_OK
                                                                               : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

        cw_managed_status CW_MANAGED_CALL RemoveScriptComponent(void* context, cw_managed_uuid entityId,
                                                                cw_managed_string_view assemblyName,
                                                                cw_managed_string_view namespaceName,
                                                                cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if ((assemblyName.data == nullptr && assemblyName.length != 0) ||
                    (namespaceName.data == nullptr && namespaceName.length != 0) ||
                    (typeName.data == nullptr && typeName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const ScriptTypeIdentity identity{ Decode(assemblyName), Decode(namespaceName), Decode(typeName) };
                if (!entity.GetScene()->HasScriptComponent(entity, identity))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                entity.GetScene()->RemoveScriptComponent(entity, identity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL GetEntityName(void* context, cw_managed_uuid entityId, cw_managed_string_view* name)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (name == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return WriteBorrowedStringView(entity.GetName(), name);
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
                String& currentName = entity.GetComponent<TagComponent>().Tag;
                const StringView nextName = DecodeView(name);
                if (currentName != nextName)
                    currentName.assign(nextName);
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
                    return entity.SetParent({}) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
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

#define CW_TRANSFORM_GET_VEC3(functionName, expression)                                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)                                 \
    {                                                                                                                                                \
        return Execute(context, [&]() -> cw_managed_status {                                                                                         \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity)                                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const glm::vec3 value = expression;                                                                                                      \
            *result = { value.x, value.y, value.z };                                                                                                 \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_TRANSFORM_SET_VEC3(functionName, statement)                                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)                            \
    {                                                                                                                                                \
        return Execute(context, [&]() -> cw_managed_status {                                                                                         \
            Entity entity = ResolveEntity(entityId);                                                                                                 \
            if (!entity)                                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const glm::vec3 vector(value->x, value->y, value->z);                                                                                    \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
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

#define CW_TRANSFORM_GET_QUAT(functionName, expression)                                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_quat* result)                                 \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity)                                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const glm::quat value = expression;                                                                                                      \
            *result = { value.x, value.y, value.z, value.w };                                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_TRANSFORM_SET_QUAT(functionName, statement)                                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_quat* value)                            \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                                 \
            if (!entity)                                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const glm::quat rotation(value->w, value->x, value->y, value->z);                                                                        \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_TRANSFORM_GET_QUAT(TransformGetRotation, entity.GetWorldRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetRotation, entity.SetWorldRotation(rotation))
        CW_TRANSFORM_GET_QUAT(TransformGetLocalRotation, entity.GetLocalRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetLocalRotation, entity.SetRotation(rotation))
#undef CW_TRANSFORM_SET_QUAT
#undef CW_TRANSFORM_GET_QUAT

#define CW_TRANSFORM_GET_MATRIX(functionName, expression)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_mat4* result)                                 \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity)                                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const glm::mat4 value = expression;                                                                                                      \
            std::memcpy(result->values, glm::value_ptr(value), sizeof(value));                                                                       \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
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

#define CW_INPUT_BUTTON(functionName, expression)                                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t code, uint8_t* result)                                                    \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = expression ? 1 : 0;                                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_INPUT_BUTTON(InputGetKey, Input::IsKeyPressed(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyDown, Input::IsKeyDown(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyUp, Input::IsKeyUp(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButton, Input::IsMouseButtonPressed(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonDown, Input::IsMouseButtonDown(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonUp, Input::IsMouseButtonUp(static_cast<MouseCode>(code)))
#undef CW_INPUT_BUTTON

#define CW_GLOBAL_FLOAT(functionName, expression)                                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, float* result)                                                                     \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_GLOBAL_FLOAT(InputGetMouseScrollX, Input::GetMouseScrollX())
        CW_GLOBAL_FLOAT(InputGetMouseScrollY, Input::GetMouseScrollY())
        CW_GLOBAL_FLOAT(TimeGetDeltaTime, Application::Get().GetTime().GetDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetTime, Application::Get().GetTime().GetTime())
        CW_GLOBAL_FLOAT(TimeGetFixedDeltaTime, Application::Get().GetTime().GetFixedDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetSmoothDeltaTime, Application::Get().GetTime().GetSmoothDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetRealtimeSinceStartup, Application::Get().GetTime().GetRealtimeSinceStartup())
#undef CW_GLOBAL_FLOAT

        cw_managed_status CW_MANAGED_CALL TimeGetFrameCount(void* context, uint32_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = static_cast<uint32_t>(Application::Get().GetTime().GetFrameCount());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL ScreenGetSize(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                float width = 0.0f;
                float height = 0.0f;
                SceneManager* manager = SceneManager::TryGet();
                const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
                if (scene != nullptr)
                {
                    width = static_cast<float>(scene->GetViewportWidth());
                    height = static_cast<float>(scene->GetViewportHeight());
                }
                // Runtime scenes without an explicit viewport render into the window framebuffer.
                if ((width <= 0.0f || height <= 0.0f) && Application::TryGet() != nullptr &&
                    !Application::TryGet()->GetApplicationDesc().Headless)
                {
                    const Window& window = Application::TryGet()->GetWindow();
                    width = static_cast<float>(window.GetFramebufferWidth());
                    height = static_cast<float>(window.GetFramebufferHeight());
                }
                *result = { width, height };
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
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetMouseDelta();
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputSetMouseGrabbed(void* context, uint8_t grabbed)
        {
            return Execute(context, [&]() {
                Input::SetMouseGrabbed(grabbed != 0);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputIsMouseGrabbed(void* context, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::IsMouseGrabbed() ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputSetCursorType(void* context, uint32_t type)
        {
            return Execute(context, [&]() {
                Input::SetCursorType(static_cast<Cursor>(type));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputGetCursorType(void* context, uint32_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = static_cast<uint32_t>(Input::GetCursorType());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputIsGamepadConnected(void* context, uint32_t gamepad, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::IsGamepadConnected(gamepad) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_GAMEPAD_BUTTON(functionName, expression)                                                                                                  \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t gamepad, uint32_t code, uint8_t* result)                                  \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = expression ? 1 : 0;                                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_GAMEPAD_BUTTON(InputGetGamepadButton, Input::IsGamepadButtonPressed(gamepad, static_cast<GamepadButtonCode>(code)))
        CW_GAMEPAD_BUTTON(InputGetGamepadButtonDown, Input::IsGamepadButtonDown(gamepad, static_cast<GamepadButtonCode>(code)))
        CW_GAMEPAD_BUTTON(InputGetGamepadButtonUp, Input::IsGamepadButtonUp(gamepad, static_cast<GamepadButtonCode>(code)))
#undef CW_GAMEPAD_BUTTON

        cw_managed_status CW_MANAGED_CALL InputGetGamepadAxis(void* context, uint32_t gamepad, uint32_t code, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::GetGamepadAxis(gamepad, static_cast<GamepadAxisCode>(code));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_INPUT_STRING_BOOLEAN(functionName, expression)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_string_view name, uint8_t* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr || (name.data == nullptr && name.length != 0))                                                                     \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = expression ? 1 : 0;                                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_INPUT_STRING_BOOLEAN(InputGetAction, Input::GetAction(Decode(name)))
        CW_INPUT_STRING_BOOLEAN(InputGetActionDown, Input::GetActionDown(Decode(name)))
        CW_INPUT_STRING_BOOLEAN(InputGetActionUp, Input::GetActionUp(Decode(name)))
        CW_INPUT_STRING_BOOLEAN(InputEnableActionMap, Input::SetActionMapEnabled(Decode(name), true))
        CW_INPUT_STRING_BOOLEAN(InputDisableActionMap, Input::SetActionMapEnabled(Decode(name), false))
#undef CW_INPUT_STRING_BOOLEAN

        cw_managed_status CW_MANAGED_CALL InputGetAxis(void* context, cw_managed_string_view name, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Input::GetAxis(Decode(name));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputGetActionVector(void* context, cw_managed_string_view name, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetActionVector(Decode(name));
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL InputClearActionRebinds(void* context)
        {
            return Execute(context, [&]() {
                Input::ClearActionRebinds();
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_RIGIDBODY_GET(functionName, resultType, expression)                                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                                           \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                                           \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_RIGIDBODY_SET(functionName, valueType, statement)                                                                                         \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                                           \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_RIGIDBODY_GET(Rigidbody2DGetMass, float, physics != nullptr ? physics->GetMass(entity) : rigidbody.GetMass())
        CW_RIGIDBODY_SET(Rigidbody2DSetMass, float, if (rigidbody.GetAutoMass()) return CW_MANAGED_STATUS_INVALID_ARGUMENT; rigidbody.SetMass(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetBodyType, int32_t, static_cast<int32_t>(rigidbody.GetBodyType()))
        CW_RIGIDBODY_SET(Rigidbody2DSetBodyType, int32_t, rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetSleepMode, int32_t, static_cast<int32_t>(rigidbody.GetSleepMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetSleepMode, int32_t, rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetCollisionDetectionMode, int32_t, static_cast<int32_t>(rigidbody.GetCollisionDetectionMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetCollisionDetectionMode, int32_t,
                         rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetInterpolation, int32_t, static_cast<int32_t>(rigidbody.GetInterpolationMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetInterpolation, int32_t, rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(value)))
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
        CW_RIGIDBODY_SET(Rigidbody2DSetAngularVelocity, float, if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetAngularVelocity(entity, value))
        CW_RIGIDBODY_GET(Rigidbody2DGetAwake, uint8_t, physics != nullptr && physics->IsBodyAwake(entity) ? 1 : 0)
        CW_RIGIDBODY_SET(Rigidbody2DSetAwake, uint8_t, if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetBodyAwake(entity, value != 0))
#undef CW_RIGIDBODY_SET
#undef CW_RIGIDBODY_GET

#define CW_RIGIDBODY_GET_VEC2(functionName, expression)                                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)                                 \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                                           \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                                           \
            const glm::vec2 value = expression;                                                                                                      \
            *result = { value.x, value.y };                                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_RIGIDBODY_SET_VEC2(functionName, statement)                                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)                            \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                                           \
            const glm::vec2 vector(value->x, value->y);                                                                                              \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetCenterOfMass, physics != nullptr ? physics->GetCenterOfMass(entity) : rigidbody.GetCenterOfMass())
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetCenterOfMass, rigidbody.SetCenterOfMass(vector))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetPosition, physics != nullptr ? physics->GetPosition(entity) : glm::vec2(entity.GetWorldPosition()))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetLinearVelocity, physics != nullptr ? physics->GetLinearVelocity(entity) : glm::vec2(0.0f))
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetLinearVelocity, if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetLinearVelocity(entity, vector))
#undef CW_RIGIDBODY_SET_VEC2
#undef CW_RIGIDBODY_GET_VEC2

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForce(void* context, cw_managed_uuid entityId, const cw_managed_vec2* force, int32_t mode)
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

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForceAtPosition(void* context, cw_managed_uuid entityId, const cw_managed_vec2* force,
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
                physics->AddForceAt(entity, glm::vec2(force->x, force->y), glm::vec2(position->x, position->y), static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddTorque(void* context, cw_managed_uuid entityId, float torque, int32_t mode)
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

#define CW_COLLIDER2D_GET(functionName, resultType, expression)                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            Collider2D* collider = ResolveCollider2D(entity);                                                                                        \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_COLLIDER2D_SET(functionName, valueType, statement)                                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            Collider2D* collider = ResolveCollider2D(entity);                                                                                        \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_COLLIDER2D_GET(Collider2DGetIsTrigger, uint8_t, collider->IsTrigger() ? 1 : 0)
        CW_COLLIDER2D_SET(Collider2DSetIsTrigger, uint8_t, collider->SetIsTrigger(value != 0))
        CW_COLLIDER2D_GET(Collider2DGetMaterial, cw_managed_uuid, ToAbiUuid(collider->GetMaterial().GetUUID()))
        CW_COLLIDER2D_SET(Collider2DSetMaterial, cw_managed_uuid, collider->SetMaterial(ResolveAsset<PhysicsMaterial2D>(value)))
#undef CW_COLLIDER2D_SET
#undef CW_COLLIDER2D_GET

        cw_managed_status CW_MANAGED_CALL Collider2DGetMaterialOverride(void* context, cw_managed_uuid entityId,
                                                                        cw_managed_physics_material_override* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider2D* collider = ResolveCollider2D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = ToAbiMaterialOverride(collider->GetMaterialOverride());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider2DSetMaterialOverride(void* context, cw_managed_uuid entityId,
                                                                        const cw_managed_physics_material_override* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider2D* collider = ResolveCollider2D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                collider->SetMaterialOverride(FromAbiMaterialOverride(*value));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider2DGetOffset(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider2D* collider = ResolveCollider2D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = { collider->GetOffset().x, collider->GetOffset().y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider2DSetOffset(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                Collider2D* collider = ResolveCollider2D(entity);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const glm::vec2 offset(value->x, value->y);
                if (entity.HasComponent<BoxCollider2DComponent>() && collider == &entity.GetComponent<BoxCollider2DComponent>())
                    static_cast<BoxCollider2DComponent*>(collider)->SetOffset(offset, entity);
                else if (entity.HasComponent<CircleCollider2DComponent>() && collider == &entity.GetComponent<CircleCollider2DComponent>())
                    static_cast<CircleCollider2DComponent*>(collider)->SetOffset(offset, entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL BoxCollider2DGetSize(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                BoxCollider2DComponent* collider = ResolveComponent<BoxCollider2DComponent>(entityId);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = { collider->GetSize().x, collider->GetSize().y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL BoxCollider2DSetSize(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity || !entity.HasComponent<BoxCollider2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                entity.GetComponent<BoxCollider2DComponent>().SetSize({ value->x, value->y }, entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CircleCollider2DGetRadius(void* context, cw_managed_uuid entityId, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                CircleCollider2DComponent* collider = ResolveComponent<CircleCollider2DComponent>(entityId);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = collider->GetRadius();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CircleCollider2DSetRadius(void* context, cw_managed_uuid entityId, float value)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                if (!entity || !entity.HasComponent<CircleCollider2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                entity.GetComponent<CircleCollider2DComponent>().SetRadius(value, entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_COLLIDER3D_GET(functionName, resultType, expression)                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            Collider3D* collider = ResolveCollider3D(entity);                                                                                        \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_COLLIDER3D_SET(functionName, valueType, statement)                                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            Collider3D* collider = ResolveCollider3D(entity);                                                                                        \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_COLLIDER3D_GET(Collider3DGetIsTrigger, uint8_t, collider->IsTrigger() ? 1 : 0)
        CW_COLLIDER3D_SET(Collider3DSetIsTrigger, uint8_t, collider->SetIsTrigger(value != 0))
        CW_COLLIDER3D_GET(Collider3DGetMaterial, cw_managed_uuid, ToAbiUuid(collider->GetMaterial().GetUUID()))
        CW_COLLIDER3D_SET(Collider3DSetMaterial, cw_managed_uuid, collider->SetMaterial(ResolveAsset<PhysicsMaterial3D>(value)))
#undef CW_COLLIDER3D_SET
#undef CW_COLLIDER3D_GET

        cw_managed_status CW_MANAGED_CALL Collider3DGetMaterialOverride(void* context, cw_managed_uuid entityId,
                                                                        cw_managed_physics_material_override* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider3D* collider = ResolveCollider3D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = ToAbiMaterialOverride(collider->GetMaterialOverride());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DSetMaterialOverride(void* context, cw_managed_uuid entityId,
                                                                        const cw_managed_physics_material_override* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider3D* collider = ResolveCollider3D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                collider->SetMaterialOverride(FromAbiMaterialOverride(*value));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DGetOffset(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider3D* collider = ResolveCollider3D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = { collider->GetOffset().x, collider->GetOffset().y, collider->GetOffset().z };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DSetOffset(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                Collider3D* collider = ResolveCollider3D(entity);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                collider->SetOffset({ value->x, value->y, value->z }, entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DGetRotation(void* context, cw_managed_uuid entityId, cw_managed_quat* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider3D* collider = ResolveCollider3D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const glm::quat rotation = collider->GetRotation();
                *result = { rotation.x, rotation.y, rotation.z, rotation.w };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DSetRotation(void* context, cw_managed_uuid entityId, const cw_managed_quat* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                Collider3D* collider = ResolveCollider3D(entity);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                collider->SetRotation(glm::quat(value->w, value->x, value->y, value->z), entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DGetCollisionFilter(void* context, cw_managed_uuid entityId, cw_managed_physics_filter3d* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Collider3D* collider = ResolveCollider3D(ResolveEntity(entityId));
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = ToAbiFilter(collider->GetFilter());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Collider3DSetCollisionFilter(void* context, cw_managed_uuid entityId,
                                                                       const cw_managed_physics_filter3d* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                Collider3D* collider = ResolveCollider3D(entity);
                if (collider == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                collider->SetFilter(FromAbiFilter(*value), entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_COLLIDER3D_VECTOR_PROPERTY(getterName, setterName, componentType, getter, setter)                                                         \
    cw_managed_status CW_MANAGED_CALL getterName(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)                                   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            componentType* collider = ResolveComponent<componentType>(entityId);                                                                     \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            const glm::vec3 vector = collider->getter();                                                                                             \
            *result = { vector.x, vector.y, vector.z };                                                                                              \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }                                                                                                                                                \
    cw_managed_status CW_MANAGED_CALL setterName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)                              \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<componentType>())                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            entity.GetComponent<componentType>().setter({ value->x, value->y, value->z }, entity);                                                   \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_COLLIDER3D_VECTOR_PROPERTY(BoxCollider3DGetSize, BoxCollider3DSetSize, BoxCollider3DComponent, GetSize, SetSize)
#undef CW_COLLIDER3D_VECTOR_PROPERTY

#define CW_COLLIDER3D_SCALAR_PROPERTY(getterName, setterName, componentType, getter, setter)                                                         \
    cw_managed_status CW_MANAGED_CALL getterName(void* context, cw_managed_uuid entityId, float* result)                                             \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            componentType* collider = ResolveComponent<componentType>(entityId);                                                                     \
            if (collider == nullptr)                                                                                                                 \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = collider->getter();                                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }                                                                                                                                                \
    cw_managed_status CW_MANAGED_CALL setterName(void* context, cw_managed_uuid entityId, float value)                                               \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<componentType>())                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            entity.GetComponent<componentType>().setter(value, entity);                                                                              \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_COLLIDER3D_SCALAR_PROPERTY(SphereCollider3DGetRadius, SphereCollider3DSetRadius, SphereCollider3DComponent, GetRadius, SetRadius)
        CW_COLLIDER3D_SCALAR_PROPERTY(CapsuleCollider3DGetRadius, CapsuleCollider3DSetRadius, CapsuleCollider3DComponent, GetRadius, SetRadius)
        CW_COLLIDER3D_SCALAR_PROPERTY(CapsuleCollider3DGetHeight, CapsuleCollider3DSetHeight, CapsuleCollider3DComponent, GetHeight, SetHeight)
#undef CW_COLLIDER3D_SCALAR_PROPERTY

#define CW_RIGIDBODY3D_GET(functionName, resultType, expression)                                                                                     \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);                                                           \
            if (body == nullptr)                                                                                                                     \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_RIGIDBODY3D_SET(functionName, valueType, statement)                                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<Rigidbody3DComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            Rigidbody3DComponent& body = entity.GetComponent<Rigidbody3DComponent>();                                                                \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_RIGIDBODY3D_GET(Rigidbody3DGetBodyType, int32_t, static_cast<int32_t>(body->GetBodyType()))
        CW_RIGIDBODY3D_SET(Rigidbody3DSetBodyType, int32_t,
                           if (value >= static_cast<int32_t>(PhysicsBodyType3D::Static) &&
                               value <= static_cast<int32_t>(PhysicsBodyType3D::Kinematic))
                             body.SetBodyType(static_cast<PhysicsBodyType3D>(value), entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetMass, float, body->GetMass())
        CW_RIGIDBODY3D_SET(Rigidbody3DSetMass, float, body.SetMass(value, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetAutoMass, uint8_t, body->GetAutoMass() ? 1 : 0)
        CW_RIGIDBODY3D_SET(Rigidbody3DSetAutoMass, uint8_t, body.SetAutoMass(value != 0, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetGravityScale, float, body->GetGravityScale())
        CW_RIGIDBODY3D_SET(Rigidbody3DSetGravityScale, float, body.SetGravityScale(value))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetLinearDamping, float, body->GetLinearDamping())
        CW_RIGIDBODY3D_SET(Rigidbody3DSetLinearDamping, float, body.SetDamping(value, body.GetAngularDamping()))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetAngularDamping, float, body->GetAngularDamping())
        CW_RIGIDBODY3D_SET(Rigidbody3DSetAngularDamping, float, body.SetDamping(body.GetLinearDamping(), value))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetAllowSleep, uint8_t, body->GetAllowSleep() ? 1 : 0)
        CW_RIGIDBODY3D_SET(Rigidbody3DSetAllowSleep, uint8_t, body.SetAllowSleep(value != 0, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetStartAwake, uint8_t, body->GetStartAwake() ? 1 : 0)
        CW_RIGIDBODY3D_SET(Rigidbody3DSetStartAwake, uint8_t, body.SetStartAwake(value != 0, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetContinuousCollision, uint8_t, body->GetContinuousCollision() ? 1 : 0)
        CW_RIGIDBODY3D_SET(Rigidbody3DSetContinuousCollision, uint8_t, body.SetContinuousCollision(value != 0, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetConstraints, uint32_t,
                           (body->GetLockRotationX() ? 1u : 0u) | (body->GetLockRotationY() ? 2u : 0u) | (body->GetLockRotationZ() ? 4u : 0u))
        CW_RIGIDBODY3D_SET(Rigidbody3DSetConstraints, uint32_t,
                           body.SetRotationLocks((value & 1u) != 0, (value & 2u) != 0, (value & 4u) != 0, entity))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetAwake, uint8_t, body->IsAwake() ? 1 : 0)
        CW_RIGIDBODY3D_SET(Rigidbody3DSetAwake, uint8_t, body.SetAwake(value != 0))
        CW_RIGIDBODY3D_GET(Rigidbody3DGetBodyHandle, uint64_t, body->RuntimeBody.Value)
#undef CW_RIGIDBODY3D_SET
#undef CW_RIGIDBODY3D_GET

#define CW_RIGIDBODY3D_VECTOR(getterName, setterName, getter, setter)                                                                                \
    cw_managed_status CW_MANAGED_CALL getterName(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)                                   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);                                                           \
            if (body == nullptr)                                                                                                                     \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            const glm::vec3 vector = body->getter();                                                                                                 \
            *result = { vector.x, vector.y, vector.z };                                                                                              \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }                                                                                                                                                \
    cw_managed_status CW_MANAGED_CALL setterName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)                              \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);                                                           \
            if (body == nullptr)                                                                                                                     \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            body->setter({ value->x, value->y, value->z });                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_RIGIDBODY3D_VECTOR(Rigidbody3DGetLinearVelocity, Rigidbody3DSetLinearVelocity, GetLinearVelocity, SetLinearVelocity)
        CW_RIGIDBODY3D_VECTOR(Rigidbody3DGetAngularVelocity, Rigidbody3DSetAngularVelocity, GetAngularVelocity, SetAngularVelocity)
#undef CW_RIGIDBODY3D_VECTOR

        cw_managed_status CW_MANAGED_CALL Rigidbody3DGetCenterOfMass(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);
                if (body == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const glm::vec3 value = body->GetCenterOfMass();
                *result = { value.x, value.y, value.z };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody3DSetCenterOfMass(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity || !entity.HasComponent<Rigidbody3DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                entity.GetComponent<Rigidbody3DComponent>().SetCenterOfMass({ value->x, value->y, value->z }, entity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody3DGetCollisionFilter(void* context, cw_managed_uuid entityId, cw_managed_physics_filter3d* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);
                if (body == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = ToAbiFilter(body->GetFilter());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody3DSetCollisionFilter(void* context, cw_managed_uuid entityId,
                                                                        const cw_managed_physics_filter3d* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);
                if (body == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                body->SetFilter(FromAbiFilter(*value));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_RIGIDBODY3D_ACTION(functionName, vectorName, statement)                                                                                   \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* vectorName, int32_t mode)         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (vectorName == nullptr)                                                                                                               \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);                                                           \
            if (body == nullptr)                                                                                                                     \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (mode >= static_cast<int32_t>(PhysicsForceMode3D::Force) && mode <= static_cast<int32_t>(PhysicsForceMode3D::Acceleration))           \
                statement;                                                                                                                           \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_RIGIDBODY3D_ACTION(Rigidbody3DAddForce, force, body->AddForce({ force->x, force->y, force->z }, static_cast<PhysicsForceMode3D>(mode)))
        CW_RIGIDBODY3D_ACTION(Rigidbody3DAddTorque, torque,
                              body->AddTorque({ torque->x, torque->y, torque->z }, static_cast<PhysicsForceMode3D>(mode)))
#undef CW_RIGIDBODY3D_ACTION

        cw_managed_status CW_MANAGED_CALL Rigidbody3DAddForceAt(void* context, cw_managed_uuid entityId, const cw_managed_vec3* force,
                                                                const cw_managed_vec3* position, int32_t mode)
        {
            return Execute(context, [&]() {
                if (force == nullptr || position == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Rigidbody3DComponent* body = ResolveComponent<Rigidbody3DComponent>(entityId);
                if (body == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (mode >= static_cast<int32_t>(PhysicsForceMode3D::Force) && mode <= static_cast<int32_t>(PhysicsForceMode3D::Acceleration))
                    body->AddForceAt({ force->x, force->y, force->z }, { position->x, position->y, position->z },
                                     static_cast<PhysicsForceMode3D>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_AUDIO_GET(functionName, resultType, expression)                                                                                           \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                                              \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_AUDIO_SET(functionName, valueType, statement)                                                                                             \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                                              \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
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
        CW_AUDIO_SET(AudioSourceSetClip, cw_managed_uuid, if (const UUID uuid = FromAbiUuid(value); uuid.Empty()) source.SetClip({});
                     else if (AssetManager::IsStartedUp()) source.SetClip(AssetManager::TryGet()->LoadFromUUID<AudioClip>(uuid));
                     else return CW_MANAGED_STATUS_NOT_INITIALIZED)
        CW_AUDIO_GET(AudioSourceGetState, int32_t, static_cast<int32_t>(source.GetState()))
#undef CW_AUDIO_SET
#undef CW_AUDIO_GET

#define CW_AUDIO_ACTION(functionName, statement)                                                                                                     \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId)                                                          \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                                           \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                                              \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_AUDIO_ACTION(AudioSourcePlay, source.Play())
        CW_AUDIO_ACTION(AudioSourcePause, source.Pause())
        CW_AUDIO_ACTION(AudioSourceStop, source.Stop())
#undef CW_AUDIO_ACTION

        cw_managed_status CW_MANAGED_CALL AssetGetName(void* context, cw_managed_uuid assetId, cw_managed_string_view* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Asset> asset = ResolveAsset<Asset>(assetId);
                if (!asset.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                thread_local String storage;
                storage = asset->GetName();
                return WriteBorrowedStringView(storage, result);
            });
        }

        cw_managed_status CW_MANAGED_CALL AssetAcquire(void* context, cw_managed_uuid assetId)
        {
            return Execute(context, [&]() {
                return ManagedAssetLeaseRegistry::Acquire(context, FromAbiUuid(assetId)) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_STALE_HANDLE;
            });
        }

        cw_managed_status CW_MANAGED_CALL AssetRelease(void* context, cw_managed_uuid assetId)
        {
            if (context == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            ManagedAssetLeaseRegistry::QueueRelease(context, FromAbiUuid(assetId));
            return CW_MANAGED_STATUS_OK;
        }

        cw_managed_status CW_MANAGED_CALL AssetDatabaseLoad(void* context, cw_managed_string_view path, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (path.data == nullptr && path.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetManager* manager = AssetManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const AssetHandle<Asset> asset = manager->Load(Path(Decode(path)));
                if (!ManagedAssetLeaseRegistry::Acquire(context, asset))
                {
                    *result = {};
                    return CW_MANAGED_STATUS_OK;
                }
                *result = ToAbiUuid(asset.GetUUID());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AssetDatabaseLoadFromUuid(void* context, cw_managed_uuid assetId, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Asset> asset = ResolveAsset<Asset>(assetId);
                if (!ManagedAssetLeaseRegistry::Acquire(context, asset))
                {
                    *result = {};
                    return CW_MANAGED_STATUS_OK;
                }
                *result = ToAbiUuid(asset.GetUUID());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AssetDatabaseGetPath(void* context, cw_managed_uuid assetId, cw_managed_string_view* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetManager* manager = AssetManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                thread_local String storage;
                Path path;
                storage = manager->GetAssetPath(FromAbiUuid(assetId), path) ? path.string() : String();
                if (storage.size() > std::numeric_limits<uint32_t>::max())
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                result->data = reinterpret_cast<const uint8_t*>(storage.data());
                result->length = static_cast<uint32_t>(storage.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AssetDatabaseIsValid(void* context, cw_managed_uuid assetId, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetManager* manager = AssetManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                Path path;
                const UUID uuid = FromAbiUuid(assetId);
                *result = !uuid.Empty() && manager->GetAssetPath(uuid, path) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_ASSET_GET(functionName, assetType, resultType, expression)                                                                                \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, resultType* result)                                       \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<assetType> asset = ResolveAsset<assetType>(assetId);                                                                   \
            if (!asset.IsLoaded())                                                                                                                   \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_ASSET_GET(AudioClipGetBitDepth, AudioClip, int32_t, static_cast<int32_t>(asset->GetDesc().BitDepth))
        CW_ASSET_GET(AudioClipGetChannels, AudioClip, int32_t, static_cast<int32_t>(asset->GetDesc().NumChannels))
        CW_ASSET_GET(AudioClipGetFrequency, AudioClip, int32_t, static_cast<int32_t>(asset->GetDesc().Frequency))
        CW_ASSET_GET(AudioClipGetSamples, AudioClip, int32_t, static_cast<int32_t>(asset->GetNumSamples()))
        CW_ASSET_GET(AudioClipGetLength, AudioClip, float, asset->GetLength())
        CW_ASSET_GET(AudioClipGetReadMode, AudioClip, int32_t, static_cast<int32_t>(asset->GetDesc().ReadMode))
        CW_ASSET_GET(AudioClipGetFormat, AudioClip, int32_t, static_cast<int32_t>(asset->GetDesc().Format))
        CW_ASSET_GET(AudioClipGetIs3D, AudioClip, uint8_t, asset->GetDesc().Is3D ? 1 : 0)
        CW_ASSET_GET(TextureGetWidth, Texture, uint32_t, asset->GetWidth())
        CW_ASSET_GET(TextureGetHeight, Texture, uint32_t, asset->GetHeight())
#undef CW_ASSET_GET

        cw_managed_status CW_MANAGED_CALL AudioMixerSetActive(void* context, cw_managed_uuid assetId)
        {
            return Execute(context, [&]() {
                const AssetHandle<AudioMixer> mixer = ResolveAsset<AudioMixer>(assetId);
                if (!mixer.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                AudioManager* manager = AudioManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                manager->SetActiveMixer(mixer);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AudioMixerGetBusVolume(void* context, cw_managed_uuid assetId, cw_managed_string_view name, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<AudioMixer> mixer = ResolveAsset<AudioMixer>(assetId);
                if (!mixer.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<AudioBus> bus = mixer->FindBus(Decode(name));
                *result = bus != nullptr ? bus->GetVolume() : 0.0f;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AudioMixerSetBusVolume(void* context, cw_managed_uuid assetId, cw_managed_string_view name, float volume)
        {
            return Execute(context, [&]() {
                if (name.data == nullptr && name.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<AudioMixer> mixer = ResolveAsset<AudioMixer>(assetId);
                if (!mixer.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const String busName = Decode(name);
                for (AudioBusDesc& bus : mixer->GetBusDescs())
                {
                    if (bus.Name == busName)
                    {
                        bus.Volume = volume;
                        break;
                    }
                }
                mixer->SyncRuntimeFromDescs();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AudioMixerIsBusMuted(void* context, cw_managed_uuid assetId, cw_managed_string_view name, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<AudioMixer> mixer = ResolveAsset<AudioMixer>(assetId);
                if (!mixer.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<AudioBus> bus = mixer->FindBus(Decode(name));
                *result = bus != nullptr && bus->IsMuted() ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL AudioMixerSetBusMuted(void* context, cw_managed_uuid assetId, cw_managed_string_view name, uint8_t muted)
        {
            return Execute(context, [&]() {
                if (name.data == nullptr && name.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<AudioMixer> mixer = ResolveAsset<AudioMixer>(assetId);
                if (!mixer.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const String busName = Decode(name);
                for (AudioBusDesc& bus : mixer->GetBusDescs())
                {
                    if (bus.Name == busName)
                    {
                        bus.Muted = muted != 0;
                        break;
                    }
                }
                mixer->SyncRuntimeFromDescs();
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_MATERIAL_SET_SCALAR(functionName, valueType, statement)                                                                                   \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, cw_managed_string_view name, valueType value)             \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (name.data == nullptr && name.length != 0)                                                                                            \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<Material> material = ResolveAsset<Material>(assetId);                                                                  \
            if (!material.IsLoaded())                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MATERIAL_SET_SCALAR(MaterialSetFloat, float, material->SetFloat(Decode(name), value))
        CW_MATERIAL_SET_SCALAR(MaterialSetInt, int32_t, material->SetInt(Decode(name), value))
        CW_MATERIAL_SET_SCALAR(MaterialSetTexture, cw_managed_uuid, material->SetTexture(Decode(name), ResolveAsset<Texture>(value)))
#undef CW_MATERIAL_SET_SCALAR

        cw_managed_status CW_MANAGED_CALL MaterialSetVector2(void* context, cw_managed_uuid assetId, cw_managed_string_view name,
                                                             const cw_managed_vec2* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                material->SetFloat2(Decode(name), { value->x, value->y });
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialSetColor(void* context, cw_managed_uuid assetId, cw_managed_string_view name,
                                                           const cw_managed_vec4* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                material->SetColor(Decode(name), { value->x, value->y, value->z, value->w });
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialSetVector3(void* context, cw_managed_uuid assetId, cw_managed_string_view name,
                                                             const cw_managed_vec3* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                material->SetVector3(Decode(name), { value->x, value->y, value->z });
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialSetMatrix(void* context, cw_managed_uuid assetId, cw_managed_string_view name,
                                                            const cw_managed_mat4* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                glm::mat4 matrix(1.0f);
                std::memcpy(glm::value_ptr(matrix), value->values, sizeof(matrix));
                material->SetMatrix(Decode(name), matrix);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialHasAlphaModeOverride(void* context, cw_managed_uuid assetId, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = material->HasAlphaModeOverride() ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialGetAlphaMode(void* context, cw_managed_uuid assetId, int32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = static_cast<int32_t>(material->GetAlphaMode());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialSetAlphaMode(void* context, cw_managed_uuid assetId, int32_t alphaMode)
        {
            return Execute(context, [&]() {
                if (alphaMode < static_cast<int32_t>(AlphaMode::Opaque) || alphaMode > static_cast<int32_t>(AlphaMode::WeightedOIT))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                material->SetAlphaMode(static_cast<AlphaMode>(alphaMode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialClearAlphaModeOverride(void* context, cw_managed_uuid assetId)
        {
            return Execute(context, [&]() {
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                material->ClearAlphaModeOverride();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MaterialApplyToonPreset(void* context, cw_managed_uuid assetId, int32_t preset, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || preset < static_cast<int32_t>(ToonMaterialPreset::Classic) ||
                    preset > static_cast<int32_t>(ToonMaterialPreset::Hatched))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Material> material = ResolveAsset<Material>(assetId);
                if (!material.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = material->ApplyToonPreset(static_cast<ToonMaterialPreset>(preset)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL PhysicsMaterial2DCreate(void* context, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetManager* manager = AssetManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const AssetHandle<PhysicsMaterial2D> material = CreateRuntimePhysicsMaterial2D(*manager);
                if (!ManagedAssetLeaseRegistry::Acquire(context, material))
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                *result = ToAbiUuid(material.GetUUID());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL PhysicsMaterial3DCreate(void* context, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetManager* manager = AssetManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const AssetHandle<PhysicsMaterial3D> material = CreateRuntimePhysicsMaterial3D(*manager);
                if (!ManagedAssetLeaseRegistry::Acquire(context, material))
                    return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
                *result = ToAbiUuid(material.GetUUID());
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_PHYSICS_MATERIAL_GET(functionName, assetType, resultType, expression)                                                                     \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, resultType* result)                                       \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<assetType> material = ResolveAsset<assetType>(assetId);                                                                \
            if (!material.IsLoaded())                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_PHYSICS_MATERIAL_SET(functionName, assetType, valueType, statement)                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, valueType value)                                          \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const AssetHandle<assetType> material = ResolveAsset<assetType>(assetId);                                                                \
            if (!material.IsLoaded())                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetDensity, PhysicsMaterial2D, float, material->GetDensity())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetDensity, PhysicsMaterial2D, float, material->SetDensity(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetFriction, PhysicsMaterial2D, float, material->GetFriction())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetFriction, PhysicsMaterial2D, float, material->SetFriction(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetRestitution, PhysicsMaterial2D, float, material->GetRestitution())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetRestitution, PhysicsMaterial2D, float, material->SetRestitution(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetRestitutionThreshold, PhysicsMaterial2D, float, material->GetRestitutionThreshold())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetRestitutionThreshold, PhysicsMaterial2D, float, material->SetRestitutionThreshold(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetFrictionCombine, PhysicsMaterial2D, int32_t, static_cast<int32_t>(material->GetFrictionCombine()))
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetFrictionCombine, PhysicsMaterial2D, int32_t,
                                material->SetFrictionCombine(static_cast<PhysicsCombineMode>(value)))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial2DGetRestitutionCombine, PhysicsMaterial2D, int32_t,
                                static_cast<int32_t>(material->GetRestitutionCombine()))
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial2DSetRestitutionCombine, PhysicsMaterial2D, int32_t,
                                material->SetRestitutionCombine(static_cast<PhysicsCombineMode>(value)))

        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetDensity, PhysicsMaterial3D, float, material->GetDensity())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetDensity, PhysicsMaterial3D, float, material->SetDensity(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetFriction, PhysicsMaterial3D, float, material->GetFriction())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetFriction, PhysicsMaterial3D, float, material->SetFriction(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetRestitution, PhysicsMaterial3D, float, material->GetRestitution())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetRestitution, PhysicsMaterial3D, float, material->SetRestitution(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetRestitutionThreshold, PhysicsMaterial3D, float, material->GetRestitutionThreshold())
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetRestitutionThreshold, PhysicsMaterial3D, float, material->SetRestitutionThreshold(value))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetFrictionCombine, PhysicsMaterial3D, int32_t, static_cast<int32_t>(material->GetFrictionCombine()))
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetFrictionCombine, PhysicsMaterial3D, int32_t,
                                material->SetFrictionCombine(static_cast<PhysicsCombineMode>(value)))
        CW_PHYSICS_MATERIAL_GET(PhysicsMaterial3DGetRestitutionCombine, PhysicsMaterial3D, int32_t,
                                static_cast<int32_t>(material->GetRestitutionCombine()))
        CW_PHYSICS_MATERIAL_SET(PhysicsMaterial3DSetRestitutionCombine, PhysicsMaterial3D, int32_t,
                                material->SetRestitutionCombine(static_cast<PhysicsCombineMode>(value)))
#undef CW_PHYSICS_MATERIAL_SET
#undef CW_PHYSICS_MATERIAL_GET

        const Vector<Path>& GetSystemFontPaths()
        {
            static const Vector<Path> paths = []() {
                Vector<Path> result;
                Vector<Path> roots;
#if defined(CW_PLATFORM_WIN32)
                roots.emplace_back("C:/Windows/Fonts");
#elif defined(CW_PLATFORM_LINUX)
                roots.emplace_back("/usr/share/fonts");
                roots.emplace_back("/usr/local/share/fonts");
#endif
                for (const Path& root : roots)
                {
                    std::error_code error;
                    if (!fs::is_directory(root, error))
                        continue;
                    for (fs::recursive_directory_iterator entry(root, fs::directory_options::skip_permission_denied, error), end; entry != end;
                         entry.increment(error))
                    {
                        if (error)
                        {
                            error.clear();
                            continue;
                        }
                        if (!entry->is_regular_file(error))
                            continue;
                        String extension = entry->path().extension().string();
                        StringUtils::ToLower(extension);
                        if (extension == ".ttf" || extension == ".otf" || extension == ".ttc")
                            result.push_back(entry->path());
                    }
                }
                std::sort(result.begin(), result.end());
                result.erase(std::unique(result.begin(), result.end()), result.end());
                return result;
            }();
            return paths;
        }

        cw_managed_status CW_MANAGED_CALL FontHasCharacter(void* context, cw_managed_uuid assetId, uint32_t codePoint, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Font> font = ResolveAsset<Font>(assetId);
                if (!font.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = font->GetGlyph(static_cast<char32_t>(codePoint)) != nullptr ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_FONT_GET(functionName, resultType, expression)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid fontId, resultType* result)                                        \
    {                                                                                                                                                \
        return Execute(context, [&]() -> cw_managed_status {                                                                                         \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            AssetHandle<Font> font;                                                                                                                  \
            const cw_managed_status status = ResolveFontHandle(fontId, font);                                                                        \
            if (status != CW_MANAGED_STATUS_OK)                                                                                                      \
                return status;                                                                                                                       \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_FONT_GET(FontGetIsValid, uint8_t, font->IsValid() ? 1 : 0)
        CW_FONT_GET(FontGetGlyphCount, uint32_t,
                    static_cast<uint32_t>(std::min(font->GetGlyphCount(), static_cast<size_t>(std::numeric_limits<uint32_t>::max()))))
        CW_FONT_GET(FontGetTabWidth, uint32_t, font->GetTabWidth())
        CW_FONT_GET(FontGetAtlasWidth, uint32_t, font->GetAtlasWidth())
        CW_FONT_GET(FontGetAtlasHeight, uint32_t, font->GetAtlasHeight())
        CW_FONT_GET(FontGetAtlasPixelRange, float, font->GetAtlasPixelRange())
        CW_FONT_GET(FontGetFallbackCount, uint32_t, static_cast<uint32_t>(font->GetFallbackFonts().size()))
#undef CW_FONT_GET

        cw_managed_status CW_MANAGED_CALL FontHasGlyph(void* context, cw_managed_uuid fontId, uint32_t codePoint, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                *result = font->HasGlyph(static_cast<char32_t>(codePoint)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontGetCharacterInfo(void* context, cw_managed_uuid fontId, uint32_t codePoint, uint8_t useFallbacks,
                                                               cw_managed_font_character_info* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                *result = ToAbiCharacterInfo(font, font->GetCharacterInfo(static_cast<char32_t>(codePoint), useFallbacks != 0));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontGetFallback(void* context, cw_managed_uuid fontId, uint32_t index, cw_managed_uuid* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                const auto& fallbacks = font->GetFallbackFonts();
                *result = ToAbiUuid(index < fallbacks.size() ? fallbacks[index].GetUUID() : UUID{});
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontAddFallback(void* context, cw_managed_uuid fontId, cw_managed_uuid value, uint8_t* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                AssetHandle<Font> font;
                const cw_managed_status status = ResolveFontHandle(fontId, font);
                if (status != CW_MANAGED_STATUS_OK)
                    return status;
                AssetHandle<Font> fallback;
                const cw_managed_status fallbackStatus = ResolveFontHandle(value, fallback);
                if (fallbackStatus != CW_MANAGED_STATUS_OK)
                    return fallbackStatus;
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

        cw_managed_status CW_MANAGED_CALL FontGetSystemFontCount(void* context, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const size_t count = GetSystemFontPaths().size();
                if (count > std::numeric_limits<uint32_t>::max())
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                *result = static_cast<uint32_t>(count);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status WriteSystemFontString(void* context, uint32_t index, bool nameOnly, cw_managed_string_view* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || index >= GetSystemFontPaths().size())
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                thread_local String storage;
                storage = nameOnly ? GetSystemFontPaths()[index].stem().string() : GetSystemFontPaths()[index].string();
                if (storage.size() > std::numeric_limits<uint32_t>::max())
                    return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
                result->data = reinterpret_cast<const uint8_t*>(storage.data());
                result->length = static_cast<uint32_t>(storage.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL FontGetSystemFontPath(void* context, uint32_t index, cw_managed_string_view* result)
        {
            return WriteSystemFontString(context, index, false, result);
        }

        cw_managed_status CW_MANAGED_CALL FontGetSystemFontName(void* context, uint32_t index, cw_managed_string_view* result)
        {
            return WriteSystemFontString(context, index, true, result);
        }

#define CW_ANIMATION_CLIP_GET(functionName, resultType, expression)                                                                                  \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, resultType* result)                                       \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<AnimationClip> clip = ResolveAsset<AnimationClip>(assetId);                                                            \
            if (!clip.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_ANIMATION_CLIP_GET(AnimationClipGetLength, float, clip->GetLength())
        CW_ANIMATION_CLIP_GET(AnimationClipGetSampleRate, float, clip->GetSampleRate())
        CW_ANIMATION_CLIP_GET(AnimationClipGetIsAdditive, uint8_t, clip->IsAdditive() ? 1 : 0)
#undef CW_ANIMATION_CLIP_GET

#define CW_ANIMATION_GET(functionName, resultType, expression)                                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            AnimationComponent* animation = ResolveComponent<AnimationComponent>(entityId);                                                          \
            if (animation == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_ANIMATION_SET(functionName, valueType, statement)                                                                                         \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            AnimationComponent* animation = ResolveComponent<AnimationComponent>(entityId);                                                          \
            if (animation == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_ANIMATION_GET(AnimationComponentGetClip, cw_managed_uuid, ToAbiUuid(animation->GetClip().GetUUID()))
        CW_ANIMATION_SET(AnimationComponentSetClip, cw_managed_uuid, animation->SetClip(ResolveAsset<AnimationClip>(value)))
        CW_ANIMATION_GET(AnimationComponentGetSpeed, float, animation->GetSpeed())
        CW_ANIMATION_SET(AnimationComponentSetSpeed, float, animation->SetSpeed(value))
        CW_ANIMATION_GET(AnimationComponentGetWrapMode, int32_t, static_cast<int32_t>(animation->GetWrapMode()))
        CW_ANIMATION_SET(AnimationComponentSetWrapMode, int32_t, animation->SetWrapMode(static_cast<AnimationWrapMode>(value)))
        CW_ANIMATION_GET(AnimationComponentGetPlayOnAwake, uint8_t, animation->GetPlayOnAwake() ? 1 : 0)
        CW_ANIMATION_SET(AnimationComponentSetPlayOnAwake, uint8_t, animation->SetPlayOnAwake(value != 0))
        CW_ANIMATION_GET(AnimationComponentGetApplyRootMotion, uint8_t, animation->GetApplyRootMotion() ? 1 : 0)
        CW_ANIMATION_SET(AnimationComponentSetApplyRootMotion, uint8_t, animation->SetApplyRootMotion(value != 0))
        CW_ANIMATION_GET(AnimationComponentGetTime, float, animation->GetTime())
        CW_ANIMATION_SET(AnimationComponentSetTime, float, animation->SetTime(value))
        CW_ANIMATION_GET(AnimationComponentGetNormalizedTime, float, animation->GetNormalizedTime())
        CW_ANIMATION_SET(AnimationComponentSetNormalizedTime, float, animation->SetNormalizedTime(value))
        CW_ANIMATION_GET(AnimationComponentGetState, int32_t, static_cast<int32_t>(animation->GetState()))
#undef CW_ANIMATION_SET
#undef CW_ANIMATION_GET

#define CW_ANIMATION_COMMAND(functionName, statement)                                                                                                \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId)                                                          \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            AnimationComponent* animation = ResolveComponent<AnimationComponent>(entityId);                                                          \
            if (animation == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_ANIMATION_COMMAND(AnimationComponentPlay, animation->Play())
        CW_ANIMATION_COMMAND(AnimationComponentPause, animation->Pause())
        CW_ANIMATION_COMMAND(AnimationComponentStop, animation->Stop())
#undef CW_ANIMATION_COMMAND

        cw_managed_status RunFileDialog(FileDialogType type, cw_managed_string_view title, cw_managed_string_view directory,
                                        cw_managed_string_view extensions, cw_managed_string_view defaultName, cw_managed_string_view* result)
        {
            if (result == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            *result = {};
#if defined(CW_PLATFORM_WIN32) || defined(CW_PLATFORM_LINUX)
            Vector<Path> paths;
            FileDialogOptions options;
            options.Type = type;
            options.Title = Decode(title);
            options.InitialDirectory = Path(Decode(directory));
            options.Filters = FileSystem::ParseDialogFilters(Decode(extensions));
            options.DefaultName = Decode(defaultName);
            if (!FileSystem::OpenFileDialog(options, paths) || paths.empty())
                return CW_MANAGED_STATUS_OK;
            thread_local String storage;
            storage = paths.front().string();
            if (storage.size() > std::numeric_limits<uint32_t>::max())
                return CW_MANAGED_STATUS_BUFFER_WRITE_FAILED;
            result->data = reinterpret_cast<const uint8_t*>(storage.data());
            result->length = static_cast<uint32_t>(storage.size());
            return CW_MANAGED_STATUS_OK;
#else
            (void)type;
            (void)title;
            (void)directory;
            (void)extensions;
            (void)defaultName;
            return CW_MANAGED_STATUS_NOT_INITIALIZED;
#endif
        }

        cw_managed_status CW_MANAGED_CALL FileDialogOpenFile(void* context, cw_managed_string_view title, cw_managed_string_view directory,
                                                             cw_managed_string_view extensions, cw_managed_string_view* result)
        {
            return Execute(context, [&]() { return RunFileDialog(FileDialogType::OpenFile, title, directory, extensions, {}, result); });
        }

        cw_managed_status CW_MANAGED_CALL FileDialogOpenFolder(void* context, cw_managed_string_view title, cw_managed_string_view directory,
                                                               cw_managed_string_view* result)
        {
            return Execute(context, [&]() { return RunFileDialog(FileDialogType::OpenFolder, title, directory, {}, {}, result); });
        }

        cw_managed_status CW_MANAGED_CALL FileDialogSaveFile(void* context, cw_managed_string_view title, cw_managed_string_view directory,
                                                             cw_managed_string_view defaultName, cw_managed_string_view extensions,
                                                             cw_managed_string_view* result)
        {
            return Execute(context, [&]() { return RunFileDialog(FileDialogType::SaveFile, title, directory, extensions, defaultName, result); });
        }

        cw_managed_status CW_MANAGED_CALL FileDialogSaveFolder(void* context, cw_managed_string_view title, cw_managed_string_view directory,
                                                               cw_managed_string_view defaultName, cw_managed_string_view* result)
        {
            return Execute(context, [&]() { return RunFileDialog(FileDialogType::OpenFolder, title, directory, {}, defaultName, result); });
        }

        cw_managed_status CW_MANAGED_CALL CompressionCompress(void* context, cw_managed_mutable_blob destination, cw_managed_blob source,
                                                              int32_t method, int32_t level, uint64_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (destination.data == nullptr && destination.length != 0) || (source.data == nullptr && source.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Compression::Compress(destination.data, destination.length, source.data, source.length,
                                                static_cast<CompressionMethod>(method), static_cast<FastLZLevel>(level));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CompressionDecompress(void* context, cw_managed_mutable_blob destination, uint64_t maximumDestinationSize,
                                                                cw_managed_blob source, uint64_t sourceSize, int32_t method, uint64_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (destination.data == nullptr && destination.length != 0) || (source.data == nullptr && source.length != 0) ||
                    maximumDestinationSize > destination.length || sourceSize > source.length)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result =
                  Compression::Decompress(destination.data, maximumDestinationSize, source.data, sourceSize, static_cast<CompressionMethod>(method));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL DebugWriteLog(void* context, int32_t severity, cw_managed_string_view message)
        {
            return Execute(context, [&]() {
                if (message.data == nullptr && message.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                ConsoleBuffer::Message::Level level;
                switch (severity)
                {
                case 0:
                    level = ConsoleBuffer::Message::Level::Info;
                    break;
                case 1:
                    level = ConsoleBuffer::Message::Level::Warn;
                    break;
                case 2:
                    level = ConsoleBuffer::Message::Level::Error;
                    break;
                case 3:
                    level = ConsoleBuffer::Message::Level::Critical;
                    break;
                default:
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                }
                ConsoleBuffer::Get().AddMessage(level, Decode(message), {});
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL RandomInitialize(void* context, int32_t seed)
        {
            return Execute(context, [&]() {
                Random::Seed(static_cast<uint32_t>(seed));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL RandomGetValue(void* context, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Random::Float();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL RandomGetRange(void* context, float minimum, float maximum, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = Random::Float(minimum, maximum);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL RandomGetInsideUnitCircle(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Random::InsideUnitCircle();
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL RandomGetInsideUnitSphere(void* context, cw_managed_vec3* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec3 value = Random::InsideUnitSphere();
                *result = { value.x, value.y, value.z };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL NoiseGetPerlin2D(void* context, float x, float y, float* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const NoiseOptions options = { 1, 1.0f, 1.0f, 123, NoiseFunc::Perlin };
                *result = Noise::Noise2D(options, x, y);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL LayerMaskGetName(void* context, int32_t layer, cw_managed_string_view* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::TryGet();
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                static const String empty;
                const String& name = layer >= 0 && layer < 32 ? physics->GetLayerName(layer) : empty;
                return WriteBorrowedStringView(name, result);
            });
        }

        cw_managed_status CW_MANAGED_CALL LayerMaskGetLayer(void* context, cw_managed_string_view name, int32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::TryGet();
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const String decodedName = Decode(name);
                *result = -1;
                for (int32_t layer = 0; layer < 32; ++layer)
                {
                    if (physics->GetLayerName(layer) == decodedName)
                    {
                        *result = layer;
                        break;
                    }
                }
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SceneGetActive(void* context, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                *result = manager != nullptr ? ToAbiUuid(manager->GetActiveSceneId()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SceneGetExecutionState(void* context, int32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                *result = static_cast<int32_t>(manager != nullptr ? manager->GetExecutionState() : SceneExecutionState::Edit);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SceneGetLoadedCount(void* context, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                *result = manager != nullptr ? static_cast<uint32_t>(manager->GetLoadedScenes().size()) : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SceneGetLoaded(void* context, uint32_t index, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const auto scenes = manager->GetLoadedScenes();
                if (index >= scenes.size())
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = ToAbiUuid(scenes[index]);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_SCENE_OPERATION(functionName, expression)                                                                                                 \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid sceneId, int32_t* result)                                          \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            SceneManager* manager = SceneManager::TryGet();                                                                                          \
            if (manager == nullptr)                                                                                                                  \
                return CW_MANAGED_STATUS_NOT_INITIALIZED;                                                                                            \
            const UUID scene = FromAbiUuid(sceneId);                                                                                                 \
            *result = static_cast<int32_t>(expression);                                                                                              \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_SCENE_OPERATION(SceneUnload, manager->UnloadScene(scene))
        CW_SCENE_OPERATION(SceneReload, manager->ReloadScene(scene))
        CW_SCENE_OPERATION(SceneSetActive, manager->SetActiveScene(scene))
#undef CW_SCENE_OPERATION

        cw_managed_status CW_MANAGED_CALL SceneLoad(void* context, cw_managed_uuid sceneId, uint8_t makeActive, int32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                if (manager == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                *result = static_cast<int32_t>(manager->LoadScene(FromAbiUuid(sceneId), makeActive != 0));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_CAMERA_GET(functionName, resultType, expression)                                                                                          \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            CameraComponent* component = ResolveComponent<CameraComponent>(entityId);                                                                \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_CAMERA_SET(functionName, valueType, statement)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            CameraComponent* component = ResolveComponent<CameraComponent>(entityId);                                                                \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_CAMERA_GET(CameraGetFieldOfView, float, glm::degrees(component->Camera.GetPerspectiveVerticalFOV()))
        CW_CAMERA_SET(CameraSetFieldOfView, float, component->Camera.SetPerspectiveVerticalFOV(glm::radians(value)))
        CW_CAMERA_GET(CameraGetProjection, int32_t, static_cast<int32_t>(component->Camera.GetProjectionType()))
        CW_CAMERA_SET(CameraSetProjection, int32_t,
                      component->Camera.SetProjectionType(value == static_cast<int32_t>(SceneCamera::CameraProjection::Perspective)
                                                            ? SceneCamera::CameraProjection::Perspective
                                                            : SceneCamera::CameraProjection::Orthographic))
        CW_CAMERA_GET(CameraGetOrthographicSize, float, component->Camera.GetOrthographicSize())
        CW_CAMERA_SET(CameraSetOrthographicSize, float, component->Camera.SetOrthographicSize(value))
        CW_CAMERA_GET(CameraGetAspectRatio, float, component->Camera.GetAspectRatio())
        CW_CAMERA_SET(CameraSetAspectRatio, float, component->Camera.SetAspectRatio(value))
        CW_CAMERA_GET(CameraGetHdr, uint8_t, component->Camera.GetHDR() ? 1 : 0)
        CW_CAMERA_SET(CameraSetHdr, uint8_t, component->Camera.SetHDR(value != 0))
        CW_CAMERA_GET(CameraGetMsaa, uint8_t, component->Camera.GetMSAA() ? 1 : 0)
        CW_CAMERA_SET(CameraSetMsaa, uint8_t, component->Camera.SetMSAA(value != 0))
        CW_CAMERA_GET(CameraGetOcclusionCulling, uint8_t, component->Camera.GetOcclusionCulling() ? 1 : 0)
        CW_CAMERA_SET(CameraSetOcclusionCulling, uint8_t, component->Camera.SetOcclusionCulling(value != 0))
#undef CW_CAMERA_SET
#undef CW_CAMERA_GET

        cw_managed_status CW_MANAGED_CALL CameraGetPrimary(void* context, cw_managed_uuid* entityId)
        {
            return Execute(context, [&]() {
                if (entityId == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                SceneManager* manager = SceneManager::TryGet();
                const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
                if (scene == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const Entity entity = scene->GetPrimaryCameraEntity();
                *entityId = entity ? ToAbiUuid(entity.GetUuid()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraGetProjectionMatrix(void* context, cw_managed_uuid entityId, cw_managed_mat4* result)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::mat4 value = component->Camera.GetProjection();
                std::memcpy(result->values, glm::value_ptr(value), sizeof(value));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraGetNearClipPlane(void* context, cw_managed_uuid entityId, float* result)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = component->Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective
                            ? component->Camera.GetPerspectiveNearClip()
                            : component->Camera.GetOrthographicNearClip();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraSetNearClipPlane(void* context, cw_managed_uuid entityId, float value)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (component->Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
                    component->Camera.SetPerspectiveNearClip(value);
                else
                    component->Camera.SetOrthographicNearClip(value);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraGetFarClipPlane(void* context, cw_managed_uuid entityId, float* result)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = component->Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective
                            ? component->Camera.GetPerspectiveFarClip()
                            : component->Camera.GetOrthographicFarClip();
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraSetFarClipPlane(void* context, cw_managed_uuid entityId, float value)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (component->Camera.GetProjectionType() == SceneCamera::CameraProjection::Perspective)
                    component->Camera.SetPerspectiveFarClip(value);
                else
                    component->Camera.SetOrthographicFarClip(value);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraGetBackgroundColor(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec3 value = component->Camera.GetBackgroundColor();
                *result = { value.x, value.y, value.z };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraSetBackgroundColor(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                component->Camera.SetBackgroundColor({ value->x, value->y, value->z });
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraGetViewportRectangle(void* context, cw_managed_uuid entityId, cw_managed_vec4* result)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec4 value = component->Camera.GetViewportRect();
                *result = { value.x, value.y, value.z, value.w };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL CameraSetViewportRectangle(void* context, cw_managed_uuid entityId, const cw_managed_vec4* value)
        {
            return Execute(context, [&]() {
                CameraComponent* component = ResolveComponent<CameraComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                component->Camera.SetViewportRect({ value->x, value->y, value->z, value->w });
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_SPRITE_GET(functionName, resultType, expression)                                                                                          \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            SpriteRendererComponent* component = ResolveComponent<SpriteRendererComponent>(entityId);                                                \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_SPRITE_SET(functionName, valueType, statement)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            SpriteRendererComponent* component = ResolveComponent<SpriteRendererComponent>(entityId);                                                \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_SPRITE_GET(SpriteRendererGetTexture, cw_managed_uuid, ToAbiUuid(component->Texture.GetUUID()))
        CW_SPRITE_SET(SpriteRendererSetTexture, cw_managed_uuid, component->Texture = ResolveAsset<Texture>(value))
        CW_SPRITE_GET(SpriteRendererGetSortingLayer, int32_t, component->SortingLayer)
        CW_SPRITE_SET(SpriteRendererSetSortingLayer, int32_t, component->SortingLayer = value)
        CW_SPRITE_GET(SpriteRendererGetOrderInLayer, int32_t, component->OrderInLayer)
        CW_SPRITE_SET(SpriteRendererSetOrderInLayer, int32_t, component->OrderInLayer = value)
#undef CW_SPRITE_SET
#undef CW_SPRITE_GET

        cw_managed_status CW_MANAGED_CALL SpriteRendererGetColor(void* context, cw_managed_uuid entityId, cw_managed_vec4* result)
        {
            return Execute(context, [&]() {
                SpriteRendererComponent* component = ResolveComponent<SpriteRendererComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = { component->Color.r, component->Color.g, component->Color.b, component->Color.a };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL SpriteRendererSetColor(void* context, cw_managed_uuid entityId, const cw_managed_vec4* value)
        {
            return Execute(context, [&]() {
                SpriteRendererComponent* component = ResolveComponent<SpriteRendererComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                component->Color = { value->x, value->y, value->z, value->w };
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_LIGHT_GET(functionName, resultType, expression)                                                                                           \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            LightComponent* component = ResolveComponent<LightComponent>(entityId);                                                                  \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_LIGHT_SET(functionName, valueType, statement)                                                                                             \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            LightComponent* component = ResolveComponent<LightComponent>(entityId);                                                                  \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_LIGHT_GET(LightGetType, int32_t, static_cast<int32_t>(component->Type))
        CW_LIGHT_SET(LightSetType, int32_t, component->Type = static_cast<LightType>(std::clamp(value, 0, 2)))
        CW_LIGHT_GET(LightGetIntensity, float, component->Intensity)
        CW_LIGHT_SET(LightSetIntensity, float, component->Intensity = std::max(value, 0.0f))
        CW_LIGHT_GET(LightGetRange, float, component->Range)
        CW_LIGHT_SET(LightSetRange, float, component->Range = std::max(value, 0.001f))
        CW_LIGHT_GET(LightGetSpotInnerAngle, float, glm::degrees(component->SpotInnerAngle))
        CW_LIGHT_SET(LightSetSpotInnerAngle, float,
                     component->SpotInnerAngle = glm::radians(std::clamp(value, 0.0f, glm::degrees(component->SpotOuterAngle))))
        CW_LIGHT_GET(LightGetSpotOuterAngle, float, glm::degrees(component->SpotOuterAngle))
        CW_LIGHT_SET(LightSetSpotOuterAngle, float,
                     component->SpotOuterAngle = glm::radians(std::clamp(value, glm::degrees(component->SpotInnerAngle), 180.0f)))
        CW_LIGHT_GET(LightGetSourceRadius, float, component->SourceRadius)
        CW_LIGHT_SET(LightSetSourceRadius, float, component->SourceRadius = std::max(value, 0.0f))
        CW_LIGHT_GET(LightGetUseColorTemperature, uint8_t, component->UseColorTemperature ? 1 : 0)
        CW_LIGHT_SET(LightSetUseColorTemperature, uint8_t, component->UseColorTemperature = value != 0)
        CW_LIGHT_GET(LightGetTemperature, float, component->Temperature)
        CW_LIGHT_SET(LightSetTemperature, float, component->Temperature = std::clamp(value, 1000.0f, 40000.0f))
        CW_LIGHT_GET(LightGetVisibilityLayers, uint32_t, component->VisibilityLayers.Value)
        CW_LIGHT_SET(LightSetVisibilityLayers, uint32_t, component->VisibilityLayers.Value = value)
        CW_LIGHT_GET(LightGetEnabled, uint8_t, component->Enabled ? 1 : 0)
        CW_LIGHT_SET(LightSetEnabled, uint8_t, component->Enabled = value != 0)
        CW_LIGHT_GET(LightGetAffectDiffuse, uint8_t, component->AffectDiffuse ? 1 : 0)
        CW_LIGHT_SET(LightSetAffectDiffuse, uint8_t, component->AffectDiffuse = value != 0)
        CW_LIGHT_GET(LightGetAffectSpecular, uint8_t, component->AffectSpecular ? 1 : 0)
        CW_LIGHT_SET(LightSetAffectSpecular, uint8_t, component->AffectSpecular = value != 0)
        CW_LIGHT_GET(LightGetVolumetric, uint8_t, component->Volumetric ? 1 : 0)
        CW_LIGHT_SET(LightSetVolumetric, uint8_t, component->Volumetric = value != 0)
        CW_LIGHT_GET(LightGetShadows, int32_t, static_cast<int32_t>(component->Shadows.Mode))
        CW_LIGHT_SET(LightSetShadows, int32_t, component->Shadows.Mode = static_cast<LightShadowMode>(std::clamp(value, 0, 2)))
        CW_LIGHT_GET(LightGetShadowBias, float, component->Shadows.Bias)
        CW_LIGHT_SET(LightSetShadowBias, float, component->Shadows.Bias = std::max(value, 0.0f))
        CW_LIGHT_GET(LightGetShadowNormalBias, float, component->Shadows.NormalBias)
        CW_LIGHT_SET(LightSetShadowNormalBias, float, component->Shadows.NormalBias = std::max(value, 0.0f))
        CW_LIGHT_GET(LightGetShadowNearPlane, float, component->Shadows.NearPlane)
        CW_LIGHT_SET(LightSetShadowNearPlane, float, component->Shadows.NearPlane = std::max(value, 0.001f))
        CW_LIGHT_GET(LightGetShadowImportance, float, component->Shadows.Importance)
        CW_LIGHT_SET(LightSetShadowImportance, float, component->Shadows.Importance = std::max(value, 0.0f))
        CW_LIGHT_GET(LightGetShadowResolution, uint32_t, component->Shadows.Resolution)
        CW_LIGHT_SET(LightSetShadowResolution, uint32_t, component->Shadows.Resolution = static_cast<uint16_t>(std::clamp(value, 64u, 8192u)))
        CW_LIGHT_GET(LightGetCacheStaticShadowCasters, uint8_t, component->Shadows.CacheStaticCasters ? 1 : 0)
        CW_LIGHT_SET(LightSetCacheStaticShadowCasters, uint8_t, component->Shadows.CacheStaticCasters = value != 0)
#undef CW_LIGHT_SET
#undef CW_LIGHT_GET

        cw_managed_status CW_MANAGED_CALL LightGetColor(void* context, cw_managed_uuid entityId, cw_managed_vec4* result)
        {
            return Execute(context, [&]() {
                LightComponent* component = ResolveComponent<LightComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = { component->Color.r, component->Color.g, component->Color.b, 1.0f };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL LightSetColor(void* context, cw_managed_uuid entityId, const cw_managed_vec4* value)
        {
            return Execute(context, [&]() {
                LightComponent* component = ResolveComponent<LightComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                component->Color = glm::max(glm::vec3(value->x, value->y, value->z), glm::vec3(0.0f));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_TEXT_GET(functionName, resultType, expression)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_TEXT_SET(functionName, valueType, statement)                                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_TEXT_GET(TextGetFont, cw_managed_uuid, ToAbiUuid(component->Font.GetUUID()))
        CW_TEXT_SET(TextSetFont, cw_managed_uuid, component->Font = ResolveAsset<Font>(value))
        CW_TEXT_GET(TextGetSize, float, component->Size)
        CW_TEXT_SET(TextSetSize, float, component->Size = value)
        CW_TEXT_GET(TextGetAutoSize, uint8_t, component->AutoSize ? 1 : 0)
        CW_TEXT_SET(TextSetAutoSize, uint8_t, component->AutoSize = value != 0)
        CW_TEXT_GET(TextGetAutoSizeMin, float, component->AutoSizeMin)
        CW_TEXT_SET(TextSetAutoSizeMin, float, component->AutoSizeMin = std::max(0.0f, value);
                    component->AutoSizeMax = std::max(component->AutoSizeMin, component->AutoSizeMax))
        CW_TEXT_GET(TextGetAutoSizeMax, float, component->AutoSizeMax)
        CW_TEXT_SET(TextSetAutoSizeMax, float, component->AutoSizeMax = std::max(0.0f, value);
                    component->AutoSizeMin = std::min(component->AutoSizeMin, component->AutoSizeMax))
        CW_TEXT_GET(TextGetWrapping, uint8_t, component->Wrapping ? 1 : 0)
        CW_TEXT_SET(TextSetWrapping, uint8_t, component->Wrapping = value != 0)
        CW_TEXT_GET(TextGetWrapMode, int32_t, static_cast<int32_t>(component->WrapMode))
        CW_TEXT_SET(TextSetWrapMode, int32_t, component->WrapMode = static_cast<TextWrapMode>(value))
        CW_TEXT_GET(TextGetOverflow, int32_t, static_cast<int32_t>(component->Overflow))
        CW_TEXT_SET(TextSetOverflow, int32_t, component->Overflow = static_cast<TextOverflow>(value))
        CW_TEXT_GET(TextGetClipToBounds, uint8_t, component->ClipToBounds ? 1 : 0)
        CW_TEXT_SET(TextSetClipToBounds, uint8_t, component->ClipToBounds = value != 0)
        CW_TEXT_GET(TextGetMaxLines, uint32_t, component->MaxLines)
        CW_TEXT_SET(TextSetMaxLines, uint32_t, component->MaxLines = value)
        CW_TEXT_GET(TextGetHorizontalAlignment, int32_t, static_cast<int32_t>(component->HorizontalAlignment))
        CW_TEXT_SET(TextSetHorizontalAlignment, int32_t, component->HorizontalAlignment = static_cast<TextHorizontalAlignment>(value))
        CW_TEXT_GET(TextGetVerticalAlignment, int32_t, static_cast<int32_t>(component->VerticalAlignment))
        CW_TEXT_SET(TextSetVerticalAlignment, int32_t, component->VerticalAlignment = static_cast<TextVerticalAlignment>(value))
        CW_TEXT_GET(TextGetFontStyle, int32_t, static_cast<int32_t>(static_cast<uint32_t>(component->FontStyle)))
        CW_TEXT_SET(TextSetFontStyle, int32_t, component->FontStyle = TextFontStyle(static_cast<uint32_t>(value)))
        CW_TEXT_GET(TextGetOutlineWidth, float, component->Thickness)
        CW_TEXT_SET(TextSetOutlineWidth, float, component->Thickness = std::max(0.0f, value))
        CW_TEXT_GET(TextGetShadowSoftness, float, component->ShadowSoftness)
        CW_TEXT_SET(TextSetShadowSoftness, float, component->ShadowSoftness = std::max(0.0f, value))
        CW_TEXT_GET(TextGetCharacterSpacing, float, component->CharacterSpacing)
        CW_TEXT_SET(TextSetCharacterSpacing, float, component->CharacterSpacing = value)
        CW_TEXT_GET(TextGetWordSpacing, float, component->WordSpacing)
        CW_TEXT_SET(TextSetWordSpacing, float, component->WordSpacing = value)
        CW_TEXT_GET(TextGetLineSpacing, float, component->LineSpacing)
        CW_TEXT_SET(TextSetLineSpacing, float, component->LineSpacing = value)
        CW_TEXT_GET(TextGetParagraphSpacing, float, component->ParagraphSpacing)
        CW_TEXT_SET(TextSetParagraphSpacing, float, component->ParagraphSpacing = value)
        CW_TEXT_GET(TextGetTabWidth, uint32_t, component->TabWidth)
        CW_TEXT_SET(TextSetTabWidth, uint32_t, component->TabWidth = std::max(1u, value))
        CW_TEXT_GET(TextGetUseCustomDecorationColor, uint8_t, component->UseCustomDecorationColor ? 1 : 0)
        CW_TEXT_SET(TextSetUseCustomDecorationColor, uint8_t, component->UseCustomDecorationColor = value != 0)
        CW_TEXT_GET(TextGetDecorationThickness, float, component->DecorationThickness)
        CW_TEXT_SET(TextSetDecorationThickness, float, component->DecorationThickness = std::max(0.0f, value))
        CW_TEXT_GET(TextGetUnderlineOffset, float, component->UnderlineOffset)
        CW_TEXT_SET(TextSetUnderlineOffset, float, component->UnderlineOffset = value)
        CW_TEXT_GET(TextGetStrikethroughOffset, float, component->StrikethroughOffset)
        CW_TEXT_SET(TextSetStrikethroughOffset, float, component->StrikethroughOffset = value)
        CW_TEXT_GET(TextGetUseKerning, uint8_t, component->UseKerning ? 1 : 0)
        CW_TEXT_SET(TextSetUseKerning, uint8_t, component->UseKerning = value != 0)
        CW_TEXT_GET(TextGetSortingLayer, int32_t, component->SortingLayer)
        CW_TEXT_SET(TextSetSortingLayer, int32_t, component->SortingLayer = value)
        CW_TEXT_GET(TextGetOrderInLayer, int32_t, component->OrderInLayer)
        CW_TEXT_SET(TextSetOrderInLayer, int32_t, component->OrderInLayer = value)
#undef CW_TEXT_SET
#undef CW_TEXT_GET

        cw_managed_status CW_MANAGED_CALL TextGetText(void* context, cw_managed_uuid entityId, cw_managed_string_view* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                TextComponent* component = ResolveComponent<TextComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                return WriteBorrowedStringView(component->Text, result);
            });
        }

        cw_managed_status CW_MANAGED_CALL TextSetText(void* context, cw_managed_uuid entityId, cw_managed_string_view value)
        {
            return Execute(context, [&]() {
                TextComponent* component = ResolveComponent<TextComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value.data == nullptr && value.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const StringView text = DecodeView(value);
                if (component->Text != text)
                    component->Text.assign(text);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL TextGetLayoutSize(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                TextComponent* component = ResolveComponent<TextComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = { component->LayoutSize.x, component->LayoutSize.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL TextSetLayoutSize(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)
        {
            return Execute(context, [&]() {
                TextComponent* component = ResolveComponent<TextComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                component->LayoutSize = glm::max(glm::vec2(value->x, value->y), glm::vec2(0.0f));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_TEXT_VEC2(getterName, setterName, memberName)                                                                                             \
    cw_managed_status CW_MANAGED_CALL getterName(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)                                   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = { component->memberName.x, component->memberName.y };                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }                                                                                                                                                \
    cw_managed_status CW_MANAGED_CALL setterName(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)                              \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            component->memberName = { value->x, value->y };                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_TEXT_VEC2(TextGetShadowOffset, TextSetShadowOffset, ShadowOffset)
#undef CW_TEXT_VEC2

#define CW_TEXT_COLOR(getterName, setterName, memberName)                                                                                            \
    cw_managed_status CW_MANAGED_CALL getterName(void* context, cw_managed_uuid entityId, cw_managed_vec4* result)                                   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            *result = { component->memberName.r, component->memberName.g, component->memberName.b, component->memberName.a };                        \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }                                                                                                                                                \
    cw_managed_status CW_MANAGED_CALL setterName(void* context, cw_managed_uuid entityId, const cw_managed_vec4* value)                              \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            TextComponent* component = ResolveComponent<TextComponent>(entityId);                                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            if (value == nullptr)                                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            component->memberName = { value->x, value->y, value->z, value->w };                                                                      \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_TEXT_COLOR(TextGetColor, TextSetColor, Color)
        CW_TEXT_COLOR(TextGetOutlineColor, TextSetOutlineColor, OutlineColor)
        CW_TEXT_COLOR(TextGetShadowColor, TextSetShadowColor, ShadowColor)
        CW_TEXT_COLOR(TextGetDecorationColor, TextSetDecorationColor, DecorationColor)
#undef CW_TEXT_COLOR

        cw_managed_status CW_MANAGED_CALL TextHitTest(void* context, cw_managed_uuid entityId, const cw_managed_vec2* position, uint32_t* result)
        {
            return Execute(context, [&]() {
                TextComponent* component = ResolveComponent<TextComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (position == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;

                AssetHandle<Font> font = component->Font;
                if (!font)
                    font = FontManager::GetDefaultFont();
                *result = 0;
                if (font && font->IsValid())
                {
                    const TextHitTestResult hit = TextLayout::HitTest(*component, *font, glm::vec2(position->x, position->y));
                    if (hit.Valid)
                        *result = static_cast<uint32_t>(std::min(hit.SourceByteOffset, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
                }
                return CW_MANAGED_STATUS_OK;
            });
        }

        struct ManagedRaycastHit2D
        {
            cw_managed_vec2 Point;
            cw_managed_vec2 Normal;
            float Fraction;
            uint32_t EntityId;
        };

        struct ManagedRaycastHit3D
        {
            cw_managed_vec3 Point;
            cw_managed_vec3 Normal;
            float Distance;
            float Fraction;
            uint64_t BodyHandle;
            uint64_t ShapeHandle;
            uint64_t EntityId;
        };

        static_assert(sizeof(ManagedRaycastHit2D) == 24);
        static_assert(sizeof(ManagedRaycastHit3D) == 56);

        Entity ResolveRuntimeEntity(uint64_t runtimeId)
        {
            if (runtimeId > std::numeric_limits<uint32_t>::max())
                return {};
            SceneManager* manager = SceneManager::TryGet();
            const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
            return scene ? Entity(static_cast<entt::entity>(static_cast<uint32_t>(runtimeId)), scene.get()) : Entity();
        }

#define CW_PHYSICS_2D_GET(functionName, resultType, fallback, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, resultType* result)                                                                \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                                           \
            *result = physics != nullptr ? (expression) : (fallback);                                                                                \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_PHYSICS_2D_GET(Physics2DGetBackend, int32_t, 0, static_cast<int32_t>(physics->GetBackend()))
        CW_PHYSICS_2D_GET(Physics2DGetIsSimulating, uint8_t, 0, physics->IsSimulating() ? 1 : 0)
        CW_PHYSICS_2D_GET(Physics2DGetVelocityIterations, uint32_t, 0u, physics->GetVelocityIterations())
        CW_PHYSICS_2D_GET(Physics2DGetPositionIterations, uint32_t, 0u, physics->GetPositionIterations())
        CW_PHYSICS_2D_GET(Physics2DGetDefaultMaterial, cw_managed_uuid, cw_managed_uuid{}, ToAbiUuid(physics->GetDefaultMaterial().GetUUID()))
#undef CW_PHYSICS_2D_GET

        cw_managed_status CW_MANAGED_CALL Physics2DGetGravity(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                const glm::vec2 value = physics != nullptr ? physics->GetGravity() : glm::vec2(0.0f);
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DSetGravity(void* context, const cw_managed_vec2* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->SetGravity({ value->x, value->y });
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_PHYSICS_2D_SET(functionName, valueType, statement)                                                                                        \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, valueType value)                                                                   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                                           \
            if (physics == nullptr)                                                                                                                  \
                return CW_MANAGED_STATUS_NOT_INITIALIZED;                                                                                            \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_PHYSICS_2D_SET(Physics2DSetVelocityIterations, uint32_t, physics->SetVelocityIterations(value))
        CW_PHYSICS_2D_SET(Physics2DSetPositionIterations, uint32_t, physics->SetPositionIterations(value))
        CW_PHYSICS_2D_SET(Physics2DSetDefaultMaterial, cw_managed_uuid, physics->SetDefaultMaterial(ResolveAsset<PhysicsMaterial2D>(value)))
#undef CW_PHYSICS_2D_SET

        cw_managed_status CW_MANAGED_CALL Physics2DGetLayerName(void* context, int32_t layer, cw_managed_string_view* result)
        {
            return Execute(context, [&]() -> cw_managed_status {
                if (result == nullptr || layer < 0 || layer >= static_cast<int32_t>(Physics2DLayerCount))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                return WriteBorrowedStringView(physics->GetLayerName(static_cast<uint32_t>(layer)), result);
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DSetLayerName(void* context, int32_t layer, cw_managed_string_view name)
        {
            return Execute(context, [&]() {
                if (layer < 0 || layer >= static_cast<int32_t>(Physics2DLayerCount) || (name.data == nullptr && name.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->SetLayerName(static_cast<uint32_t>(layer), Decode(name));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DGetLayerMask(void* context, int32_t layer, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || layer < 0 || layer >= static_cast<int32_t>(Physics2DLayerCount))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                *result = physics->GetCategoryMask(static_cast<uint32_t>(layer));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DSetLayerMask(void* context, int32_t layer, uint32_t mask)
        {
            return Execute(context, [&]() {
                if (layer < 0 || layer >= static_cast<int32_t>(Physics2DLayerCount))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->SetCategoryMask(static_cast<uint32_t>(layer), mask);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DResolveEntity(void* context, uint32_t runtimeId, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveRuntimeEntity(runtimeId);
                *result = entity ? ToAbiUuid(entity.GetUuid()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics2DRaycast(void* context, const cw_managed_vec2* origin, const cw_managed_vec2* direction,
                                                           float distance, uint32_t layerMask, void* destination, uint32_t capacity, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (origin == nullptr || direction == nullptr || result == nullptr || (destination == nullptr && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const Vector<PhysicsRaycastHit2D> hits =
                  physics->Raycast({ origin->x, origin->y }, { direction->x, direction->y }, distance, layerMask);
                auto* output = static_cast<ManagedRaycastHit2D*>(destination);
                const uint32_t count = std::min(capacity, static_cast<uint32_t>(hits.size()));
                for (uint32_t index = 0; index < count; ++index)
                {
                    output[index] = { { hits[index].Point.x, hits[index].Point.y },
                                      { hits[index].Normal.x, hits[index].Normal.y },
                                      hits[index].Fraction,
                                      static_cast<uint32_t>(hits[index].HitEntity.GetHandle()) };
                }
                *result = static_cast<uint32_t>(hits.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        bool IsValidPhysics3DBackend(int32_t value)
        {
            return value >= static_cast<int32_t>(Physics3DBackendType::Box3D) && value <= static_cast<int32_t>(Physics3DBackendType::Bullet);
        }

        bool MakePhysics3DQueryShape(int32_t shapeType, const cw_managed_vec3& size, float radius, float height, PhysicsShape3DDesc& result)
        {
            constexpr float minimumExtent = 0.0001f;
            switch (shapeType)
            {
            case static_cast<int32_t>(PhysicsShapeType3D::Box):
                result.Type = PhysicsShapeType3D::Box;
                result.HalfExtents = glm::max(glm::abs(glm::vec3(size.x, size.y, size.z)) * 0.5f, glm::vec3(minimumExtent));
                return true;
            case static_cast<int32_t>(PhysicsShapeType3D::Sphere):
                result.Type = PhysicsShapeType3D::Sphere;
                result.Radius = std::max(std::abs(radius), minimumExtent);
                return true;
            case static_cast<int32_t>(PhysicsShapeType3D::Capsule):
                result.Type = PhysicsShapeType3D::Capsule;
                result.Radius = std::max(std::abs(radius), minimumExtent);
                result.Height = std::max(std::abs(height), minimumExtent);
                return true;
            default:
                return false;
            }
        }

        uint32_t WritePhysics3DHits(const Vector<PhysicsQueryHit3D>& hits, void* destination, uint32_t capacity)
        {
            auto* output = static_cast<ManagedRaycastHit3D*>(destination);
            const uint32_t count = std::min(capacity, static_cast<uint32_t>(hits.size()));
            for (uint32_t index = 0; index < count; ++index)
            {
                const PhysicsQueryHit3D& hit = hits[index];
                output[index] = { { hit.Point.x, hit.Point.y, hit.Point.z },
                                  { hit.Normal.x, hit.Normal.y, hit.Normal.z },
                                  hit.Distance,
                                  hit.Fraction,
                                  hit.Body.Value,
                                  hit.Shape.Value,
                                  hit.UserData };
            }
            return static_cast<uint32_t>(hits.size());
        }

#define CW_PHYSICS_3D_GET(functionName, resultType, fallback, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, resultType* result)                                                                \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;                                                           \
            *result = physics != nullptr ? (expression) : (fallback);                                                                                \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_PHYSICS_3D_GET(Physics3DGetBackend, int32_t, 0, static_cast<int32_t>(physics->GetBackend()))
        CW_PHYSICS_3D_GET(Physics3DGetIsSimulating, uint8_t, 0, physics->IsSimulating() ? 1 : 0)
        CW_PHYSICS_3D_GET(Physics3DGetCapabilities, uint64_t, 0ull, static_cast<uint64_t>(physics->GetCapabilities()))
        CW_PHYSICS_3D_GET(Physics3DGetSubsteps, uint32_t, 1u, physics->GetSettings().Substeps)
        CW_PHYSICS_3D_GET(Physics3DGetDefaultMaterial, cw_managed_uuid, cw_managed_uuid{}, ToAbiUuid(physics->GetDefaultMaterial().GetUUID()))
#undef CW_PHYSICS_3D_GET

        cw_managed_status CW_MANAGED_CALL Physics3DGetBackendName(void* context, cw_managed_string_view* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                thread_local String storage;
                storage = physics != nullptr ? String(physics->GetBackendName()) : String();
                *result = { reinterpret_cast<const uint8_t*>(storage.data()), static_cast<uint32_t>(storage.size()) };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DGetGravity(void* context, cw_managed_vec3* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                const glm::vec3 value = physics != nullptr ? physics->GetSettings().Gravity : glm::vec3(0.0f);
                *result = { value.x, value.y, value.z };
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DSetGravity(void* context, const cw_managed_vec3* value)
        {
            return Execute(context, [&]() {
                if (value == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->SetGravity({ value->x, value->y, value->z });
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DSetSubsteps(void* context, uint32_t value)
        {
            return Execute(context, [&]() {
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                Physics3DSettings settings = physics->GetSettings();
                settings.Substeps = std::max(value, 1u);
                physics->SetSettings(settings);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DSetDefaultMaterial(void* context, cw_managed_uuid materialId)
        {
            return Execute(context, [&]() {
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->SetDefaultMaterial(ResolveAsset<PhysicsMaterial3D>(materialId));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DTrySetBackend(void* context, int32_t value, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr || physics->IsSimulating() || !IsValidPhysics3DBackend(value))
                {
                    *result = 0;
                    return CW_MANAGED_STATUS_OK;
                }
                const auto backend = static_cast<Physics3DBackendType>(value);
                *result = Physics3D::IsBackendCompiled(backend) && physics->SetBackend(backend) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DIsBackendAvailable(void* context, int32_t value, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = IsValidPhysics3DBackend(value) && Physics3D::IsBackendCompiled(static_cast<Physics3DBackendType>(value)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DResolveEntity(void* context, uint64_t runtimeId, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveRuntimeEntity(runtimeId);
                *result = entity ? ToAbiUuid(entity.GetUuid()) : cw_managed_uuid{};
                return CW_MANAGED_STATUS_OK;
            });
        }

        PhysicsQueryFilter3D MakePhysics3DQueryFilter(uint32_t layerMask, uint8_t includeTriggers, uint64_t ignoreBodyHandle)
        {
            PhysicsQueryFilter3D filter;
            filter.LayerMask = layerMask;
            filter.IncludeTriggers = includeTriggers != 0;
            filter.IgnoreBody = PhysicsBody3DHandle{ ignoreBodyHandle };
            return filter;
        }

        cw_managed_status CW_MANAGED_CALL Physics3DRaycast(void* context, const cw_managed_vec3* origin, const cw_managed_vec3* direction,
                                                           float distance, uint32_t layerMask, uint8_t includeTriggers, uint64_t ignoreBodyHandle,
                                                           void* destination, uint32_t capacity, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (origin == nullptr || direction == nullptr || result == nullptr || (destination == nullptr && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                const Vector<PhysicsQueryHit3D> hits =
                  physics->Raycast({ origin->x, origin->y, origin->z }, { direction->x, direction->y, direction->z }, distance,
                                   MakePhysics3DQueryFilter(layerMask, includeTriggers, ignoreBodyHandle));
                *result = WritePhysics3DHits(hits, destination, capacity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DSweep(void* context, int32_t shapeType, const cw_managed_vec3* size, float radius, float height,
                                                         const cw_managed_vec3* position, const cw_managed_quat* rotation,
                                                         const cw_managed_vec3* direction, float distance, uint32_t layerMask,
                                                         uint8_t includeTriggers, uint64_t ignoreBodyHandle, void* destination, uint32_t capacity,
                                                         uint32_t* result)
        {
            return Execute(context, [&]() {
                if (size == nullptr || position == nullptr || rotation == nullptr || direction == nullptr || result == nullptr ||
                    (destination == nullptr && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                PhysicsShape3DDesc shape;
                if (!MakePhysics3DQueryShape(shapeType, *size, radius, height, shape))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Vector<PhysicsQueryHit3D> hits = physics->Sweep(
                  shape, { position->x, position->y, position->z }, glm::quat(rotation->w, rotation->x, rotation->y, rotation->z),
                  { direction->x, direction->y, direction->z }, distance, MakePhysics3DQueryFilter(layerMask, includeTriggers, ignoreBodyHandle));
                *result = WritePhysics3DHits(hits, destination, capacity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Physics3DOverlap(void* context, int32_t shapeType, const cw_managed_vec3* size, float radius, float height,
                                                           const cw_managed_vec3* position, const cw_managed_quat* rotation, uint32_t layerMask,
                                                           uint8_t includeTriggers, uint64_t ignoreBodyHandle, void* destination, uint32_t capacity,
                                                           uint32_t* result)
        {
            return Execute(context, [&]() {
                if (size == nullptr || position == nullptr || rotation == nullptr || result == nullptr || (destination == nullptr && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                Physics3D* physics = Physics3D::IsStartedUp() ? Physics3D::TryGet() : nullptr;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                PhysicsShape3DDesc shape;
                if (!MakePhysics3DQueryShape(shapeType, *size, radius, height, shape))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Vector<PhysicsQueryHit3D> hits =
                  physics->Overlap(shape, { position->x, position->y, position->z }, glm::quat(rotation->w, rotation->x, rotation->y, rotation->z),
                                   MakePhysics3DQueryFilter(layerMask, includeTriggers, ignoreBodyHandle));
                *result = WritePhysics3DHits(hits, destination, capacity);
                return CW_MANAGED_STATUS_OK;
            });
        }

        enum class ManagedVertexAttributeFormat : int32_t
        {
            Float32 = 1,
            Float16 = 2,
            UNorm8 = 3,
            SNorm8 = 4,
            UInt8 = 5,
            SInt8 = 6,
            UInt16 = 7,
            SInt16 = 8,
            UInt32 = 9,
            SInt32 = 10
        };

        struct ManagedVertexAttributeDescriptor
        {
            int32_t Attribute;
            ManagedVertexAttributeFormat Format;
            int32_t Dimension;
            int32_t Stream;
        };

        static_assert(sizeof(ManagedVertexAttributeDescriptor) == 16);
        static_assert(sizeof(glm::vec2) == 8);
        static_assert(sizeof(glm::vec3) == 12);
        static_assert(sizeof(glm::vec4) == 16);

        ShaderDataType ToShaderDataType(ManagedVertexAttributeFormat format, int32_t dimension)
        {
            if (format == ManagedVertexAttributeFormat::Float32)
            {
                switch (dimension)
                {
                case 1:
                    return ShaderDataType::Float;
                case 2:
                    return ShaderDataType::Float2;
                case 3:
                    return ShaderDataType::Float3;
                case 4:
                    return ShaderDataType::Float4;
                default:
                    break;
                }
            }
            else if (format == ManagedVertexAttributeFormat::SInt32 || format == ManagedVertexAttributeFormat::UInt32)
            {
                switch (dimension)
                {
                case 1:
                    return ShaderDataType::Int;
                case 2:
                    return ShaderDataType::Int2;
                case 3:
                    return ShaderDataType::Int3;
                case 4:
                    return ShaderDataType::Int4;
                default:
                    break;
                }
            }
            else if ((format == ManagedVertexAttributeFormat::UNorm8 || format == ManagedVertexAttributeFormat::UInt8) && dimension == 4)
                return ShaderDataType::UByte4;
            else if (format == ManagedVertexAttributeFormat::SNorm8 || format == ManagedVertexAttributeFormat::SInt8)
            {
                switch (dimension)
                {
                case 1:
                    return ShaderDataType::SByte;
                case 2:
                    return ShaderDataType::SByte2;
                case 3:
                    return ShaderDataType::SByte3;
                case 4:
                    return ShaderDataType::SByte4;
                default:
                    break;
                }
            }
            return ShaderDataType::None;
        }

        void FromShaderDataType(ShaderDataType type, ManagedVertexAttributeFormat& format, int32_t& dimension)
        {
            switch (type)
            {
            case ShaderDataType::Float:
                format = ManagedVertexAttributeFormat::Float32;
                dimension = 1;
                return;
            case ShaderDataType::Float2:
                format = ManagedVertexAttributeFormat::Float32;
                dimension = 2;
                return;
            case ShaderDataType::Float3:
                format = ManagedVertexAttributeFormat::Float32;
                dimension = 3;
                return;
            case ShaderDataType::Float4:
                format = ManagedVertexAttributeFormat::Float32;
                dimension = 4;
                return;
            case ShaderDataType::Int:
                format = ManagedVertexAttributeFormat::SInt32;
                dimension = 1;
                return;
            case ShaderDataType::Int2:
                format = ManagedVertexAttributeFormat::SInt32;
                dimension = 2;
                return;
            case ShaderDataType::Int3:
                format = ManagedVertexAttributeFormat::SInt32;
                dimension = 3;
                return;
            case ShaderDataType::Int4:
                format = ManagedVertexAttributeFormat::SInt32;
                dimension = 4;
                return;
            case ShaderDataType::UByte4:
                format = ManagedVertexAttributeFormat::UNorm8;
                dimension = 4;
                return;
            case ShaderDataType::SByte:
                format = ManagedVertexAttributeFormat::SNorm8;
                dimension = 1;
                return;
            case ShaderDataType::SByte2:
                format = ManagedVertexAttributeFormat::SNorm8;
                dimension = 2;
                return;
            case ShaderDataType::SByte3:
                format = ManagedVertexAttributeFormat::SNorm8;
                dimension = 3;
                return;
            case ShaderDataType::SByte4:
                format = ManagedVertexAttributeFormat::SNorm8;
                dimension = 4;
                return;
            case ShaderDataType::Color:
                format = ManagedVertexAttributeFormat::UNorm8;
                dimension = 4;
                return;
            default:
                format = ManagedVertexAttributeFormat::Float32;
                dimension = 0;
                return;
            }
        }

#define CW_MESH_COUNT(functionName, expression)                                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, uint32_t* result)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);                                                                              \
            if (!mesh.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_COUNT(MeshGetVertexCount, mesh->GetVertexCount())
        CW_MESH_COUNT(MeshGetIndexCount, mesh->GetIndexCount())
        CW_MESH_COUNT(MeshGetVertexStride, mesh->GetMeshData() ? mesh->GetMeshData()->GetBufferLayout().GetStride() : 0u)
        CW_MESH_COUNT(MeshGetVertexAttributeCount,
                      mesh->GetMeshData() ? static_cast<uint32_t>(mesh->GetMeshData()->GetBufferLayout().GetElements().size()) : 0u)
#undef CW_MESH_COUNT

#define CW_MESH_COPY(functionName, elementType, expression)                                                                                          \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, void* destination, uint32_t capacity, uint32_t* result)   \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr || (destination == nullptr && capacity != 0))                                                                      \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);                                                                              \
            if (!mesh.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            const Ref<MeshData> data = mesh->GetMeshData();                                                                                          \
            const Vector<elementType> values = data ? expression : Vector<elementType>();                                                            \
            const uint32_t count = std::min(capacity, static_cast<uint32_t>(values.size()));                                                         \
            if (count != 0)                                                                                                                          \
                std::memcpy(destination, values.data(), static_cast<size_t>(count) * sizeof(elementType));                                           \
            *result = static_cast<uint32_t>(values.size());                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_COPY(MeshCopyVertices, glm::vec3, data->GetPositions())
        CW_MESH_COPY(MeshCopyNormals, glm::vec3, data->GetNormals())
        CW_MESH_COPY(MeshCopyColors, glm::vec4, data->GetColors())
        CW_MESH_COPY(MeshCopyIndices, uint32_t, data->GetIndices())
#undef CW_MESH_COPY

#define CW_MESH_SET(functionName, elementType, statement)                                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, void* source, uint32_t count)                             \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (source == nullptr && count != 0)                                                                                                     \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);                                                                              \
            if (!mesh.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            const Ref<MeshData> data = mesh->GetMeshData();                                                                                          \
            if (!data)                                                                                                                               \
                return CW_MANAGED_STATUS_NOT_INITIALIZED;                                                                                            \
            Vector<elementType> values(count);                                                                                                       \
            if (count != 0)                                                                                                                          \
                std::memcpy(values.data(), source, static_cast<size_t>(count) * sizeof(elementType));                                                \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_SET(MeshSetVertices, glm::vec3, data->SetPositions(values))
        CW_MESH_SET(MeshSetNormals, glm::vec3, data->SetNormals(values))
        CW_MESH_SET(MeshSetColors, glm::vec4, data->SetColors(values))
        CW_MESH_SET(MeshSetIndices, uint32_t, data->SetIndices(values))
#undef CW_MESH_SET

        cw_managed_status CW_MANAGED_CALL MeshCopyUvs(void* context, cw_managed_uuid assetId, uint32_t channel, void* destination, uint32_t capacity,
                                                      uint32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (destination == nullptr && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                const Vector<glm::vec2> values = data ? data->GetUVs(channel) : Vector<glm::vec2>();
                const uint32_t count = std::min(capacity, static_cast<uint32_t>(values.size()));
                if (count != 0)
                    std::memcpy(destination, values.data(), static_cast<size_t>(count) * sizeof(glm::vec2));
                *result = static_cast<uint32_t>(values.size());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshSetUvs(void* context, cw_managed_uuid assetId, uint32_t channel, void* source, uint32_t count)
        {
            return Execute(context, [&]() {
                if (source == nullptr && count != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                if (!data)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                Vector<glm::vec2> values(count);
                if (count != 0)
                    std::memcpy(values.data(), source, static_cast<size_t>(count) * sizeof(glm::vec2));
                data->SetUVs(channel, values);
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_MESH_ACTION(functionName, statement)                                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId)                                                           \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);                                                                              \
            if (!mesh.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_ACTION(MeshRecalculateBounds, mesh->RecalculateBounds())
        CW_MESH_ACTION(MeshRecalculateNormals, mesh->RecalculateNormals())
        CW_MESH_ACTION(MeshRecalculateTangents, mesh->RecalculateTangents())
        CW_MESH_ACTION(MeshUploadData, mesh->UploadToGpu())
        CW_MESH_ACTION(MeshClear, if (const Ref<MeshData> data = mesh->GetMeshData()) data->AllocateBuffer())
#undef CW_MESH_ACTION

#define CW_MESH_BOUNDS(functionName, expression)                                                                                                     \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid assetId, cw_managed_vec3* result)                                  \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);                                                                              \
            if (!mesh.IsLoaded())                                                                                                                    \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            const glm::vec3 value = expression;                                                                                                      \
            *result = { value.x, value.y, value.z };                                                                                                 \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_BOUNDS(MeshGetBoundsMin, mesh->GetBounds().GetMin())
        CW_MESH_BOUNDS(MeshGetBoundsMax, mesh->GetBounds().GetMax())
#undef CW_MESH_BOUNDS

        cw_managed_status CW_MANAGED_CALL MeshSetVertexBufferParams(void* context, cw_managed_uuid assetId, uint32_t vertexCount, void* layout,
                                                                    uint32_t layoutCount)
        {
            return Execute(context, [&]() {
                if (layout == nullptr && layoutCount != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const auto* descriptors = static_cast<const ManagedVertexAttributeDescriptor*>(layout);
                BufferLayout bufferLayout;
                for (uint32_t index = 0; index < layoutCount; ++index)
                {
                    const ShaderDataType dataType = ToShaderDataType(descriptors[index].Format, descriptors[index].Dimension);
                    if (dataType == ShaderDataType::None || descriptors[index].Attribute < 0 || descriptors[index].Stream < 0)
                        return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                    BufferElement element(dataType, static_cast<VertexAttribute>(descriptors[index].Attribute));
                    element.StreamIdx = static_cast<uint32_t>(descriptors[index].Stream);
                    bufferLayout.AddBufferElement(element);
                }
                mesh->SetMeshData(MeshData::Create(vertexCount, 0, bufferLayout));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshSetVertexBufferData(void* context, cw_managed_uuid assetId, void* source, uint32_t meshBufferStart,
                                                                  uint32_t count, uint32_t stride)
        {
            return Execute(context, [&]() {
                if ((source == nullptr && count != 0) || (stride == 0 && count != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                if (!data)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                if (meshBufferStart > data->GetVertexCount() || count > data->GetVertexCount() - meshBufferStart)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const uint32_t meshStride = data->GetBufferLayout().GetStride();
                uint8_t* destination = data->GetVertexBufferData() + static_cast<size_t>(meshBufferStart) * meshStride;
                if (stride == meshStride)
                    std::memcpy(destination, source, static_cast<size_t>(count) * stride);
                else
                {
                    const uint32_t copySize = std::min(stride, meshStride);
                    const auto* bytes = static_cast<const uint8_t*>(source);
                    for (uint32_t index = 0; index < count; ++index)
                        std::memcpy(destination + static_cast<size_t>(index) * meshStride, bytes + static_cast<size_t>(index) * stride, copySize);
                }
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshGetVertexBufferData(void* context, cw_managed_uuid assetId, void* destination, uint32_t capacity,
                                                                  uint32_t stride, uint32_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (destination == nullptr && capacity != 0) || (stride == 0 && capacity != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                if (!data)
                {
                    *result = 0;
                    return CW_MANAGED_STATUS_OK;
                }
                const uint32_t count = std::min(capacity, data->GetVertexCount());
                const uint32_t meshStride = data->GetBufferLayout().GetStride();
                const uint8_t* source = data->GetVertexBufferData();
                if (stride == meshStride)
                    std::memcpy(destination, source, static_cast<size_t>(count) * stride);
                else
                {
                    const uint32_t copySize = std::min(stride, meshStride);
                    auto* bytes = static_cast<uint8_t*>(destination);
                    for (uint32_t index = 0; index < count; ++index)
                        std::memcpy(bytes + static_cast<size_t>(index) * stride, source + static_cast<size_t>(index) * meshStride, copySize);
                }
                *result = count;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshHasVertexAttribute(void* context, cw_managed_uuid assetId, int32_t attribute, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                *result = data && data->GetBufferLayout().HasAttribute(static_cast<VertexAttribute>(attribute)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshGetVertexAttribute(void* context, cw_managed_uuid assetId, int32_t index, void* destination)
        {
            return Execute(context, [&]() {
                if (destination == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const AssetHandle<Mesh> mesh = ResolveAsset<Mesh>(assetId);
                if (!mesh.IsLoaded())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                const Ref<MeshData> data = mesh->GetMeshData();
                if (!data || index < 0 || index >= static_cast<int32_t>(data->GetBufferLayout().GetElements().size()))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const BufferElement& element = data->GetBufferLayout().GetElements()[static_cast<size_t>(index)];
                auto& descriptor = *static_cast<ManagedVertexAttributeDescriptor*>(destination);
                descriptor.Attribute = static_cast<int32_t>(element.Attribute);
                descriptor.Stream = static_cast<int32_t>(element.StreamIdx);
                FromShaderDataType(element.Type, descriptor.Format, descriptor.Dimension);
                return descriptor.Dimension == 0 ? CW_MANAGED_STATUS_INVALID_ARGUMENT : CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status StoreCreatedMesh(void* context, const Ref<Mesh>& mesh, cw_managed_uuid* result)
        {
            if (result == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            AssetManager* manager = AssetManager::TryGet();
            if (!mesh || manager == nullptr)
                return CW_MANAGED_STATUS_NOT_INITIALIZED;
            const AssetHandle<Mesh> handle = static_asset_cast<Mesh>(manager->CreateAssetHandle(mesh));
            if (!ManagedAssetLeaseRegistry::Acquire(context, handle))
                return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
            *result = ToAbiUuid(handle.GetUUID());
            return CW_MANAGED_STATUS_OK;
        }

#define CW_MESH_CREATE(functionName, arguments, expression)                                                                                          \
    cw_managed_status CW_MANAGED_CALL functionName arguments                                                                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() { return StoreCreatedMesh(context, expression, result); });                                                    \
    }
        CW_MESH_CREATE(MeshCreatePlane,
                       (void* context, float width, float height, uint32_t subdivisionsX, uint32_t subdivisionsY, cw_managed_uuid* result),
                       MeshFactory::CreatePlane(width, height, glm::vec3(0.0f, 1.0f, 0.0f), subdivisionsX, subdivisionsY, MeshUsage::CpuCached))
        CW_MESH_CREATE(MeshCreateBox, (void* context, const cw_managed_vec3* dimensions, cw_managed_uuid* result),
                       dimensions ? MeshFactory::CreateBox(glm::vec3(dimensions->x, dimensions->y, dimensions->z), MeshUsage::CpuCached)
                                  : Ref<Mesh>())
        CW_MESH_CREATE(MeshCreateCube, (void* context, float size, cw_managed_uuid* result), MeshFactory::CreateCube(size, MeshUsage::CpuCached))
        CW_MESH_CREATE(MeshCreateSphere, (void* context, float radius, uint32_t segments, uint32_t rings, cw_managed_uuid* result),
                       MeshFactory::CreateSphere(radius, segments, rings, MeshUsage::CpuCached))
        CW_MESH_CREATE(MeshCreateCylinder, (void* context, float radius, float height, uint32_t segments, uint8_t capped, cw_managed_uuid* result),
                       MeshFactory::CreateCylinder(radius, height, segments, capped != 0, MeshUsage::CpuCached))
        CW_MESH_CREATE(MeshCreateCone, (void* context, float radius, float height, uint32_t segments, uint8_t capped, cw_managed_uuid* result),
                       MeshFactory::CreateCone(radius, height, segments, capped != 0, MeshUsage::CpuCached))
        CW_MESH_CREATE(MeshCreateCapsule,
                       (void* context, float radius, float height, uint32_t segments, uint32_t hemisphereRings, cw_managed_uuid* result),
                       MeshFactory::CreateCapsule(radius, height, segments, hemisphereRings, MeshUsage::CpuCached))
#undef CW_MESH_CREATE

#define CW_MESH_RENDERER_GET(functionName, resultType, expression)                                                                                   \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                                      \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (result == nullptr)                                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            MeshRendererComponent* component = ResolveComponent<MeshRendererComponent>(entityId);                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            *result = expression;                                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
#define CW_MESH_RENDERER_SET(functionName, valueType, statement)                                                                                     \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                                         \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            MeshRendererComponent* component = ResolveComponent<MeshRendererComponent>(entityId);                                                    \
            if (component == nullptr)                                                                                                                \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                                               \
            statement;                                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MESH_RENDERER_GET(MeshRendererGetMesh, cw_managed_uuid, ToAbiUuid(component->MeshHandle.GetUUID()))
        CW_MESH_RENDERER_SET(MeshRendererSetMesh, cw_managed_uuid, component->MeshHandle = ResolveAsset<Mesh>(value))
        CW_MESH_RENDERER_GET(MeshRendererGetMaterialCount, uint32_t, component->GetMaterialCount())
        CW_MESH_RENDERER_SET(MeshRendererSetMaterialCount, uint32_t, component->Materials.resize(value))
#undef CW_MESH_RENDERER_SET
#undef CW_MESH_RENDERER_GET

        cw_managed_status CW_MANAGED_CALL MeshRendererGetMaterial(void* context, cw_managed_uuid entityId, uint32_t index, cw_managed_uuid* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                MeshRendererComponent* component = ResolveComponent<MeshRendererComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = ToAbiUuid(component->GetMaterial(index).GetUUID());
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL MeshRendererSetMaterial(void* context, cw_managed_uuid entityId, uint32_t index, cw_managed_uuid materialId)
        {
            return Execute(context, [&]() {
                MeshRendererComponent* component = ResolveComponent<MeshRendererComponent>(entityId);
                if (component == nullptr)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                component->SetMaterial(index, ResolveAsset<Material>(materialId));
                return CW_MANAGED_STATUS_OK;
            });
        }

        glm::mat4 ToMatrix(const cw_managed_mat4& value)
        {
            glm::mat4 result(1.0f);
            std::memcpy(glm::value_ptr(result), value.values, sizeof(result));
            return result;
        }

        void WriteMatrix(const glm::mat4& value, cw_managed_mat4& result) { std::memcpy(result.values, glm::value_ptr(value), sizeof(value)); }

        cw_managed_status CW_MANAGED_CALL MathMatrixDeterminant(void* context, const cw_managed_mat4* matrix, float* result)
        {
            return Execute(context, [&]() {
                if (matrix == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = glm::determinant(ToMatrix(*matrix));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_MATRIX_OPERATION(functionName, expression)                                                                                                \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, const cw_managed_mat4* matrix, cw_managed_mat4* result)                            \
    {                                                                                                                                                \
        return Execute(context, [&]() {                                                                                                              \
            if (matrix == nullptr || result == nullptr)                                                                                              \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                                           \
            WriteMatrix(expression, *result);                                                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                                             \
        });                                                                                                                                          \
    }
        CW_MATRIX_OPERATION(MathMatrixInverse, glm::inverse(ToMatrix(*matrix)))
        CW_MATRIX_OPERATION(MathMatrixAffineInverse, glm::affineInverse(ToMatrix(*matrix)))
#undef CW_MATRIX_OPERATION

        cw_managed_status CW_MANAGED_CALL MathLookAt(void* context, const cw_managed_vec3* from, const cw_managed_vec3* to, const cw_managed_vec3* up,
                                                     cw_managed_mat4* result)
        {
            return Execute(context, [&]() {
                if (from == nullptr || to == nullptr || up == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                WriteMatrix(glm::lookAt(glm::vec3(from->x, from->y, from->z), glm::vec3(to->x, to->y, to->z), glm::vec3(up->x, up->y, up->z)),
                            *result);
                return CW_MANAGED_STATUS_OK;
            });
        }
    } // namespace

    void PopulateManagedHostBindings(cw_managed_host_api& api)
    {
#define CW_ASSIGN_HOST_FUNCTION(functionName, fieldName) api.fieldName = &functionName;
        CW_MANAGED_HOST_FUNCTION_LIST(CW_ASSIGN_HOST_FUNCTION)
#undef CW_ASSIGN_HOST_FUNCTION
    }

    void ReleaseManagedHostBindings(void* context) { ManagedAssetLeaseRegistry::ReleaseAll(context); }
} // namespace Crowny
