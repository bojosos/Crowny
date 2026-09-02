#include "cwepch.h"

// Built-in component widgets for the entity inspector. Every widget binds its fields
// through the multi-selection SelectionProperty machinery. Script components live in
// ScriptComponentInspector.cpp.

#include "Editor/SelectionProperty.h"
#include "Panels/EntityInspector.h"

#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include "Editor/EditorAssets.h"
#include "UI/Properties.h"
#include "UI/SelectionProperties.h"

#include <glm/gtx/euler_angles.hpp>

namespace Crowny
{
    namespace
    {
        template <typename Collider> void DrawCollider3DProperties(const Vector<Entity>& entities, const char* componentName)
        {
            const auto properties = InspectorSelection(entities, componentName).Components<Collider>();
            UI::Property("Offset", properties.Bind("Offset", &Collider::GetOffset, &Collider::SetOffset), 0.05f);
            UI::Property(
              "Rotation",
              properties.Bind(
                "Rotation", [](const Collider& collider) { return glm::degrees(glm::eulerAngles(collider.GetRotation())); },
                [](Collider& collider, const glm::vec3& value, Entity entity) { collider.SetRotation(glm::quat(glm::radians(value)), entity); }),
              0.5f);
            UI::Property("Is Trigger", properties.Bind("IsTrigger", &Collider::IsTrigger, &Collider::SetIsTrigger));
            UI::PropertyAsset<PhysicsMaterial3D>("Material", properties.Bind("Material", &Collider::GetMaterial, &Collider::SetMaterial));

            const auto filter = properties.Bind("Filter", &Collider::GetFilter, &Collider::SetFilter);
            UI::Property("Layer", filter.Member("Filter", &PhysicsFilter3D::Layer));
            UI::Property("Mask", filter.Member("Filter", &PhysicsFilter3D::Mask));
            UI::Property("Group", filter.Member("Filter", &PhysicsFilter3D::Group));
        }
    } // namespace

    template <> void ComponentSelectionEditorWidget<TransformComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Transform").Components<TransformComponent>();
        UI::ConfigureSelectionVectorColumns(3u);
        UI::Property("Position",
                     properties.Bind(
                       "Position", [](const TransformComponent& transform) { return transform.GetLocalTransform().GetPosition(); },
                       [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetPosition(value); }),
                     UI::SelectionVectorPropertyOptions{ .ResetValue = 0.0f });
        UI::Property(
          "Rotation",
          properties.Bind(
            "Rotation",
            [](const TransformComponent& transform) { return glm::degrees(glm::eulerAngles(transform.GetLocalTransform().GetRotation())); },
            [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetRotation(glm::quat(glm::radians(value))); }),
          UI::SelectionVectorPropertyOptions{ .Speed = 0.5f, .ResetValue = 0.0f });
        UI::Property("Scale",
                     properties.Bind(
                       "Scale", [](const TransformComponent& transform) { return transform.GetLocalTransform().GetScale(); },
                       [](TransformComponent&, const glm::vec3& value, Entity entity) { entity.SetScale(value); }),
                     UI::SelectionVectorPropertyOptions{ .Speed = 0.05f, .ResetValue = 1.0f });
    }

    template <> void ComponentSelectionEditorWidget<CameraComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Camera").Components<CameraComponent>();
        UI::PropertyColor("Background", properties.Bind(
                                          "BackgroundColor", [](const CameraComponent& camera) { return camera.Camera.GetBackgroundColor(); },
                                          [](CameraComponent& camera, const glm::vec3& value) { camera.Camera.SetBackgroundColor(value); }));
        const auto projection = properties.Bind(
          "ProjectionType", [](const CameraComponent& camera) { return camera.Camera.GetProjectionType(); },
          [](CameraComponent& camera, SceneCamera::CameraProjection value) { camera.Camera.SetProjectionType(value); });
        UI::PropertyDropdown("Projection", { "Orthographic", "Perspective" }, projection);

        const SelectionPropertyValue<SceneCamera::CameraProjection> projectionValue = projection.Read();
        if (projectionValue && !projectionValue.Mixed)
        {
            if (*projectionValue.Primary == SceneCamera::CameraProjection::Perspective)
            {
                UI::Property("Field of View",
                             properties.Bind(
                               "PerspectiveFOV", [](const CameraComponent& camera) { return glm::degrees(camera.Camera.GetPerspectiveVerticalFOV()); },
                               [](CameraComponent& camera, float value) {
                                   camera.Camera.SetPerspectiveVerticalFOV(glm::radians(std::clamp(value, 1.0f, 179.0f)));
                               }),
                             0.5f, 1.0f, 179.0f);
                UI::Property("Near",
                             properties.Bind(
                               "PerspectiveNear", [](const CameraComponent& camera) { return camera.Camera.GetPerspectiveNearClip(); },
                               [](CameraComponent& camera, float value) { camera.Camera.SetPerspectiveNearClip(std::max(value, 0.0001f)); }),
                             0.01f, 0.0001f, 1000000.0f);
                UI::Property("Far",
                             properties.Bind(
                               "PerspectiveFar", [](const CameraComponent& camera) { return camera.Camera.GetPerspectiveFarClip(); },
                               [](CameraComponent& camera, float value) { camera.Camera.SetPerspectiveFarClip(std::max(value, 0.0001f)); }),
                             1.0f, 0.0001f, 1000000.0f);
            }
            else
            {
                UI::Property("Size",
                             properties.Bind(
                               "Size", [](const CameraComponent& camera) { return camera.Camera.GetOrthographicSize(); },
                               [](CameraComponent& camera, float value) { camera.Camera.SetOrthographicSize(std::max(value, 0.0001f)); }),
                             0.1f, 0.0001f, 1000000.0f);
                UI::Property("Near",
                             properties.Bind(
                               "Near", [](const CameraComponent& camera) { return camera.Camera.GetOrthographicNearClip(); },
                               [](CameraComponent& camera, float value) { camera.Camera.SetOrthographicNearClip(value); }),
                             0.01f, -1000000.0f, 1000000.0f);
                UI::Property("Far",
                             properties.Bind(
                               "Far", [](const CameraComponent& camera) { return camera.Camera.GetOrthographicFarClip(); },
                               [](CameraComponent& camera, float value) { camera.Camera.SetOrthographicFarClip(value); }),
                             0.01f, -1000000.0f, 1000000.0f);
            }
        }

        UI::Property("Viewport",
                     properties.Bind(
                       "Viewport", [](const CameraComponent& camera) { return camera.Camera.GetViewportRect(); },
                       [](CameraComponent& camera, const glm::vec4& value) { camera.Camera.SetViewportRect(glm::clamp(value, 0.0f, 1.0f)); }),
                     0.01f, 0.0f, 1.0f);
        UI::Property("Occlusion Culling", properties.Bind(
                                            "OcclusionCulling", [](const CameraComponent& camera) { return camera.Camera.GetOcclusionCulling(); },
                                            [](CameraComponent& camera, bool value) { camera.Camera.SetOcclusionCulling(value); }));
        UI::Property("HDR", properties.Bind(
                              "HDR", [](const CameraComponent& camera) { return camera.Camera.GetHDR(); },
                              [](CameraComponent& camera, bool value) { camera.Camera.SetHDR(value); }));
        UI::Property("MSAA", properties.Bind(
                               "MSAA", [](const CameraComponent& camera) { return camera.Camera.GetMSAA(); },
                               [](CameraComponent& camera, bool value) { camera.Camera.SetMSAA(value); }));
    }

    template <> void ComponentSelectionEditorWidget<LightComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Light").Components<LightComponent>();
        const auto typeProperty = properties.Bind("Type", &LightComponent::Type);
        UI::PropertyDropdown("Type", { "Directional", "Point", "Spot" }, typeProperty);
        UI::PropertyColor("Color", properties.Bind("Color", &LightComponent::Color));
        const auto temperatureEnabled = properties.Bind("UseColorTemperature", &LightComponent::UseColorTemperature);
        UI::Property("Use Temperature", temperatureEnabled);

        const SelectionPropertyValue<bool> temperatureEnabledValue = temperatureEnabled.Read();
        if (temperatureEnabledValue && !temperatureEnabledValue.Mixed && *temperatureEnabledValue.Primary)
        {
            UI::Property("Temperature (K)",
                         properties.Bind(
                           "Temperature", [](const LightComponent& light) { return light.Temperature; },
                           [](LightComponent& light, float value) { light.Temperature = std::clamp(value, 1000.0f, 40000.0f); }),
                         50.0f, 1000.0f, 40000.0f);
        }

        const SelectionPropertyValue<LightType> typeValue = typeProperty.Read();
        const bool typeMixed = !typeValue || typeValue.Mixed;
        const LightType type = typeValue.Primary.value_or(LightType::Point);
        const char* intensityLabel = !typeMixed && type == LightType::Directional ? "Illuminance (lux)" : !typeMixed ? "Flux (lumens)" : "Intensity";
        UI::Property(intensityLabel,
                     properties.Bind(
                       "Intensity", [](const LightComponent& light) { return light.Intensity; },
                       [](LightComponent& light, float value) { light.Intensity = std::max(value, 0.0f); }),
                     10.0f, 0.0f, 0.0f);
        if (!typeMixed && type != LightType::Directional)
        {
            UI::Property("Range",
                         properties.Bind(
                           "Range", [](const LightComponent& light) { return light.Range; },
                           [](LightComponent& light, float value) { light.Range = std::max(value, 0.001f); }),
                         0.1f, 0.001f, 0.0f);
            UI::Property("Source Radius",
                         properties.Bind(
                           "SourceRadius", [](const LightComponent& light) { return light.SourceRadius; },
                           [](LightComponent& light, float value) { light.SourceRadius = std::max(value, 0.0f); }),
                         0.01f, 0.0f, 0.0f);
        }
        if (!typeMixed && type == LightType::Spot)
        {
            UI::Property("Inner Angle",
                         properties.Bind(
                           "SpotInnerAngle", [](const LightComponent& light) { return glm::degrees(light.SpotInnerAngle); },
                           [](LightComponent& light, float value) {
                               light.SpotInnerAngle = glm::radians(std::clamp(value, 0.0f, glm::degrees(light.SpotOuterAngle)));
                           }),
                         0.25f, 0.0f, 180.0f);
            UI::Property("Outer Angle",
                         properties.Bind(
                           "SpotOuterAngle", [](const LightComponent& light) { return glm::degrees(light.SpotOuterAngle); },
                           [](LightComponent& light, float value) {
                               light.SpotOuterAngle = glm::radians(std::clamp(value, glm::degrees(light.SpotInnerAngle), 180.0f));
                           }),
                         0.25f, 0.0f, 180.0f);
        }

        UI::Property("Enabled", properties.Bind("Enabled", &LightComponent::Enabled));
        UI::Property("Affect Diffuse", properties.Bind("AffectDiffuse", &LightComponent::AffectDiffuse));
        UI::Property("Affect Specular", properties.Bind("AffectSpecular", &LightComponent::AffectSpecular));
        UI::Property("Volumetric", properties.Bind("Volumetric", &LightComponent::Volumetric));

        const auto shadows = properties.Bind("Shadows", &LightComponent::Shadows);
        const auto shadowMode = shadows.Member("Shadows.Mode", &LightShadowSettings::Mode);
        UI::PropertyDropdown("Shadows", { "Disabled", "Hard", "Soft" }, shadowMode);
        const SelectionPropertyValue<LightShadowMode> shadowModeValue = shadowMode.Read();
        if (shadowModeValue && !shadowModeValue.Mixed && *shadowModeValue.Primary != LightShadowMode::Disabled)
        {
            const auto bias = shadows.Project(
              "Shadows.Bias", [](const LightShadowSettings& value) { return value.Bias; },
              [](LightShadowSettings& value, float bias) { value.Bias = std::max(bias, 0.0f); });
            UI::Property("Shadow Bias", bias, 0.0001f, 0.0f, 0.0f);
            const auto normalBias = shadows.Project(
              "Shadows.NormalBias", [](const LightShadowSettings& value) { return value.NormalBias; },
              [](LightShadowSettings& value, float bias) { value.NormalBias = std::max(bias, 0.0f); });
            UI::Property("Shadow Normal Bias", normalBias, 0.001f, 0.0f, 0.0f);
            const auto nearPlane = shadows.Project(
              "Shadows.NearPlane", [](const LightShadowSettings& value) { return value.NearPlane; },
              [](LightShadowSettings& value, float nearPlane) { value.NearPlane = std::max(nearPlane, 0.001f); });
            UI::Property("Shadow Near", nearPlane, 0.001f, 0.001f, 0.0f);
            const auto importance = shadows.Project(
              "Shadows.Importance", [](const LightShadowSettings& value) { return value.Importance; },
              [](LightShadowSettings& value, float importance) { value.Importance = std::max(importance, 0.0f); });
            UI::Property("Shadow Importance", importance, 0.05f, 0.0f, 0.0f);
            const auto resolution = shadows.Project(
              "Shadows.Resolution", [](const LightShadowSettings& value) { return static_cast<uint32_t>(value.Resolution); },
              [](LightShadowSettings& value, uint32_t resolution) { value.Resolution = static_cast<uint16_t>(std::clamp(resolution, 64u, 8192u)); });
            UI::Property("Shadow Resolution", resolution, 64u, 8192u);
            UI::Property("Cache Static Casters", shadows.Member("Shadows.CacheStaticCasters", &LightShadowSettings::CacheStaticCasters));
        }
    }

    namespace
    {
        // A full-width rollout drawn inside the two-column property grid. The open state is stored by
        // ImGui under the current ID stack, so it persists per component while the editor runs.
        bool BeginInspectorSection(const char* label, bool defaultOpen = false)
        {
            ImGui::Columns(1);
            ImGui::SetNextItemOpen(defaultOpen, ImGuiCond_Once);
            const bool open = ImGui::TreeNodeEx(
              label, ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_FramePadding);
            ImGui::Columns(2);
            return open;
        }
    } // namespace

    template <> void ComponentSelectionEditorWidget<TextComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Text").Components<TextComponent>();

        // Main properties stay flat at the top.
        UI::PropertyMultiline("Text", properties.Bind("Text", &TextComponent::Text));
        UI::PropertyAsset<Font>("Font", properties.Bind("Font", &TextComponent::Font));
        const auto fontStyle = properties.Bind("Style", &TextComponent::FontStyle);
        UI::PropertyFlags("Style", { "Bold", "Italic", "Underline", "Strike" },
                          { TextFontStyleBits::Bold, TextFontStyleBits::Italic, TextFontStyleBits::Underline, TextFontStyleBits::Strikethrough },
                          fontStyle);
        UI::PropertyColor("Color", properties.Bind("Color", &TextComponent::Color));

        const auto autoSizeProperty = properties.Bind("Auto Size", &TextComponent::AutoSize);
        const SelectionPropertyValue<bool> autoSizeValue = autoSizeProperty.Read();
        {
            UI::ScopedDisable disableSize(autoSizeValue && !autoSizeValue.Mixed && *autoSizeValue.Primary);
            UI::Property("Size",
                         properties.Bind(
                           "Size", [](const TextComponent& text) { return text.Size; },
                           [](TextComponent& text, float value) { text.Size = std::max(value, 0.0f); }),
                         1.0f, 0.0f, 0.0f);
        }
        UI::Property("Auto Size", autoSizeProperty);
        if (autoSizeValue && !autoSizeValue.Mixed && *autoSizeValue.Primary)
        {
            UI::Property("Minimum Size",
                         properties.Bind(
                           "Auto Size Min", [](const TextComponent& text) { return text.AutoSizeMin; },
                           [](TextComponent& text, float value) {
                               text.AutoSizeMin = std::max(value, 0.0f);
                               text.AutoSizeMax = std::max(text.AutoSizeMin, text.AutoSizeMax);
                           }),
                         1.0f, 0.0f, 0.0f);
            UI::Property("Maximum Size",
                         properties.Bind(
                           "Auto Size Max", [](const TextComponent& text) { return text.AutoSizeMax; },
                           [](TextComponent& text, float value) {
                               text.AutoSizeMax = std::max(value, 0.0f);
                               text.AutoSizeMin = std::min(text.AutoSizeMin, text.AutoSizeMax);
                           }),
                         1.0f, 0.0f, 0.0f);
        }

        // Advanced groups are collapsed until the user opens them.
        if (BeginInspectorSection("Layout"))
        {
            UI::Property("Layout Size",
                         properties.Bind(
                           "Layout Size", [](const TextComponent& text) { return text.LayoutSize; },
                           [](TextComponent& text, const glm::vec2& value) { text.LayoutSize = glm::max(value, glm::vec2(0.0f)); }),
                         0.1f, 0.0f, 0.0f);

            const EditorAssetsLibrary assets = EditorAssets::Get();
            UI::PropertyIconTabs("Horizontal Alignment",
                                 { { "Left", static_cast<int32_t>(TextHorizontalAlignment::Left), assets.AlignLeft },
                                   { "Center", static_cast<int32_t>(TextHorizontalAlignment::Center), assets.AlignCenter },
                                   { "Right", static_cast<int32_t>(TextHorizontalAlignment::Right), assets.AlignRight },
                                   { "Justified", static_cast<int32_t>(TextHorizontalAlignment::Justified), {}, UI::PropertyIconGlyph::Justified },
                                   { "Flush", static_cast<int32_t>(TextHorizontalAlignment::Flush), {}, UI::PropertyIconGlyph::Flush } },
                                 properties.Bind("Horizontal Alignment", &TextComponent::HorizontalAlignment));
            UI::PropertyIconTabs("Vertical Alignment",
                                 { { "Top", static_cast<int32_t>(TextVerticalAlignment::Top), {}, UI::PropertyIconGlyph::VerticalTop },
                                   { "Middle", static_cast<int32_t>(TextVerticalAlignment::Middle), {}, UI::PropertyIconGlyph::VerticalMiddle },
                                   { "Bottom", static_cast<int32_t>(TextVerticalAlignment::Bottom), {}, UI::PropertyIconGlyph::VerticalBottom },
                                   { "Baseline", static_cast<int32_t>(TextVerticalAlignment::Baseline), {}, UI::PropertyIconGlyph::Baseline },
                                   { "Midline", static_cast<int32_t>(TextVerticalAlignment::Midline), {}, UI::PropertyIconGlyph::Midline } },
                                 properties.Bind("Vertical Alignment", &TextComponent::VerticalAlignment));

            const auto wrapping = properties.Bind("Wrapping", &TextComponent::Wrapping);
            UI::Property("Wrapping", wrapping);
            const SelectionPropertyValue<bool> wrappingValue = wrapping.Read();
            if (wrappingValue && !wrappingValue.Mixed && *wrappingValue.Primary)
                UI::PropertyDropdown("Wrap Mode", { "Word", "Character", "Word Then Character" },
                                     properties.Bind("Wrap Mode", &TextComponent::WrapMode));
            UI::PropertyDropdown("Overflow", { "Overflow", "Ellipsis", "Truncate" }, properties.Bind("Overflow", &TextComponent::Overflow));
            UI::Property("Clip To Bounds", properties.Bind("Clip To Bounds", &TextComponent::ClipToBounds));
            UI::Property("Max Lines", properties.Bind("Max Lines", &TextComponent::MaxLines));
        }

        if (BeginInspectorSection("Spacing"))
        {
            UI::Property("Use Kerning", properties.Bind("Use Kerning", &TextComponent::UseKerning));
            UI::Property("Character Spacing", properties.Bind("Character Spacing", &TextComponent::CharacterSpacing));
            UI::Property("Word Spacing", properties.Bind("Word Spacing", &TextComponent::WordSpacing));
            UI::Property("Line Spacing", properties.Bind("Line Spacing", &TextComponent::LineSpacing));
            UI::Property("Paragraph Spacing", properties.Bind("Paragraph Spacing", &TextComponent::ParagraphSpacing));
            UI::Property("Tab Width", properties.Bind(
                                        "TabWidth", [](const TextComponent& text) { return text.TabWidth; },
                                        [](TextComponent& text, uint32_t value) { text.TabWidth = std::max(1u, value); }));
        }

        if (BeginInspectorSection("Rendering"))
        {
            UI::PropertyColor("Outline", properties.Bind("Outline", &TextComponent::OutlineColor));
            UI::Property("Outline Width",
                         properties.Bind(
                           "Outline Width", [](const TextComponent& text) { return text.Thickness; },
                           [](TextComponent& text, float value) { text.Thickness = std::max(value, 0.0f); }),
                         0.05f, 0.0f, 0.0f);
            UI::PropertyColor("Shadow Color", properties.Bind("ShadowColor", &TextComponent::ShadowColor));
            UI::Property("Shadow Offset", properties.Bind("ShadowOffset", &TextComponent::ShadowOffset), 0.05f);
            UI::Property("Shadow Softness",
                         properties.Bind(
                           "ShadowSoftness", [](const TextComponent& text) { return text.ShadowSoftness; },
                           [](TextComponent& text, float value) { text.ShadowSoftness = std::max(value, 0.0f); }),
                         0.05f, 0.0f, 0.0f);

            const SelectionPropertyValue<bool> underlineValue =
              properties.Inspect([](const TextComponent& text) { return text.FontStyle.IsSet(TextFontStyleBits::Underline); });
            const SelectionPropertyValue<bool> strikethroughValue =
              properties.Inspect([](const TextComponent& text) { return text.FontStyle.IsSet(TextFontStyleBits::Strikethrough); });
            if (underlineValue && strikethroughValue && !underlineValue.Mixed && !strikethroughValue.Mixed &&
                (*underlineValue.Primary || *strikethroughValue.Primary))
            {
                const auto customColor = properties.Bind("Custom Decoration Color", &TextComponent::UseCustomDecorationColor);
                UI::Property("Custom Decoration Color", customColor);
                {
                    const SelectionPropertyValue<bool> customColorValue = customColor.Read();
                    UI::ScopedDisable disableColor(customColorValue && !customColorValue.Mixed && !*customColorValue.Primary);
                    UI::PropertyColor("Decoration Color", properties.Bind("Decoration Color", &TextComponent::DecorationColor));
                }
                UI::Property("Decoration Thickness",
                             properties.Bind(
                               "Decoration Thickness", [](const TextComponent& text) { return text.DecorationThickness; },
                               [](TextComponent& text, float value) { text.DecorationThickness = std::max(value, 0.0f); }),
                             0.01f, 0.0f, 0.0f);
                if (*underlineValue.Primary)
                    UI::Property("Underline Offset", properties.Bind("Underline Offset", &TextComponent::UnderlineOffset));
                if (*strikethroughValue.Primary)
                    UI::Property("Strikethrough Offset", properties.Bind("Strikethrough Offset", &TextComponent::StrikethroughOffset));
            }

            UI::Property("Sorting Layer", properties.Bind("SortingLayer", &TextComponent::SortingLayer));
            UI::Property("Order In Layer", properties.Bind("OrderInLayer", &TextComponent::OrderInLayer));
        }
    }

    template <> void ComponentSelectionEditorWidget<SpriteRendererComponent>(Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Sprite Renderer").Components<SpriteRendererComponent>();
        if (entities.size() == 1u)
        {
            const AssetHandle<Texture>& texture = primary.GetComponent<SpriteRendererComponent>().Texture;
            const Ref<Texture>& preview = texture ? texture.GetInternalPtr() : EditorAssets::Get().UnassignedTexture;
            ImGui::Image(ImGuiVulkanTexture::Get(preview), { 50.0f, 50.0f }, { 0, 1 }, { 1, 0 });
        }
        UI::PropertyAsset<Texture>("Texture", properties.Bind("Texture", &SpriteRendererComponent::Texture));
        UI::PropertyColor("Color", properties.Bind("Color", &SpriteRendererComponent::Color));
        UI::Property("Sorting Layer", properties.Bind("SortingLayer", &SpriteRendererComponent::SortingLayer));
        UI::Property("Order In Layer", properties.Bind("OrderInLayer", &SpriteRendererComponent::OrderInLayer));
    }

    template <> void ComponentSelectionEditorWidget<MeshRendererComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Mesh Filter").Components<MeshRendererComponent>();
        UI::PropertyAsset<Mesh>("Mesh", properties.Bind("Mesh", &MeshRendererComponent::MeshHandle));

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
            UI::PropertyAsset<Material>(
              label.c_str(), properties.Bind(
                               "Materials", [slot](const MeshRendererComponent& mesh) { return mesh.GetMaterial(slot); },
                               [slot](MeshRendererComponent& mesh, const AssetHandle<Material>& value) { mesh.SetMaterial(slot, value); }));
        }
        UI::Property("Visible", properties.Bind("Visible", &MeshRendererComponent::Visible));
        UI::Property("Cast Shadows", properties.Bind("CastShadows", &MeshRendererComponent::CastShadows));
        UI::Property("Receive Shadows", properties.Bind("ReceiveShadows", &MeshRendererComponent::ReceiveShadows));
        UI::Property("Motion Vectors", properties.Bind("MotionVectors", &MeshRendererComponent::MotionVectors));
        UI::Property("LOD Bias",
                     properties.Bind(
                       "LodBias", [](const MeshRendererComponent& mesh) { return mesh.LodBias; },
                       [](MeshRendererComponent& mesh, float value) { mesh.LodBias = std::clamp(value, -8.0f, 8.0f); }),
                     0.05f, -8.0f, 8.0f);
        UI::Property("Render Layer Order", properties.Bind("RenderLayerOrder", &MeshRendererComponent::RenderLayerOrder));
    }

    template <> void ComponentSelectionEditorWidget<AnimationComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Animation").Components<AnimationComponent>();
        UI::PropertyAsset<AnimationClip>("Clip", properties.Bind("Clip", &AnimationComponent::GetClip, &AnimationComponent::SetClip));
        UI::Property("Speed", properties.Bind("Speed", &AnimationComponent::GetSpeed, &AnimationComponent::SetSpeed), 0.05f);
        UI::PropertyDropdown("Wrap Mode", { "Clamp", "Loop", "Ping Pong" },
                             properties.Bind("WrapMode", &AnimationComponent::GetWrapMode, &AnimationComponent::SetWrapMode));
        UI::Property("Play On Awake", properties.Bind("PlayOnAwake", &AnimationComponent::GetPlayOnAwake, &AnimationComponent::SetPlayOnAwake));
        UI::Property("Apply Root Motion",
                     properties.Bind("ApplyRootMotion", &AnimationComponent::GetApplyRootMotion, &AnimationComponent::SetApplyRootMotion));
    }

    template <> void ComponentSelectionEditorWidget<Rigidbody2DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Rigidbody 2D").Components<Rigidbody2DComponent>();
        UI::EditSelectionProperty(properties.Bind("Layer", &Rigidbody2DComponent::GetLayerMask, &Rigidbody2DComponent::SetLayerMask),
                                  [](uint32_t& value) { return UIUtils::PropertyLayer("Layer", value); });
        const auto bodyType = properties.Bind("BodyType", &Rigidbody2DComponent::GetBodyType, &Rigidbody2DComponent::SetBodyType);
        UI::PropertyDropdown("Body Type", { "Static", "Dynamic", "Kinematic" }, bodyType);

        const SelectionPropertyValue<RigidbodyBodyType> bodyTypeValue = bodyType.Read();
        if (bodyTypeValue && !bodyTypeValue.Mixed && *bodyTypeValue.Primary == RigidbodyBodyType::Dynamic)
        {
            UI::Property("Auto Mass", properties.Bind("AutoMass", &Rigidbody2DComponent::GetAutoMass, &Rigidbody2DComponent::SetAutoMass));
            UI::Property("Mass",
                         properties.Bind("Mass", &Rigidbody2DComponent::GetConfiguredMass,
                                         [](Rigidbody2DComponent& body, float value) { body.SetMass(std::max(value, 0.0001f)); }),
                         0.1f, 0.0001f, 0.0f);
            UI::Property("Gravity Scale",
                         properties.Bind("GravityScale", &Rigidbody2DComponent::GetGravityScale, &Rigidbody2DComponent::SetGravityScale));
            UI::Property("Linear Drag", properties.Bind("LinearDrag", &Rigidbody2DComponent::GetLinearDrag,
                                                        [](Rigidbody2DComponent& body, float value) { body.SetLinearDrag(std::max(value, 0.0f)); }));
            UI::Property("Angular Drag",
                         properties.Bind("AngularDrag", &Rigidbody2DComponent::GetAngularDrag,
                                         [](Rigidbody2DComponent& body, float value) { body.SetAngularDrag(std::max(value, 0.0f)); }));
        }
        if (bodyTypeValue && !bodyTypeValue.Mixed && *bodyTypeValue.Primary != RigidbodyBodyType::Static)
        {
            UI::PropertyDropdown("Collision Detection", { "Discrete", "Continuous" },
                                 properties.Bind("CollisionDetection", &Rigidbody2DComponent::GetCollisionDetectionMode,
                                                 &Rigidbody2DComponent::SetCollisionDetectionMode));
            UI::PropertyDropdown("Sleeping Mode", { "Never Sleep", "Start Awake", "Start Sleeping" },
                                 properties.Bind("SleepMode", &Rigidbody2DComponent::GetSleepMode, &Rigidbody2DComponent::SetSleepMode));
            UI::PropertyFlags("Constraints", { "Position X", "Position Y", "Rotation" },
                              { Rigidbody2DConstraintsBits::FreezePositionX, Rigidbody2DConstraintsBits::FreezePositionY,
                                Rigidbody2DConstraintsBits::FreezeRotation },
                              properties.Bind("Constraints", &Rigidbody2DComponent::GetConstraints, &Rigidbody2DComponent::SetConstraints));
        }
    }

    template <> void ComponentSelectionEditorWidget<BoxCollider2DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Box Collider 2D").Components<BoxCollider2DComponent>();
        UI::Property("Offset", properties.Bind("Offset", &BoxCollider2DComponent::GetOffset, &BoxCollider2DComponent::SetOffset), 0.05f);
        UI::Property("Size",
                     properties.Bind("Size", &BoxCollider2DComponent::GetSize,
                                     [](BoxCollider2DComponent& collider, const glm::vec2& value, Entity entity) {
                                         collider.SetSize(glm::max(value, glm::vec2(0.001f)), entity);
                                     }),
                     0.05f, 0.001f, 0.0f);
        UI::Property("Is Trigger", properties.Bind("IsTrigger", &BoxCollider2DComponent::IsTrigger, &BoxCollider2DComponent::SetIsTrigger));
        UI::PropertyAsset<PhysicsMaterial2D>("Material",
                                             properties.Bind("Material", &BoxCollider2DComponent::GetMaterial, &BoxCollider2DComponent::SetMaterial));
    }

    template <> void ComponentSelectionEditorWidget<CircleCollider2DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Circle Collider 2D").Components<CircleCollider2DComponent>();
        UI::Property("Offset", properties.Bind("Offset", &CircleCollider2DComponent::GetOffset, &CircleCollider2DComponent::SetOffset), 0.05f);
        UI::Property("Radius",
                     properties.Bind(
                       "Radius", &CircleCollider2DComponent::GetRadius,
                       [](CircleCollider2DComponent& collider, float value, Entity entity) { collider.SetRadius(std::max(value, 0.001f), entity); }),
                     0.05f, 0.001f, 0.0f);
        UI::Property("Is Trigger", properties.Bind("IsTrigger", &CircleCollider2DComponent::IsTrigger, &CircleCollider2DComponent::SetIsTrigger));
        UI::PropertyAsset<PhysicsMaterial2D>(
          "Material", properties.Bind("Material", &CircleCollider2DComponent::GetMaterial, &CircleCollider2DComponent::SetMaterial));
    }

    template <> void ComponentSelectionEditorWidget<Rigidbody3DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Rigidbody 3D").Components<Rigidbody3DComponent>();
        const auto bodyType = properties.Bind("BodyType", &Rigidbody3DComponent::GetBodyType, &Rigidbody3DComponent::SetBodyType);
        UI::PropertyDropdown("Body Type", { "Static", "Dynamic", "Kinematic" }, bodyType);

        const SelectionPropertyValue<PhysicsBodyType3D> bodyTypeValue = bodyType.Read();
        if (bodyTypeValue && !bodyTypeValue.Mixed && *bodyTypeValue.Primary == PhysicsBodyType3D::Dynamic)
        {
            UI::Property("Auto Mass", properties.Bind("AutoMass", &Rigidbody3DComponent::GetAutoMass, &Rigidbody3DComponent::SetAutoMass));
            UI::Property(
              "Mass",
              properties.Bind("Mass", &Rigidbody3DComponent::GetMass,
                              [](Rigidbody3DComponent& body, float value, Entity entity) { body.SetMass(std::max(value, 0.0001f), entity); }),
              0.1f, 0.0001f, 0.0f);
            UI::Property("Gravity Scale",
                         properties.Bind("GravityScale", &Rigidbody3DComponent::GetGravityScale, &Rigidbody3DComponent::SetGravityScale), 0.05f);
            UI::Property(
              "Linear Damping",
              properties.Bind("LinearDamping", &Rigidbody3DComponent::GetLinearDamping,
                              [](Rigidbody3DComponent& body, float value) { body.SetDamping(std::max(value, 0.0f), body.GetAngularDamping()); }),
              0.01f, 0.0f, 0.0f);
            UI::Property(
              "Angular Damping",
              properties.Bind("AngularDamping", &Rigidbody3DComponent::GetAngularDamping,
                              [](Rigidbody3DComponent& body, float value) { body.SetDamping(body.GetLinearDamping(), std::max(value, 0.0f)); }),
              0.01f, 0.0f, 0.0f);
        }
        UI::Property("Center Of Mass",
                     properties.Bind("CenterOfMass", &Rigidbody3DComponent::GetCenterOfMass, &Rigidbody3DComponent::SetCenterOfMass), 0.05f);
        UI::Property("Continuous Collision", properties.Bind("ContinuousCollision", &Rigidbody3DComponent::GetContinuousCollision,
                                                             &Rigidbody3DComponent::SetContinuousCollision));
        UI::Property("Allow Sleep", properties.Bind("AllowSleep", &Rigidbody3DComponent::GetAllowSleep, &Rigidbody3DComponent::SetAllowSleep));
        UI::Property("Start Awake", properties.Bind("StartAwake", &Rigidbody3DComponent::GetStartAwake, &Rigidbody3DComponent::SetStartAwake));
        UI::Property("Lock Rotation X", properties.Bind("LockRotationX", &Rigidbody3DComponent::GetLockRotationX,
                                                        [](Rigidbody3DComponent& body, bool value, Entity entity) {
                                                            body.SetRotationLocks(value, body.GetLockRotationY(), body.GetLockRotationZ(), entity);
                                                        }));
        UI::Property("Lock Rotation Y", properties.Bind("LockRotationY", &Rigidbody3DComponent::GetLockRotationY,
                                                        [](Rigidbody3DComponent& body, bool value, Entity entity) {
                                                            body.SetRotationLocks(body.GetLockRotationX(), value, body.GetLockRotationZ(), entity);
                                                        }));
        UI::Property("Lock Rotation Z", properties.Bind("LockRotationZ", &Rigidbody3DComponent::GetLockRotationZ,
                                                        [](Rigidbody3DComponent& body, bool value, Entity entity) {
                                                            body.SetRotationLocks(body.GetLockRotationX(), body.GetLockRotationY(), value, entity);
                                                        }));

        const auto filter = properties.Bind("Filter", &Rigidbody3DComponent::GetFilter, &Rigidbody3DComponent::SetFilter);
        UI::Property("Layer", filter.Member("Filter", &PhysicsFilter3D::Layer));
        UI::Property("Mask", filter.Member("Filter", &PhysicsFilter3D::Mask));
        UI::Property("Group", filter.Member("Filter", &PhysicsFilter3D::Group));
    }

    template <> void ComponentSelectionEditorWidget<BoxCollider3DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        DrawCollider3DProperties<BoxCollider3DComponent>(entities, "Box Collider 3D");
        const auto properties = InspectorSelection(entities, "Box Collider 3D").Components<BoxCollider3DComponent>();
        UI::Property("Size",
                     properties.Bind("Size", &BoxCollider3DComponent::GetSize,
                                     [](BoxCollider3DComponent& collider, const glm::vec3& value, Entity entity) {
                                         collider.SetSize(glm::max(value, glm::vec3(0.001f)), entity);
                                     }),
                     0.05f, 0.001f, 0.0f);
    }

    template <> void ComponentSelectionEditorWidget<SphereCollider3DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        DrawCollider3DProperties<SphereCollider3DComponent>(entities, "Sphere Collider 3D");
        const auto properties = InspectorSelection(entities, "Sphere Collider 3D").Components<SphereCollider3DComponent>();
        UI::Property("Radius",
                     properties.Bind(
                       "Radius", &SphereCollider3DComponent::GetRadius,
                       [](SphereCollider3DComponent& collider, float value, Entity entity) { collider.SetRadius(std::max(value, 0.001f), entity); }),
                     0.05f, 0.001f, 0.0f);
    }

    template <> void ComponentSelectionEditorWidget<CapsuleCollider3DComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        DrawCollider3DProperties<CapsuleCollider3DComponent>(entities, "Capsule Collider 3D");
        const auto properties = InspectorSelection(entities, "Capsule Collider 3D").Components<CapsuleCollider3DComponent>();
        UI::Property("Radius",
                     properties.Bind(
                       "Radius", &CapsuleCollider3DComponent::GetRadius,
                       [](CapsuleCollider3DComponent& collider, float value, Entity entity) { collider.SetRadius(std::max(value, 0.001f), entity); }),
                     0.05f, 0.001f, 0.0f);
        UI::Property("Height",
                     properties.Bind("Height", &CapsuleCollider3DComponent::GetHeight,
                                     [](CapsuleCollider3DComponent& collider, float value, Entity entity) {
                                         collider.SetHeight(std::max(value, collider.GetRadius() * 2.0f), entity);
                                     }),
                     0.05f, 0.001f, 0.0f);
    }

    template <> void ComponentSelectionEditorWidget<AudioListenerComponent>(CW_MAYBE_UNUSED Entity primary, CW_MAYBE_UNUSED const Vector<Entity>& entities)
    {
    }

    template <> void ComponentSelectionEditorWidget<AudioSourceComponent>(CW_MAYBE_UNUSED Entity primary, const Vector<Entity>& entities)
    {
        const auto properties = InspectorSelection(entities, "Audio Source").Components<AudioSourceComponent>();
        UI::PropertyAsset<AudioClip>("Audio Clip", properties.Bind("Clip", &AudioSourceComponent::GetClip, &AudioSourceComponent::SetClip));
        UI::PropertySlider("Volume", properties.Bind("Volume", &AudioSourceComponent::GetVolume, &AudioSourceComponent::SetVolume), 0.0f, 1.0f);
        UI::Property("Mute", properties.Bind("Mute", &AudioSourceComponent::GetIsMuted, &AudioSourceComponent::SetIsMuted));
        UI::PropertySlider("Pitch", properties.Bind("Pitch", &AudioSourceComponent::GetPitch, &AudioSourceComponent::SetPitch), -3.0f, 3.0f);
        UI::Property("Play On Awake", properties.Bind("PlayOnAwake", &AudioSourceComponent::GetPlayOnAwake, &AudioSourceComponent::SetPlayOnAwake));
        UI::Property("Loop", properties.Bind("Loop", &AudioSourceComponent::GetLooping, &AudioSourceComponent::SetLooping));
        UI::Property("Min Distance", properties.Bind("MinDistance", &AudioSourceComponent::GetMinDistance, &AudioSourceComponent::SetMinDistance));
        UI::Property("Max Distance", properties.Bind("MaxDistance", &AudioSourceComponent::GetMaxDistance, &AudioSourceComponent::SetMaxDistance));

        const AssetHandle<AudioMixer> mixer = AudioManager::TryGet()->GetActiveMixer();
        const Vector<AudioBusDesc>* busDescs = mixer ? &mixer->GetBusDescs() : nullptr;
        const size_t busOptionCount = 1u + (busDescs != nullptr ? busDescs->size() : 0u);
        UI::EditSelectionProperty(properties.Bind("Bus", &AudioSourceComponent::GetBusName, &AudioSourceComponent::SetBusName), [&](String& value) {
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
        UI::PropertySlider("Low Pass Gain",
                           properties.Bind("LowPassGain", &AudioSourceComponent::GetLowPassGain, &AudioSourceComponent::SetLowPassGain), 0.0f, 1.0f);
        UI::PropertySlider("High Pass Gain",
                           properties.Bind("HighPassGain", &AudioSourceComponent::GetHighPassGain, &AudioSourceComponent::SetHighPassGain), 0.0f,
                           1.0f);
        UI::PropertySlider("Cone Inner Angle",
                           properties.Bind("ConeInnerAngle", &AudioSourceComponent::GetConeInnerAngle, &AudioSourceComponent::SetConeInnerAngle),
                           0.0f, 360.0f);
        UI::PropertySlider("Cone Outer Angle",
                           properties.Bind("ConeOuterAngle", &AudioSourceComponent::GetConeOuterAngle, &AudioSourceComponent::SetConeOuterAngle),
                           0.0f, 360.0f);
        UI::PropertySlider("Cone Outer Gain",
                           properties.Bind("ConeOuterGain", &AudioSourceComponent::GetConeOuterGain, &AudioSourceComponent::SetConeOuterGain), 0.0f,
                           1.0f);
        UI::PropertySlider("Cone Outer Gain HF",
                           properties.Bind("ConeOuterGainHF", &AudioSourceComponent::GetConeOuterGainHF, &AudioSourceComponent::SetConeOuterGainHF),
                           0.0f, 1.0f);
    }
} // namespace Crowny
