#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"

#include <glm/glm.hpp>

namespace Crowny
{
    enum class DecalProjection : uint32_t
    {
        Box,
        Cylinder
    };
    enum class DecalTargetMode : uint32_t
    {
        Layers,
        Entity,
        Subtree
    };
    enum class DecalBlend : uint32_t
    {
        Replace,
        Multiply,
        Add
    };
    enum class DecalChannel : uint32_t
    {
        Color = 1u,
        Normal = 2u,
        Roughness = 4u,
        Metallic = 8u,
        AmbientOcclusion = 16u,
        Emission = 32u,
        Corrections = 64u,
        Coating = 128u
    };

    struct DecalSettings
    {
        DecalProjection Projection = DecalProjection::Box;
        glm::vec3 Offset{ 0.0f };
        glm::vec3 Size{ 1.0f, 1.0f, 0.2f };
        float BottomRadius = 0.5f;
        float TopRadius = 0.5f;
        float Height = 1.0f;
        float ShellThickness = 0.1f;
        float Arc = 360.0f;
        float SeamRotation = 0.0f;
        glm::vec4 Tint{ 1.0f };
        float Opacity = 1.0f;
        glm::vec2 UVOffset{ 0.0f };
        glm::vec2 UVScale{ 1.0f };
        float UVRotation = 0.0f;
        bool PreserveTexelDensity = false;
        int32_t SortOrder = 0;
        uint32_t ReceiverLayers = 0xffffffffu;
        DecalTargetMode TargetMode = DecalTargetMode::Layers;
        UUID Target;
        float EdgeFeather = 0.0f;
        float DepthFeather = 0.0f;
        float AngleFadeStart = 60.0f;
        float AngleFadeEnd = 85.0f;
        float DistanceFadeStart = 0.0f;
        float DistanceFadeEnd = 0.0f;
        bool Enabled = true;
        float FadeIn = 0.0f;
        float Lifetime = 0.0f;
        float FadeOut = 0.0f;
        bool DestroyOwnerOnExpiry = false;
        bool operator==(const DecalSettings&) const = default;

        // Shared by serialization and property tooling. Runtime state is deliberately excluded.
        template <typename Visitor> void Visit(Visitor&& visitor)
        {
            visitor("Projection", Projection);
            visitor("Offset", Offset);
            visitor("Size", Size);
            visitor("BottomRadius", BottomRadius);
            visitor("TopRadius", TopRadius);
            visitor("Height", Height);
            visitor("ShellThickness", ShellThickness);
            visitor("Arc", Arc);
            visitor("SeamRotation", SeamRotation);
            visitor("Tint", Tint);
            visitor("Opacity", Opacity);
            visitor("UVOffset", UVOffset);
            visitor("UVScale", UVScale);
            visitor("UVRotation", UVRotation);
            visitor("PreserveTexelDensity", PreserveTexelDensity);
            visitor("SortOrder", SortOrder);
            visitor("ReceiverLayers", ReceiverLayers);
            visitor("TargetMode", TargetMode);
            visitor("Target", Target);
            visitor("EdgeFeather", EdgeFeather);
            visitor("DepthFeather", DepthFeather);
            visitor("AngleFadeStart", AngleFadeStart);
            visitor("AngleFadeEnd", AngleFadeEnd);
            visitor("DistanceFadeStart", DistanceFadeStart);
            visitor("DistanceFadeEnd", DistanceFadeEnd);
            visitor("Enabled", Enabled);
            visitor("FadeIn", FadeIn);
            visitor("Lifetime", Lifetime);
            visitor("FadeOut", FadeOut);
            visitor("DestroyOwnerOnExpiry", DestroyOwnerOnExpiry);
        }
    };

    struct DecalClusterGrid
    {
        glm::uvec3 Dimensions{ 1 };
        Vector<glm::uvec2> Cells;
        Vector<uint32_t> Indices;
        uint32_t Overflow = 0;
        // Input spheres must already be in overlap order. A count of UINT32_MAX selects the complete list.
        void Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection, const Vector<glm::vec4>& spheres);
    };

    struct DecalProjectionSample
    {
        glm::vec2 UV{ 0.0f };
        glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
        float Coverage = 0.0f;
    };

    struct DecalSurface
    {
        glm::vec3 Color{ 1.0f };
        glm::vec3 Normal{ 0.0f, 0.0f, 1.0f };
        glm::vec3 Emission{ 0.0f };
        float Roughness = 0.5f;
        float Metallic = 0.0f;
        float AmbientOcclusion = 1.0f;
        float Opacity = 1.0f;
    };

    struct DecalMaterialDesc
    {
        uint32_t Channels = static_cast<uint32_t>(DecalChannel::Color);
        DecalBlend ColorBlend = DecalBlend::Replace;
        DecalBlend RoughnessBlend = DecalBlend::Replace;
        DecalBlend EmissionBlend = DecalBlend::Add;
        bool ReplaceNormal = false;
        // World-space surface gradient after projecting the sampled normal map.
        glm::vec3 NormalGradient{ 0.0f };
        glm::vec3 GeometricNormal{ 0.0f, 0.0f, 1.0f };
        glm::vec4 Color{ 1.0f };
        glm::vec3 Emission{ 0.0f };
        float EmissionIntensity = 1.0f;
        float Roughness = 0.5f;
        float Metallic = 0.0f;
        float AmbientOcclusion = 1.0f;
        float CoatingOpacity = 1.0f;
        glm::vec4 Strengths{ 1.0f };  // color, normal, roughness, metallic
        glm::vec4 Strengths2{ 1.0f }; // AO, emission, corrections, coating
        glm::vec3 CorrectionTint{ 1.0f };
        float Exposure = 0.0f;
        float Hue = 0.0f;
        float Saturation = 1.0f;
        float Contrast = 1.0f;
        float MaskThreshold = 0.0f;
        float MaskSoftness = 0.0f;
    };

    class DecalMath
    {
    public:
        static bool IsValid(const DecalSettings& settings, const glm::mat4& world);
        static DecalProjectionSample Project(const DecalSettings& settings, const glm::mat4& world, const glm::vec3& position,
                                             const glm::vec3& geometricNormal, float cameraDistance = 0.0f);
        static float LifetimeOpacity(const DecalSettings& settings, float age);
        static glm::vec3 CorrectColor(const glm::vec3& color, const DecalMaterialDesc& material);
        static void Blend(DecalSurface& surface, const DecalMaterialDesc& material, float coverage, uint32_t responseMask = 0xffffffffu);
        static glm::vec4 Bounds(const DecalSettings& settings, const glm::mat4& world);
    };
} // namespace Crowny
