#include "cwepch.h"

#include "Panels/ComponentEditor.h"
#include "Panels/InspectorPropertyAccessor.h"

#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include "Editor/EditorAssets.h"
#include "UI/Properties.h"

#include <glm/gtx/euler_angles.hpp>

namespace Crowny
{
    namespace
    {
        template <typename Value, typename Getter> bool HasMixedValue(const Vector<Entity>& entities, const Value& primaryValue, Getter&& getter)
        {
            return InspectorPropertyAccessor(entities, "").IsMixed(primaryValue, std::forward<Getter>(getter));
        }

        template <typename Value, typename Getter, typename Setter, typename Drawer>
        bool MultiValue(const Vector<Entity>& entities, const char* componentName, const char* propertyName, Getter&& getter, Setter&& setter,
                        Drawer&& drawer)
        {
            return InspectorPropertyAccessor(entities, componentName)
              .Edit<Value>(propertyName, std::forward<Getter>(getter), std::forward<Setter>(setter), std::forward<Drawer>(drawer));
        }

        template <typename FlagsType, typename Enum, typename Getter, typename Setter>
        bool MultiFlags(const char* label, std::initializer_list<const char*> buttonLabels, std::initializer_list<Enum> bits,
                        const Vector<Entity>& entities, const char* componentName, const char* propertyName, Getter&& getter, Setter&& setter)
        {
            if (entities.empty() || buttonLabels.size() != bits.size() || bits.size() == 0u)
                return false;

            UI::Pre(label);
            UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            const float buttonWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x / static_cast<float>(bits.size()));
            bool changed = false;
            auto bit = bits.begin();
            uint32_t index = 0u;
            for (const char* buttonLabel : buttonLabels)
            {
                const Enum currentBit = *bit++;
                const bool primarySet = getter(entities.front()).IsSet(currentBit);
                const bool mixed = std::any_of(entities.begin(), entities.end(),
                                               [&](Entity entity) { return entity && getter(entity).IsSet(currentBit) != primarySet; });
                const String id = String(buttonLabel) + "##" + label + std::to_string(index);
                if (!mixed && primarySet)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGuiCol_ButtonActive);
                const bool buttonChanged = ImGui::Button(id.c_str(), ImVec2(buttonWidth, 0.0f));
                if (buttonChanged)
                {
                    const bool set = mixed || !primarySet;
                    InspectorPropertyAccessor(entities, componentName).Assign(propertyName, [&](Entity entity) {
                        FlagsType value = getter(entity);
                        if (set)
                            value.Set(currentBit);
                        else
                            value.Unset(currentBit);
                        setter(entity, value);
                    });
                    changed = true;
                }
                if (!mixed && primarySet)
                    ImGui::PopStyleColor();
                if (mixed)
                {
                    const ImRect bounds = UI::GetItemRect();
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(bounds.Min.x + 7.0f, bounds.GetCenter().y),
                                                        ImVec2(bounds.Max.x - 7.0f, bounds.GetCenter().y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                                        1.5f);
                }
                UndoRedo::Get().OnItemInteract(buttonChanged);
                if (++index < bits.size())
                    ImGui::SameLine(0.0f, 0.0f);
            }
            UI::Post();
            return changed;
        }

        template <typename VectorType, typename Getter, typename Setter>
        bool MultiVector(const char* label, const Vector<Entity>& entities, const char* componentName, const char* propertyName, Getter&& getter,
                         Setter&& setter, float speed = 0.1f, float minimum = 0.0f, float maximum = 0.0f)
        {
            if (entities.empty())
                return false;

            VectorType primaryValue = getter(entities.front());
            const int length = static_cast<int>(primaryValue.length());
            constexpr const char* formats[] = { "X %.3f", "Y %.3f", "Z %.3f", "W %.3f" };
            bool changed = false;

            UI::Pre(label);
            UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
            const float width = std::max(1.0f, (ImGui::GetContentRegionAvail().x - (length - 1) * 3.0f) / static_cast<float>(length));
            for (int axis = 0; axis < length; ++axis)
            {
                if (axis != 0)
                    ImGui::SameLine();
                ImGui::SetNextItemWidth(width);
                const float axisValue = primaryValue[axis];
                const bool mixed =
                  std::any_of(entities.begin(), entities.end(), [&](Entity entity) { return entity && getter(entity)[axis] != axisValue; });
                if (mixed)
                    ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
                ImGui::PushID(axis);
                float editedValue = axisValue;
                const bool axisChanged = UI::DragFloat("##axis", &editedValue, speed, minimum, maximum, formats[axis]);
                UndoRedo::Get().OnItemInteract(axisChanged);
                ImGui::PopID();
                if (mixed)
                    ImGui::PopItemFlag();

                if (!axisChanged)
                    continue;
                changed = true;
                InspectorPropertyAccessor(entities, componentName).Assign(propertyName, [&](Entity entity) {
                    VectorType entityValue = getter(entity);
                    entityValue[axis] = editedValue;
                    setter(entity, entityValue);
                });
            }
            UI::Post();
            return changed;
        }

        template <typename AssetType, typename Getter, typename Setter>
        bool MultiAsset(const char* label, const Vector<Entity>& entities, const char* componentName, const char* propertyName, Getter&& getter,
                        Setter&& setter)
        {
            return MultiValue<AssetHandle<AssetType>>(
              entities, componentName, propertyName, std::forward<Getter>(getter), std::forward<Setter>(setter),
              [&](AssetHandle<AssetType>& value) { return UIUtils::AssetReference<AssetType>(label, value); });
        }

        template <typename Component, typename Member>
        bool MultiMember(const char* label, const Vector<Entity>& entities, const char* componentName, const char* propertyName,
                         Member Component::* member)
        {
            return MultiValue<Member>(
              entities, componentName, propertyName, [member](Entity entity) { return entity.GetComponent<Component>().*member; },
              [member](Entity entity, const Member& value) { entity.GetComponent<Component>().*member = value; },
              [label](Member& value) { return UI::Property(label, value); });
        }

        template <typename Component, typename Member>
        bool MultiColor(const char* label, const Vector<Entity>& entities, const char* componentName, const char* propertyName,
                        Member Component::* member)
        {
            return MultiValue<Member>(
              entities, componentName, propertyName, [member](Entity entity) { return entity.GetComponent<Component>().*member; },
              [member](Entity entity, const Member& value) { entity.GetComponent<Component>().*member = value; },
              [label](Member& value) { return UI::PropertyColor(label, value); });
        }

        template <typename Component, typename Enum>
        bool MultiDropdown(const char* label, std::initializer_list<const char*> options, const Vector<Entity>& entities, const char* componentName,
                           const char* propertyName, Enum Component::* member)
        {
            return MultiValue<Enum>(
              entities, componentName, propertyName, [member](Entity entity) { return entity.GetComponent<Component>().*member; },
              [member](Entity entity, Enum value) { entity.GetComponent<Component>().*member = value; },
              [label, options](Enum& value) { return UI::PropertyDropdown(label, options, value); });
        }

        template <typename Collider, typename Setter>
        void MultiCollider3DBase(const Vector<Entity>& entities, const char* componentName, Setter&& getCollider)
        {
            MultiVector<glm::vec3>(
              "Offset", entities, componentName, "Offset", [&](Entity entity) { return getCollider(entity).GetOffset(); },
              [&](Entity entity, const glm::vec3& value) { getCollider(entity).SetOffset(value, entity); }, 0.05f);
            MultiVector<glm::vec3>(
              "Rotation", entities, componentName, "Rotation",
              [&](Entity entity) { return glm::degrees(glm::eulerAngles(getCollider(entity).GetRotation())); },
              [&](Entity entity, const glm::vec3& value) { getCollider(entity).SetRotation(glm::quat(glm::radians(value)), entity); }, 0.5f);
            MultiValue<bool>(
              entities, componentName, "IsTrigger", [&](Entity entity) { return getCollider(entity).IsTrigger(); },
              [&](Entity entity, bool value) { getCollider(entity).SetIsTrigger(value); },
              [](bool& value) { return UI::Property("Is Trigger", value); });

            MultiAsset<PhysicsMaterial3D>(
              "Material", entities, componentName, "Material", [&](Entity entity) { return getCollider(entity).GetMaterial(); },
              [&](Entity entity, const AssetHandle<PhysicsMaterial3D>& value) { getCollider(entity).SetMaterial(value); });
            MultiValue<uint32_t>(
              entities, componentName, "Filter.Layer", [&](Entity entity) { return getCollider(entity).GetFilter().Layer; },
              [&](Entity entity, uint32_t value) {
                  PhysicsFilter3D filter = getCollider(entity).GetFilter();
                  filter.Layer = value;
                  getCollider(entity).SetFilter(filter, entity);
              },
              [](uint32_t& value) { return UI::Property("Layer", value); });
            MultiValue<uint32_t>(
              entities, componentName, "Filter.Mask", [&](Entity entity) { return getCollider(entity).GetFilter().Mask; },
              [&](Entity entity, uint32_t value) {
                  PhysicsFilter3D filter = getCollider(entity).GetFilter();
                  filter.Mask = value;
                  getCollider(entity).SetFilter(filter, entity);
              },
              [](uint32_t& value) { return UI::Property("Mask", value); });
            MultiValue<int32_t>(
              entities, componentName, "Filter.Group", [&](Entity entity) { return getCollider(entity).GetFilter().Group; },
              [&](Entity entity, int32_t value) {
                  PhysicsFilter3D filter = getCollider(entity).GetFilter();
                  filter.Group = value;
                  getCollider(entity).SetFilter(filter, entity);
              },
              [](int32_t& value) { return UI::Property("Group", value); });
        }
    } // namespace

    template <> void ComponentSelectionEditorWidget<TransformComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiVector<glm::vec3>(
          "Position", entities, "Transform", "Position", [](Entity entity) { return entity.GetLocalPosition(); },
          [](Entity entity, const glm::vec3& value) { entity.SetPosition(value); });
        MultiVector<glm::vec3>(
          "Rotation", entities, "Transform", "Rotation", [](Entity entity) { return glm::degrees(glm::eulerAngles(entity.GetLocalRotation())); },
          [](Entity entity, const glm::vec3& value) { entity.SetRotation(glm::quat(glm::radians(value))); }, 0.5f);
        MultiVector<glm::vec3>(
          "Scale", entities, "Transform", "Scale", [](Entity entity) { return entity.GetLocalScale(); },
          [](Entity entity, const glm::vec3& value) { entity.SetScale(value); }, 0.05f);
    }

    template <> void ComponentSelectionEditorWidget<CameraComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiValue<glm::vec3>(
          entities, "Camera", "Background", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetBackgroundColor(); },
          [](Entity entity, const glm::vec3& value) { entity.GetComponent<CameraComponent>().Camera.SetBackgroundColor(value); },
          [](glm::vec3& value) { return UI::PropertyColor("Background", value); });
        const bool projectionMixed = HasMixedValue(entities, entities.front().GetComponent<CameraComponent>().Camera.GetProjectionType(),
                                                   [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetProjectionType(); });
        MultiValue<SceneCamera::CameraProjection>(
          entities, "Camera", "Projection", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetProjectionType(); },
          [](Entity entity, SceneCamera::CameraProjection value) { entity.GetComponent<CameraComponent>().Camera.SetProjectionType(value); },
          [](SceneCamera::CameraProjection& value) { return UI::PropertyDropdown("Projection", { "Orthographic", "Perspective" }, value); });

        if (!projectionMixed)
        {
            const SceneCamera::CameraProjection projection = entities.front().GetComponent<CameraComponent>().Camera.GetProjectionType();
            if (projection == SceneCamera::CameraProjection::Perspective)
            {
                MultiValue<float>(
                  entities, "Camera", "FieldOfView",
                  [](Entity entity) { return glm::degrees(entity.GetComponent<CameraComponent>().Camera.GetPerspectiveVerticalFOV()); },
                  [](Entity entity, float value) {
                      entity.GetComponent<CameraComponent>().Camera.SetPerspectiveVerticalFOV(glm::radians(std::clamp(value, 1.0f, 179.0f)));
                  },
                  [](float& value) { return UI::Property("Field of View", value, 0.5f, 1.0f, 179.0f); });
                MultiValue<float>(
                  entities, "Camera", "Near", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveNearClip(); },
                  [](Entity entity, float value) { entity.GetComponent<CameraComponent>().Camera.SetPerspectiveNearClip(std::max(value, 0.0001f)); },
                  [](float& value) { return UI::Property("Near", value, 0.01f, 0.0001f, 1000000.0f); });
                MultiValue<float>(
                  entities, "Camera", "Far", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetPerspectiveFarClip(); },
                  [](Entity entity, float value) { entity.GetComponent<CameraComponent>().Camera.SetPerspectiveFarClip(std::max(value, 0.0001f)); },
                  [](float& value) { return UI::Property("Far", value, 1.0f, 0.0001f, 1000000.0f); });
            }
            else
            {
                MultiValue<float>(
                  entities, "Camera", "Size", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetOrthographicSize(); },
                  [](Entity entity, float value) { entity.GetComponent<CameraComponent>().Camera.SetOrthographicSize(std::max(value, 0.0001f)); },
                  [](float& value) { return UI::Property("Size", value, 0.1f, 0.0001f, 1000000.0f); });
                MultiValue<float>(
                  entities, "Camera", "Near", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetOrthographicNearClip(); },
                  [](Entity entity, float value) { entity.GetComponent<CameraComponent>().Camera.SetOrthographicNearClip(value); },
                  [](float& value) { return UI::Property("Near", value, 0.01f, -1000000.0f, 1000000.0f); });
                MultiValue<float>(
                  entities, "Camera", "Far", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetOrthographicFarClip(); },
                  [](Entity entity, float value) { entity.GetComponent<CameraComponent>().Camera.SetOrthographicFarClip(value); },
                  [](float& value) { return UI::Property("Far", value, 0.01f, -1000000.0f, 1000000.0f); });
            }
        }

        MultiVector<glm::vec4>(
          "Viewport", entities, "Camera", "Viewport", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetViewportRect(); },
          [](Entity entity, const glm::vec4& value) { entity.GetComponent<CameraComponent>().Camera.SetViewportRect(glm::clamp(value, 0.0f, 1.0f)); },
          0.01f, 0.0f, 1.0f);
        MultiValue<bool>(
          entities, "Camera", "OcclusionCulling", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetOcclusionCulling(); },
          [](Entity entity, bool value) { entity.GetComponent<CameraComponent>().Camera.SetOcclusionCulling(value); },
          [](bool& value) { return UI::Property("Occlusion Culling", value); });
        MultiValue<bool>(
          entities, "Camera", "HDR", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetHDR(); },
          [](Entity entity, bool value) { entity.GetComponent<CameraComponent>().Camera.SetHDR(value); },
          [](bool& value) { return UI::Property("HDR", value); });
        MultiValue<bool>(
          entities, "Camera", "MSAA", [](Entity entity) { return entity.GetComponent<CameraComponent>().Camera.GetMSAA(); },
          [](Entity entity, bool value) { entity.GetComponent<CameraComponent>().Camera.SetMSAA(value); },
          [](bool& value) { return UI::Property("MSAA", value); });
    }

    template <> void ComponentSelectionEditorWidget<LightComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        const InspectorPropertyAccessor properties(entities, "Light");
        MultiDropdown<LightComponent>("Type", { "Directional", "Point", "Spot" }, entities, "Light", "Type", &LightComponent::Type);
        MultiColor("Color", entities, "Light", "Color", &LightComponent::Color);
        MultiMember("Use Temperature", entities, "Light", "UseColorTemperature", &LightComponent::UseColorTemperature);

        const auto getTemperatureEnabled = [](Entity entity) { return entity.GetComponent<LightComponent>().UseColorTemperature; };
        if (!properties.IsMixed(getTemperatureEnabled) && getTemperatureEnabled(entities.front()))
        {
            MultiValue<float>(
              entities, "Light", "Temperature", [](Entity entity) { return entity.GetComponent<LightComponent>().Temperature; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Temperature = std::clamp(value, 1000.0f, 40000.0f); },
              [](float& value) { return UI::Property("Temperature (K)", value, 50.0f, 1000.0f, 40000.0f); });
        }

        const auto getType = [](Entity entity) { return entity.GetComponent<LightComponent>().Type; };
        const bool typeMixed = properties.IsMixed(getType);
        const LightType type = getType(entities.front());
        const char* intensityLabel = !typeMixed && type == LightType::Directional ? "Illuminance (lux)" : !typeMixed ? "Flux (lumens)" : "Intensity";
        MultiValue<float>(
          entities, "Light", "Intensity", [](Entity entity) { return entity.GetComponent<LightComponent>().Intensity; },
          [](Entity entity, float value) { entity.GetComponent<LightComponent>().Intensity = std::max(value, 0.0f); },
          [intensityLabel](float& value) { return UI::Property(intensityLabel, value, 10.0f, 0.0f, 0.0f); });
        if (!typeMixed && type != LightType::Directional)
        {
            MultiValue<float>(
              entities, "Light", "Range", [](Entity entity) { return entity.GetComponent<LightComponent>().Range; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Range = std::max(value, 0.001f); },
              [](float& value) { return UI::Property("Range", value, 0.1f, 0.001f, 0.0f); });
            MultiValue<float>(
              entities, "Light", "SourceRadius", [](Entity entity) { return entity.GetComponent<LightComponent>().SourceRadius; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().SourceRadius = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Source Radius", value, 0.01f, 0.0f, 0.0f); });
        }
        if (!typeMixed && type == LightType::Spot)
        {
            MultiValue<float>(
              entities, "Light", "SpotInnerAngle", [](Entity entity) { return glm::degrees(entity.GetComponent<LightComponent>().SpotInnerAngle); },
              [](Entity entity, float value) {
                  LightComponent& light = entity.GetComponent<LightComponent>();
                  light.SpotInnerAngle = glm::radians(std::clamp(value, 0.0f, glm::degrees(light.SpotOuterAngle)));
              },
              [](float& value) { return UI::Property("Inner Angle", value, 0.25f, 0.0f, 180.0f); });
            MultiValue<float>(
              entities, "Light", "SpotOuterAngle", [](Entity entity) { return glm::degrees(entity.GetComponent<LightComponent>().SpotOuterAngle); },
              [](Entity entity, float value) {
                  LightComponent& light = entity.GetComponent<LightComponent>();
                  light.SpotOuterAngle = glm::radians(std::clamp(value, glm::degrees(light.SpotInnerAngle), 180.0f));
              },
              [](float& value) { return UI::Property("Outer Angle", value, 0.25f, 0.0f, 180.0f); });
        }

        MultiMember("Enabled", entities, "Light", "Enabled", &LightComponent::Enabled);
        MultiMember("Affect Diffuse", entities, "Light", "AffectDiffuse", &LightComponent::AffectDiffuse);
        MultiMember("Affect Specular", entities, "Light", "AffectSpecular", &LightComponent::AffectSpecular);
        MultiMember("Volumetric", entities, "Light", "Volumetric", &LightComponent::Volumetric);

        MultiValue<LightShadowMode>(
          entities, "Light", "Shadows.Mode", [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.Mode; },
          [](Entity entity, LightShadowMode value) { entity.GetComponent<LightComponent>().Shadows.Mode = value; },
          [](LightShadowMode& value) { return UI::PropertyDropdown("Shadows", { "Disabled", "Hard", "Soft" }, value); });
        const auto getShadowMode = [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.Mode; };
        if (!properties.IsMixed(getShadowMode) && getShadowMode(entities.front()) != LightShadowMode::Disabled)
        {
            MultiValue<float>(
              entities, "Light", "Shadows.Bias", [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.Bias; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Shadows.Bias = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Shadow Bias", value, 0.0001f, 0.0f, 0.0f); });
            MultiValue<float>(
              entities, "Light", "Shadows.NormalBias", [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.NormalBias; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Shadows.NormalBias = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Shadow Normal Bias", value, 0.001f, 0.0f, 0.0f); });
            MultiValue<float>(
              entities, "Light", "Shadows.NearPlane", [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.NearPlane; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Shadows.NearPlane = std::max(value, 0.001f); },
              [](float& value) { return UI::Property("Shadow Near", value, 0.001f, 0.001f, 0.0f); });
            MultiValue<float>(
              entities, "Light", "Shadows.Importance", [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.Importance; },
              [](Entity entity, float value) { entity.GetComponent<LightComponent>().Shadows.Importance = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Shadow Importance", value, 0.05f, 0.0f, 0.0f); });
            MultiValue<uint32_t>(
              entities, "Light", "Shadows.Resolution",
              [](Entity entity) { return static_cast<uint32_t>(entity.GetComponent<LightComponent>().Shadows.Resolution); },
              [](Entity entity, uint32_t value) {
                  entity.GetComponent<LightComponent>().Shadows.Resolution = static_cast<uint16_t>(std::clamp(value, 64u, 8192u));
              },
              [](uint32_t& value) { return UI::Property("Shadow Resolution", value, 64u, 8192u); });
            MultiValue<bool>(
              entities, "Light", "Shadows.CacheStaticCasters",
              [](Entity entity) { return entity.GetComponent<LightComponent>().Shadows.CacheStaticCasters; },
              [](Entity entity, bool value) { entity.GetComponent<LightComponent>().Shadows.CacheStaticCasters = value; },
              [](bool& value) { return UI::Property("Cache Static Casters", value); });
        }
    }

    template <> void ComponentSelectionEditorWidget<TextComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        const InspectorPropertyAccessor properties(entities, "Text");
        MultiValue<String>(
          entities, "Text", "Text", [](Entity entity) { return entity.GetComponent<TextComponent>().Text; },
          [](Entity entity, const String& value) { entity.GetComponent<TextComponent>().Text = value; },
          [](String& value) { return UI::PropertyMultiline("Text", value); });
        MultiAsset<Font>(
          "Font", entities, "Text", "Font", [](Entity entity) { return entity.GetComponent<TextComponent>().Font; },
          [](Entity entity, const AssetHandle<Font>& value) { entity.GetComponent<TextComponent>().Font = value; });
        MultiFlags<TextFontStyle>(
          "Style", { "Bold", "Italic", "Underline", "Strike" },
          { TextFontStyleBits::Bold, TextFontStyleBits::Italic, TextFontStyleBits::Underline, TextFontStyleBits::Strikethrough }, entities, "Text",
          "FontStyle", [](Entity entity) { return entity.GetComponent<TextComponent>().FontStyle; },
          [](Entity entity, TextFontStyle value) { entity.GetComponent<TextComponent>().FontStyle = value; });
        MultiColor("Color", entities, "Text", "Color", &TextComponent::Color);
        MultiVector<glm::vec2>(
          "Layout Size", entities, "Text", "LayoutSize", [](Entity entity) { return entity.GetComponent<TextComponent>().LayoutSize; },
          [](Entity entity, const glm::vec2& value) { entity.GetComponent<TextComponent>().LayoutSize = glm::max(value, glm::vec2(0.0f)); }, 0.1f,
          0.0f, 0.0f);

        const EditorAssetsLibrary assets = EditorAssets::Get();
        MultiValue<TextHorizontalAlignment>(
          entities, "Text", "HorizontalAlignment", [](Entity entity) { return entity.GetComponent<TextComponent>().HorizontalAlignment; },
          [](Entity entity, TextHorizontalAlignment value) { entity.GetComponent<TextComponent>().HorizontalAlignment = value; },
          [&](TextHorizontalAlignment& value) {
              return UI::PropertyIconTabs(
                "Horizontal Alignment",
                { { "Left", static_cast<int32_t>(TextHorizontalAlignment::Left), assets.AlignLeft },
                  { "Center", static_cast<int32_t>(TextHorizontalAlignment::Center), assets.AlignCenter },
                  { "Right", static_cast<int32_t>(TextHorizontalAlignment::Right), assets.AlignRight },
                  { "Justified", static_cast<int32_t>(TextHorizontalAlignment::Justified), {}, UI::PropertyIconGlyph::Justified },
                  { "Flush", static_cast<int32_t>(TextHorizontalAlignment::Flush), {}, UI::PropertyIconGlyph::Flush } },
                value);
          });
        MultiValue<TextVerticalAlignment>(
          entities, "Text", "VerticalAlignment", [](Entity entity) { return entity.GetComponent<TextComponent>().VerticalAlignment; },
          [](Entity entity, TextVerticalAlignment value) { entity.GetComponent<TextComponent>().VerticalAlignment = value; },
          [](TextVerticalAlignment& value) {
              return UI::PropertyIconTabs(
                "Vertical Alignment",
                { { "Top", static_cast<int32_t>(TextVerticalAlignment::Top), {}, UI::PropertyIconGlyph::VerticalTop },
                  { "Middle", static_cast<int32_t>(TextVerticalAlignment::Middle), {}, UI::PropertyIconGlyph::VerticalMiddle },
                  { "Bottom", static_cast<int32_t>(TextVerticalAlignment::Bottom), {}, UI::PropertyIconGlyph::VerticalBottom },
                  { "Baseline", static_cast<int32_t>(TextVerticalAlignment::Baseline), {}, UI::PropertyIconGlyph::Baseline },
                  { "Midline", static_cast<int32_t>(TextVerticalAlignment::Midline), {}, UI::PropertyIconGlyph::Midline } },
                value);
          });
        MultiMember("Auto Size", entities, "Text", "AutoSize", &TextComponent::AutoSize);
        const auto getAutoSize = [](Entity entity) { return entity.GetComponent<TextComponent>().AutoSize; };
        const bool autoSizeMixed = properties.IsMixed(getAutoSize);
        const bool autoSize = getAutoSize(entities.front());
        {
            UI::ScopedDisable disableSize(!autoSizeMixed && autoSize);
            MultiValue<float>(
              entities, "Text", "Size", [](Entity entity) { return entity.GetComponent<TextComponent>().Size; },
              [](Entity entity, float value) { entity.GetComponent<TextComponent>().Size = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Size", value, 1.0f, 0.0f, 0.0f); });
        }
        if (!autoSizeMixed && autoSize)
        {
            MultiValue<float>(
              entities, "Text", "AutoSizeMin", [](Entity entity) { return entity.GetComponent<TextComponent>().AutoSizeMin; },
              [](Entity entity, float value) {
                  TextComponent& text = entity.GetComponent<TextComponent>();
                  text.AutoSizeMin = std::max(value, 0.0f);
                  text.AutoSizeMax = std::max(text.AutoSizeMin, text.AutoSizeMax);
              },
              [](float& value) { return UI::Property("Minimum Size", value, 1.0f, 0.0f, 0.0f); });
            MultiValue<float>(
              entities, "Text", "AutoSizeMax", [](Entity entity) { return entity.GetComponent<TextComponent>().AutoSizeMax; },
              [](Entity entity, float value) {
                  TextComponent& text = entity.GetComponent<TextComponent>();
                  text.AutoSizeMax = std::max(value, 0.0f);
                  text.AutoSizeMin = std::min(text.AutoSizeMin, text.AutoSizeMax);
              },
              [](float& value) { return UI::Property("Maximum Size", value, 1.0f, 0.0f, 0.0f); });
        }
        MultiMember("Wrapping", entities, "Text", "Wrapping", &TextComponent::Wrapping);
        const auto getWrapping = [](Entity entity) { return entity.GetComponent<TextComponent>().Wrapping; };
        if (!properties.IsMixed(getWrapping) && getWrapping(entities.front()))
        {
            MultiDropdown<TextComponent>("Wrap Mode", { "Word", "Character", "Word Then Character" }, entities, "Text", "WrapMode",
                                         &TextComponent::WrapMode);
        }
        MultiDropdown<TextComponent>("Overflow", { "Overflow", "Ellipsis", "Truncate" }, entities, "Text", "Overflow", &TextComponent::Overflow);
        MultiMember("Clip To Bounds", entities, "Text", "ClipToBounds", &TextComponent::ClipToBounds);
        MultiMember("Max Lines", entities, "Text", "MaxLines", &TextComponent::MaxLines);
        MultiColor("Outline", entities, "Text", "OutlineColor", &TextComponent::OutlineColor);
        MultiValue<float>(
          entities, "Text", "Thickness", [](Entity entity) { return entity.GetComponent<TextComponent>().Thickness; },
          [](Entity entity, float value) { entity.GetComponent<TextComponent>().Thickness = std::max(value, 0.0f); },
          [](float& value) { return UI::Property("Outline Width", value, 0.05f, 0.0f, 0.0f); });
        MultiColor("Shadow Color", entities, "Text", "ShadowColor", &TextComponent::ShadowColor);
        MultiVector<glm::vec2>(
          "Shadow Offset", entities, "Text", "ShadowOffset", [](Entity entity) { return entity.GetComponent<TextComponent>().ShadowOffset; },
          [](Entity entity, const glm::vec2& value) { entity.GetComponent<TextComponent>().ShadowOffset = value; }, 0.05f);
        MultiValue<float>(
          entities, "Text", "ShadowSoftness", [](Entity entity) { return entity.GetComponent<TextComponent>().ShadowSoftness; },
          [](Entity entity, float value) { entity.GetComponent<TextComponent>().ShadowSoftness = std::max(value, 0.0f); },
          [](float& value) { return UI::Property("Shadow Softness", value, 0.05f, 0.0f, 0.0f); });
        MultiMember("Use Kerning", entities, "Text", "UseKerning", &TextComponent::UseKerning);
        MultiMember("Character Spacing", entities, "Text", "CharacterSpacing", &TextComponent::CharacterSpacing);
        MultiMember("Word Spacing", entities, "Text", "WordSpacing", &TextComponent::WordSpacing);
        MultiMember("Line Spacing", entities, "Text", "LineSpacing", &TextComponent::LineSpacing);
        MultiMember("Paragraph Spacing", entities, "Text", "ParagraphSpacing", &TextComponent::ParagraphSpacing);
        MultiValue<uint32_t>(
          entities, "Text", "TabWidth", [](Entity entity) { return entity.GetComponent<TextComponent>().TabWidth; },
          [](Entity entity, uint32_t value) { entity.GetComponent<TextComponent>().TabWidth = std::max(1u, value); },
          [](uint32_t& value) { return UI::Property("Tab Width", value); });
        MultiMember("Sorting Layer", entities, "Text", "SortingLayer", &TextComponent::SortingLayer);
        MultiMember("Order In Layer", entities, "Text", "OrderInLayer", &TextComponent::OrderInLayer);

        const auto hasUnderline = [](Entity entity) { return entity.GetComponent<TextComponent>().FontStyle.IsSet(TextFontStyleBits::Underline); };
        const auto hasStrikethrough = [](Entity entity) {
            return entity.GetComponent<TextComponent>().FontStyle.IsSet(TextFontStyleBits::Strikethrough);
        };
        const bool underlineMixed = properties.IsMixed(hasUnderline);
        const bool strikethroughMixed = properties.IsMixed(hasStrikethrough);
        const bool underline = hasUnderline(entities.front());
        const bool strikethrough = hasStrikethrough(entities.front());
        if (!underlineMixed && !strikethroughMixed && (underline || strikethrough))
        {
            MultiMember("Custom Decoration Color", entities, "Text", "UseCustomDecorationColor", &TextComponent::UseCustomDecorationColor);
            const auto useCustomColor = [](Entity entity) { return entity.GetComponent<TextComponent>().UseCustomDecorationColor; };
            {
                const bool customColorMixed = properties.IsMixed(useCustomColor);
                UI::ScopedDisable disableColor(!customColorMixed && !useCustomColor(entities.front()));
                MultiColor("Decoration Color", entities, "Text", "DecorationColor", &TextComponent::DecorationColor);
            }
            MultiValue<float>(
              entities, "Text", "DecorationThickness", [](Entity entity) { return entity.GetComponent<TextComponent>().DecorationThickness; },
              [](Entity entity, float value) { entity.GetComponent<TextComponent>().DecorationThickness = std::max(value, 0.0f); },
              [](float& value) { return UI::Property("Decoration Thickness", value, 0.01f, 0.0f, 0.0f); });
            if (underline)
                MultiMember("Underline Offset", entities, "Text", "UnderlineOffset", &TextComponent::UnderlineOffset);
            if (strikethrough)
                MultiMember("Strikethrough Offset", entities, "Text", "StrikethroughOffset", &TextComponent::StrikethroughOffset);
        }
    }

    template <> void ComponentSelectionEditorWidget<SpriteRendererComponent>(Entity primary, const Vector<Entity>& entities)
    {
        if (entities.size() == 1u)
        {
            const AssetHandle<Texture>& texture = primary.GetComponent<SpriteRendererComponent>().Texture;
            const Ref<Texture>& preview = texture ? texture.GetInternalPtr() : EditorAssets::Get().UnassignedTexture;
            ImGui::Image(ImGuiVulkanTexture::Get(preview), { 50.0f, 50.0f }, { 0, 1 }, { 1, 0 });
        }
        MultiAsset<Texture>(
          "Texture", entities, "Sprite Renderer", "Texture", [](Entity entity) { return entity.GetComponent<SpriteRendererComponent>().Texture; },
          [](Entity entity, const AssetHandle<Texture>& value) { entity.GetComponent<SpriteRendererComponent>().Texture = value; });
        MultiColor("Color", entities, "Sprite Renderer", "Color", &SpriteRendererComponent::Color);
        MultiMember("Sorting Layer", entities, "Sprite Renderer", "SortingLayer", &SpriteRendererComponent::SortingLayer);
        MultiMember("Order In Layer", entities, "Sprite Renderer", "OrderInLayer", &SpriteRendererComponent::OrderInLayer);
    }

    template <> void ComponentSelectionEditorWidget<MeshRendererComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiAsset<Mesh>(
          "Mesh", entities, "Mesh Filter", "MeshHandle", [](Entity entity) { return entity.GetComponent<MeshRendererComponent>().MeshHandle; },
          [](Entity entity, const AssetHandle<Mesh>& value) { entity.GetComponent<MeshRendererComponent>().MeshHandle = value; });

        uint32_t commonSlots = UINT32_MAX;
        for (Entity entity : entities)
        {
            const MeshRendererComponent& mesh = entity.GetComponent<MeshRendererComponent>();
            const uint32_t subMeshCount = mesh.MeshHandle ? static_cast<uint32_t>(mesh.MeshHandle->GetSubMeshes().size()) : 0u;
            commonSlots = std::min(commonSlots, std::max(1u, std::max(subMeshCount, mesh.GetMaterialCount())));
        }
        for (uint32_t slot = 0; slot < commonSlots; ++slot)
        {
            const String label = commonSlots == 1u ? "Material" : "Material " + std::to_string(slot);
            MultiAsset<Material>(
              label.c_str(), entities, "Mesh Filter", "Materials",
              [slot](Entity entity) { return entity.GetComponent<MeshRendererComponent>().GetMaterial(slot); },
              [slot](Entity entity, const AssetHandle<Material>& value) { entity.GetComponent<MeshRendererComponent>().SetMaterial(slot, value); });
        }
        MultiMember("Visible", entities, "Mesh Filter", "Visible", &MeshRendererComponent::Visible);
        MultiMember("Cast Shadows", entities, "Mesh Filter", "CastShadows", &MeshRendererComponent::CastShadows);
        MultiMember("Receive Shadows", entities, "Mesh Filter", "ReceiveShadows", &MeshRendererComponent::ReceiveShadows);
        MultiMember("Motion Vectors", entities, "Mesh Filter", "MotionVectors", &MeshRendererComponent::MotionVectors);
        MultiValue<float>(
          entities, "Mesh Filter", "LodBias", [](Entity entity) { return entity.GetComponent<MeshRendererComponent>().LodBias; },
          [](Entity entity, float value) { entity.GetComponent<MeshRendererComponent>().LodBias = std::clamp(value, -8.0f, 8.0f); },
          [](float& value) { return UI::Property("LOD Bias", value, 0.05f, -8.0f, 8.0f); });
        MultiMember("Render Layer Order", entities, "Mesh Filter", "RenderLayerOrder", &MeshRendererComponent::RenderLayerOrder);
    }

    template <> void ComponentSelectionEditorWidget<AnimationComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        const auto reset = [](Entity entity) { entity.GetComponent<AnimationComponent>().ResetRuntime(); };
        MultiAsset<AnimationClip>(
          "Clip", entities, "Animation", "Clip", [](Entity entity) { return entity.GetComponent<AnimationComponent>().Clip; },
          [&](Entity entity, const AssetHandle<AnimationClip>& value) {
              entity.GetComponent<AnimationComponent>().Clip = value;
              reset(entity);
          });
        MultiValue<float>(
          entities, "Animation", "Speed", [](Entity entity) { return entity.GetComponent<AnimationComponent>().Speed; },
          [&](Entity entity, float value) {
              entity.GetComponent<AnimationComponent>().Speed = value;
              reset(entity);
          },
          [](float& value) { return UI::Property("Speed", value, 0.05f); });
        MultiValue<AnimationWrapMode>(
          entities, "Animation", "WrapMode", [](Entity entity) { return entity.GetComponent<AnimationComponent>().WrapMode; },
          [&](Entity entity, AnimationWrapMode value) {
              entity.GetComponent<AnimationComponent>().WrapMode = value;
              reset(entity);
          },
          [](AnimationWrapMode& value) { return UI::PropertyDropdown("Wrap Mode", { "Clamp", "Loop", "Ping Pong" }, value); });
        MultiValue<bool>(
          entities, "Animation", "PlayOnAwake", [](Entity entity) { return entity.GetComponent<AnimationComponent>().PlayOnAwake; },
          [&](Entity entity, bool value) {
              entity.GetComponent<AnimationComponent>().PlayOnAwake = value;
              reset(entity);
          },
          [](bool& value) { return UI::Property("Play On Awake", value); });
        MultiValue<bool>(
          entities, "Animation", "ApplyRootMotion", [](Entity entity) { return entity.GetComponent<AnimationComponent>().ApplyRootMotion; },
          [&](Entity entity, bool value) {
              entity.GetComponent<AnimationComponent>().ApplyRootMotion = value;
              reset(entity);
          },
          [](bool& value) { return UI::Property("Apply Root Motion", value); });
    }

    template <> void ComponentSelectionEditorWidget<Rigidbody2DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiValue<uint32_t>(
          entities, "Rigidbody 2D", "Layer", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetLayerMask(); },
          [](Entity entity, uint32_t value) { entity.GetComponent<Rigidbody2DComponent>().SetLayerMask(value, entity); },
          [](uint32_t& value) { return UIUtils::PropertyLayer("Layer", value); });
        const bool typeMixed = HasMixedValue(entities, entities.front().GetComponent<Rigidbody2DComponent>().GetBodyType(),
                                             [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetBodyType(); });
        MultiValue<RigidbodyBodyType>(
          entities, "Rigidbody 2D", "BodyType", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetBodyType(); },
          [](Entity entity, RigidbodyBodyType value) { entity.GetComponent<Rigidbody2DComponent>().SetBodyType(value); },
          [](RigidbodyBodyType& value) { return UI::PropertyDropdown("Body Type", { "Static", "Dynamic", "Kinematic" }, value); });
        if (!typeMixed && entities.front().GetComponent<Rigidbody2DComponent>().GetBodyType() == RigidbodyBodyType::Dynamic)
        {
            MultiValue<bool>(
              entities, "Rigidbody 2D", "AutoMass", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetAutoMass(); },
              [](Entity entity, bool value) { entity.GetComponent<Rigidbody2DComponent>().SetAutoMass(value, entity); },
              [](bool& value) { return UI::Property("Auto Mass", value); });
            MultiValue<float>(
              entities, "Rigidbody 2D", "Mass", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetConfiguredMass(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody2DComponent>().SetMass(std::max(value, 0.0001f)); },
              [](float& value) { return UI::Property("Mass", value, 0.1f, 0.0001f, 0.0f); });
            MultiValue<float>(
              entities, "Rigidbody 2D", "GravityScale", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetGravityScale(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody2DComponent>().SetGravityScale(value); },
              [](float& value) { return UI::Property("Gravity Scale", value); });
            MultiValue<float>(
              entities, "Rigidbody 2D", "LinearDrag", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetLinearDrag(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody2DComponent>().SetLinearDrag(std::max(value, 0.0f)); },
              [](float& value) { return UI::Property("Linear Drag", value); });
            MultiValue<float>(
              entities, "Rigidbody 2D", "AngularDrag", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetAngularDrag(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody2DComponent>().SetAngularDrag(std::max(value, 0.0f)); },
              [](float& value) { return UI::Property("Angular Drag", value); });
        }
        if (!typeMixed && entities.front().GetComponent<Rigidbody2DComponent>().GetBodyType() != RigidbodyBodyType::Static)
        {
            MultiValue<CollisionDetectionMode2D>(
              entities, "Rigidbody 2D", "CollisionDetection",
              [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetCollisionDetectionMode(); },
              [](Entity entity, CollisionDetectionMode2D value) { entity.GetComponent<Rigidbody2DComponent>().SetCollisionDetectionMode(value); },
              [](CollisionDetectionMode2D& value) { return UI::PropertyDropdown("Collision Detection", { "Discrete", "Continuous" }, value); });
            MultiValue<RigidbodySleepMode>(
              entities, "Rigidbody 2D", "SleepMode", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetSleepMode(); },
              [](Entity entity, RigidbodySleepMode value) { entity.GetComponent<Rigidbody2DComponent>().SetSleepMode(value); },
              [](RigidbodySleepMode& value) {
                  return UI::PropertyDropdown("Sleeping Mode", { "Never Sleep", "Start Awake", "Start Sleeping" }, value);
              });
            MultiFlags<Rigidbody2DConstraints>(
              "Constraints", { "Position X", "Position Y", "Rotation" },
              { Rigidbody2DConstraintsBits::FreezePositionX, Rigidbody2DConstraintsBits::FreezePositionY,
                Rigidbody2DConstraintsBits::FreezeRotation },
              entities, "Rigidbody 2D", "Constraints", [](Entity entity) { return entity.GetComponent<Rigidbody2DComponent>().GetConstraints(); },
              [](Entity entity, Rigidbody2DConstraints value) { entity.GetComponent<Rigidbody2DComponent>().SetConstraints(value); });
        }
    }

    template <> void ComponentSelectionEditorWidget<BoxCollider2DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiVector<glm::vec2>(
          "Offset", entities, "Box Collider 2D", "Offset", [](Entity entity) { return entity.GetComponent<BoxCollider2DComponent>().GetOffset(); },
          [](Entity entity, const glm::vec2& value) { entity.GetComponent<BoxCollider2DComponent>().SetOffset(value, entity); }, 0.05f);
        MultiVector<glm::vec2>(
          "Size", entities, "Box Collider 2D", "Size", [](Entity entity) { return entity.GetComponent<BoxCollider2DComponent>().GetSize(); },
          [](Entity entity, const glm::vec2& value) {
              entity.GetComponent<BoxCollider2DComponent>().SetSize(glm::max(value, glm::vec2(0.001f)), entity);
          },
          0.05f, 0.001f, 0.0f);
        MultiValue<bool>(
          entities, "Box Collider 2D", "IsTrigger", [](Entity entity) { return entity.GetComponent<BoxCollider2DComponent>().IsTrigger(); },
          [](Entity entity, bool value) { entity.GetComponent<BoxCollider2DComponent>().SetIsTrigger(value); },
          [](bool& value) { return UI::Property("Is Trigger", value); });
        MultiAsset<PhysicsMaterial2D>(
          "Material", entities, "Box Collider 2D", "Material",
          [](Entity entity) { return entity.GetComponent<BoxCollider2DComponent>().GetMaterial(); },
          [](Entity entity, const AssetHandle<PhysicsMaterial2D>& value) { entity.GetComponent<BoxCollider2DComponent>().SetMaterial(value); });
    }

    template <> void ComponentSelectionEditorWidget<CircleCollider2DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiVector<glm::vec2>(
          "Offset", entities, "Circle Collider 2D", "Offset",
          [](Entity entity) { return entity.GetComponent<CircleCollider2DComponent>().GetOffset(); },
          [](Entity entity, const glm::vec2& value) { entity.GetComponent<CircleCollider2DComponent>().SetOffset(value, entity); }, 0.05f);
        MultiValue<float>(
          entities, "Circle Collider 2D", "Radius", [](Entity entity) { return entity.GetComponent<CircleCollider2DComponent>().GetRadius(); },
          [](Entity entity, float value) { entity.GetComponent<CircleCollider2DComponent>().SetRadius(std::max(value, 0.001f), entity); },
          [](float& value) { return UI::Property("Radius", value, 0.05f, 0.001f, 0.0f); });
        MultiValue<bool>(
          entities, "Circle Collider 2D", "IsTrigger", [](Entity entity) { return entity.GetComponent<CircleCollider2DComponent>().IsTrigger(); },
          [](Entity entity, bool value) { entity.GetComponent<CircleCollider2DComponent>().SetIsTrigger(value); },
          [](bool& value) { return UI::Property("Is Trigger", value); });
        MultiAsset<PhysicsMaterial2D>(
          "Material", entities, "Circle Collider 2D", "Material",
          [](Entity entity) { return entity.GetComponent<CircleCollider2DComponent>().GetMaterial(); },
          [](Entity entity, const AssetHandle<PhysicsMaterial2D>& value) { entity.GetComponent<CircleCollider2DComponent>().SetMaterial(value); });
    }

    template <> void ComponentSelectionEditorWidget<Rigidbody3DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        const bool typeMixed = HasMixedValue(entities, entities.front().GetComponent<Rigidbody3DComponent>().GetBodyType(),
                                             [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetBodyType(); });
        MultiValue<PhysicsBodyType3D>(
          entities, "Rigidbody 3D", "BodyType", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetBodyType(); },
          [](Entity entity, PhysicsBodyType3D value) { entity.GetComponent<Rigidbody3DComponent>().SetBodyType(value, entity); },
          [](PhysicsBodyType3D& value) { return UI::PropertyDropdown("Body Type", { "Static", "Dynamic", "Kinematic" }, value); });
        if (!typeMixed && entities.front().GetComponent<Rigidbody3DComponent>().GetBodyType() == PhysicsBodyType3D::Dynamic)
        {
            MultiValue<bool>(
              entities, "Rigidbody 3D", "AutoMass", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetAutoMass(); },
              [](Entity entity, bool value) { entity.GetComponent<Rigidbody3DComponent>().SetAutoMass(value, entity); },
              [](bool& value) { return UI::Property("Auto Mass", value); });
            MultiValue<float>(
              entities, "Rigidbody 3D", "Mass", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetMass(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody3DComponent>().SetMass(std::max(value, 0.0001f), entity); },
              [](float& value) { return UI::Property("Mass", value, 0.1f, 0.0001f, 0.0f); });
            MultiValue<float>(
              entities, "Rigidbody 3D", "GravityScale", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetGravityScale(); },
              [](Entity entity, float value) { entity.GetComponent<Rigidbody3DComponent>().SetGravityScale(value); },
              [](float& value) { return UI::Property("Gravity Scale", value, 0.05f); });
            MultiValue<float>(
              entities, "Rigidbody 3D", "LinearDamping", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetLinearDamping(); },
              [](Entity entity, float value) {
                  auto& body = entity.GetComponent<Rigidbody3DComponent>();
                  body.SetDamping(std::max(value, 0.0f), body.GetAngularDamping());
              },
              [](float& value) { return UI::Property("Linear Damping", value, 0.01f, 0.0f, 0.0f); });
            MultiValue<float>(
              entities, "Rigidbody 3D", "AngularDamping",
              [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetAngularDamping(); },
              [](Entity entity, float value) {
                  auto& body = entity.GetComponent<Rigidbody3DComponent>();
                  body.SetDamping(body.GetLinearDamping(), std::max(value, 0.0f));
              },
              [](float& value) { return UI::Property("Angular Damping", value, 0.01f, 0.0f, 0.0f); });
        }
        MultiVector<glm::vec3>(
          "Center Of Mass", entities, "Rigidbody 3D", "CenterOfMass",
          [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetCenterOfMass(); },
          [](Entity entity, const glm::vec3& value) { entity.GetComponent<Rigidbody3DComponent>().SetCenterOfMass(value, entity); }, 0.05f);
        MultiValue<bool>(
          entities, "Rigidbody 3D", "ContinuousCollision",
          [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetContinuousCollision(); },
          [](Entity entity, bool value) { entity.GetComponent<Rigidbody3DComponent>().SetContinuousCollision(value, entity); },
          [](bool& value) { return UI::Property("Continuous Collision", value); });
        MultiValue<bool>(
          entities, "Rigidbody 3D", "AllowSleep", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetAllowSleep(); },
          [](Entity entity, bool value) { entity.GetComponent<Rigidbody3DComponent>().SetAllowSleep(value, entity); },
          [](bool& value) { return UI::Property("Allow Sleep", value); });
        MultiValue<bool>(
          entities, "Rigidbody 3D", "StartAwake", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetStartAwake(); },
          [](Entity entity, bool value) { entity.GetComponent<Rigidbody3DComponent>().SetStartAwake(value, entity); },
          [](bool& value) { return UI::Property("Start Awake", value); });
        MultiValue<bool>(
          entities, "Rigidbody 3D", "LockRotationX", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetLockRotationX(); },
          [](Entity entity, bool value) {
              auto& body = entity.GetComponent<Rigidbody3DComponent>();
              body.SetRotationLocks(value, body.GetLockRotationY(), body.GetLockRotationZ(), entity);
          },
          [](bool& value) { return UI::Property("Lock Rotation X", value); });
        MultiValue<bool>(
          entities, "Rigidbody 3D", "LockRotationY", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetLockRotationY(); },
          [](Entity entity, bool value) {
              auto& body = entity.GetComponent<Rigidbody3DComponent>();
              body.SetRotationLocks(body.GetLockRotationX(), value, body.GetLockRotationZ(), entity);
          },
          [](bool& value) { return UI::Property("Lock Rotation Y", value); });
        MultiValue<bool>(
          entities, "Rigidbody 3D", "LockRotationZ", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetLockRotationZ(); },
          [](Entity entity, bool value) {
              auto& body = entity.GetComponent<Rigidbody3DComponent>();
              body.SetRotationLocks(body.GetLockRotationX(), body.GetLockRotationY(), value, entity);
          },
          [](bool& value) { return UI::Property("Lock Rotation Z", value); });
        MultiValue<uint32_t>(
          entities, "Rigidbody 3D", "Filter.Layer", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetFilter().Layer; },
          [](Entity entity, uint32_t value) {
              Rigidbody3DComponent& body = entity.GetComponent<Rigidbody3DComponent>();
              PhysicsFilter3D filter = body.GetFilter();
              filter.Layer = value;
              body.SetFilter(filter);
          },
          [](uint32_t& value) { return UI::Property("Layer", value); });
        MultiValue<uint32_t>(
          entities, "Rigidbody 3D", "Filter.Mask", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetFilter().Mask; },
          [](Entity entity, uint32_t value) {
              Rigidbody3DComponent& body = entity.GetComponent<Rigidbody3DComponent>();
              PhysicsFilter3D filter = body.GetFilter();
              filter.Mask = value;
              body.SetFilter(filter);
          },
          [](uint32_t& value) { return UI::Property("Mask", value); });
        MultiValue<int32_t>(
          entities, "Rigidbody 3D", "Filter.Group", [](Entity entity) { return entity.GetComponent<Rigidbody3DComponent>().GetFilter().Group; },
          [](Entity entity, int32_t value) {
              Rigidbody3DComponent& body = entity.GetComponent<Rigidbody3DComponent>();
              PhysicsFilter3D filter = body.GetFilter();
              filter.Group = value;
              body.SetFilter(filter);
          },
          [](int32_t& value) { return UI::Property("Group", value); });
    }

    template <> void ComponentSelectionEditorWidget<BoxCollider3DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiCollider3DBase<BoxCollider3DComponent>(
          entities, "Box Collider 3D", [](Entity entity) -> BoxCollider3DComponent& { return entity.GetComponent<BoxCollider3DComponent>(); });
        MultiVector<glm::vec3>(
          "Size", entities, "Box Collider 3D", "Size", [](Entity entity) { return entity.GetComponent<BoxCollider3DComponent>().GetSize(); },
          [](Entity entity, const glm::vec3& value) {
              entity.GetComponent<BoxCollider3DComponent>().SetSize(glm::max(value, glm::vec3(0.001f)), entity);
          },
          0.05f, 0.001f, 0.0f);
    }

    template <> void ComponentSelectionEditorWidget<SphereCollider3DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiCollider3DBase<SphereCollider3DComponent>(entities, "Sphere Collider 3D", [](Entity entity) -> SphereCollider3DComponent& {
            return entity.GetComponent<SphereCollider3DComponent>();
        });
        MultiValue<float>(
          entities, "Sphere Collider 3D", "Radius", [](Entity entity) { return entity.GetComponent<SphereCollider3DComponent>().GetRadius(); },
          [](Entity entity, float value) { entity.GetComponent<SphereCollider3DComponent>().SetRadius(std::max(value, 0.001f), entity); },
          [](float& value) { return UI::Property("Radius", value, 0.05f, 0.001f, 0.0f); });
    }

    template <> void ComponentSelectionEditorWidget<CapsuleCollider3DComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiCollider3DBase<CapsuleCollider3DComponent>(entities, "Capsule Collider 3D", [](Entity entity) -> CapsuleCollider3DComponent& {
            return entity.GetComponent<CapsuleCollider3DComponent>();
        });
        MultiValue<float>(
          entities, "Capsule Collider 3D", "Radius", [](Entity entity) { return entity.GetComponent<CapsuleCollider3DComponent>().GetRadius(); },
          [](Entity entity, float value) { entity.GetComponent<CapsuleCollider3DComponent>().SetRadius(std::max(value, 0.001f), entity); },
          [](float& value) { return UI::Property("Radius", value, 0.05f, 0.001f, 0.0f); });
        MultiValue<float>(
          entities, "Capsule Collider 3D", "Height", [](Entity entity) { return entity.GetComponent<CapsuleCollider3DComponent>().GetHeight(); },
          [](Entity entity, float value) {
              auto& collider = entity.GetComponent<CapsuleCollider3DComponent>();
              collider.SetHeight(std::max(value, collider.GetRadius() * 2.0f), entity);
          },
          [](float& value) { return UI::Property("Height", value, 0.05f, 0.001f, 0.0f); });
    }

    template <> void ComponentSelectionEditorWidget<AudioListenerComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        (void)entities;
    }

    template <> void ComponentSelectionEditorWidget<AudioSourceComponent>(Entity primary, const Vector<Entity>& entities)
    {
        (void)primary;
        MultiAsset<AudioClip>(
          "Audio Clip", entities, "Audio Source", "Clip", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetClip(); },
          [](Entity entity, const AssetHandle<AudioClip>& value) { entity.GetComponent<AudioSourceComponent>().SetClip(value); });
        MultiValue<float>(
          entities, "Audio Source", "Volume", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetVolume(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetVolume(value); },
          [](float& value) { return UI::PropertySlider("Volume", value, 0.0f, 1.0f); });
        MultiValue<bool>(
          entities, "Audio Source", "Mute", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetIsMuted(); },
          [](Entity entity, bool value) { entity.GetComponent<AudioSourceComponent>().SetIsMuted(value); },
          [](bool& value) { return UI::Property("Mute", value); });
        MultiValue<float>(
          entities, "Audio Source", "Pitch", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetPitch(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetPitch(value); },
          [](float& value) { return UI::PropertySlider("Pitch", value, -3.0f, 3.0f); });
        MultiValue<bool>(
          entities, "Audio Source", "PlayOnAwake", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetPlayOnAwake(); },
          [](Entity entity, bool value) { entity.GetComponent<AudioSourceComponent>().SetPlayOnAwake(value); },
          [](bool& value) { return UI::Property("Play On Awake", value); });
        MultiValue<bool>(
          entities, "Audio Source", "Loop", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetLooping(); },
          [](Entity entity, bool value) { entity.GetComponent<AudioSourceComponent>().SetLooping(value); },
          [](bool& value) { return UI::Property("Loop", value); });
        MultiValue<float>(
          entities, "Audio Source", "MinDistance", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetMinDistance(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetMinDistance(value); },
          [](float& value) { return UI::Property("Min Distance", value); });
        MultiValue<float>(
          entities, "Audio Source", "MaxDistance", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetMaxDistance(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetMaxDistance(value); },
          [](float& value) { return UI::Property("Max Distance", value); });

        const AssetHandle<AudioMixer> mixer = AudioManager::TryGet()->GetActiveMixer();
        const Vector<AudioBusDesc>* busDescs = mixer ? &mixer->GetBusDescs() : nullptr;
        const size_t busOptionCount = 1u + (busDescs != nullptr ? busDescs->size() : 0u);
        MultiValue<String>(
          entities, "Audio Source", "Bus", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetBusName(); },
          [](Entity entity, const String& value) { entity.GetComponent<AudioSourceComponent>().SetBusName(value); },
          [&](String& value) {
              int selected = 0;
              for (size_t index = 1; index < busOptionCount; index++)
              {
                  if ((*busDescs)[index - 1u].Name == value)
                  {
                      selected = static_cast<int>(index);
                      break;
                  }
              }
              if (!UI::PropertyDropdown("Bus", busOptionCount, selected,
                                        [&](size_t index) { return index == 0 ? "(None)" : (*busDescs)[index - 1u].Name.c_str(); }))
                  return false;
              value = selected == 0 ? String() : (*busDescs)[static_cast<size_t>(selected) - 1u].Name;
              return true;
          });
        MultiValue<float>(
          entities, "Audio Source", "LowPassGain", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetLowPassGain(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetLowPassGain(value); },
          [](float& value) { return UI::PropertySlider("Low Pass Gain", value, 0.0f, 1.0f); });
        MultiValue<float>(
          entities, "Audio Source", "HighPassGain", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetHighPassGain(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetHighPassGain(value); },
          [](float& value) { return UI::PropertySlider("High Pass Gain", value, 0.0f, 1.0f); });
        MultiValue<float>(
          entities, "Audio Source", "ConeInnerAngle", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetConeInnerAngle(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetConeInnerAngle(value); },
          [](float& value) { return UI::PropertySlider("Cone Inner Angle", value, 0.0f, 360.0f); });
        MultiValue<float>(
          entities, "Audio Source", "ConeOuterAngle", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetConeOuterAngle(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetConeOuterAngle(value); },
          [](float& value) { return UI::PropertySlider("Cone Outer Angle", value, 0.0f, 360.0f); });
        MultiValue<float>(
          entities, "Audio Source", "ConeOuterGain", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetConeOuterGain(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetConeOuterGain(value); },
          [](float& value) { return UI::PropertySlider("Cone Outer Gain", value, 0.0f, 1.0f); });
        MultiValue<float>(
          entities, "Audio Source", "ConeOuterGainHF", [](Entity entity) { return entity.GetComponent<AudioSourceComponent>().GetConeOuterGainHF(); },
          [](Entity entity, float value) { entity.GetComponent<AudioSourceComponent>().SetConeOuterGainHF(value); },
          [](float& value) { return UI::PropertySlider("Cone Outer Gain HF", value, 0.0f, 1.0f); });
    }
} // namespace Crowny
