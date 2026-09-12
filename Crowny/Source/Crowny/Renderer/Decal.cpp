#include "cwpch.h"

#include "Crowny/Renderer/Decal.h"

#include <glm/gtc/constants.hpp>

namespace Crowny
{
    namespace
    {
        float Feather(float distance, float width) { return width > 0.0f ? glm::smoothstep(0.0f, width, distance) : 1.0f; }
        template <typename T> T Combine(T base, T value, DecalBlend blend, float weight)
        {
            return glm::mix(base, blend == DecalBlend::Multiply ? base * value : blend == DecalBlend::Add ? base + value : value, weight);
        }
    } // namespace

    void DecalClusterGrid::Build(const ClusteredLightGridDesc& desc, const glm::mat4& view, const glm::mat4& projection,
                                 const Vector<glm::vec4>& spheres)
    {
        Dimensions = ClusteredLightBuilder::GetDimensions(desc);
        Cells.assign(size_t(Dimensions.x) * Dimensions.y * Dimensions.z, glm::uvec2(0));
        Indices.clear();
        Overflow = 0;
        struct Range
        {
            glm::uvec3 Min{ 0 }, Max{ 0 };
            bool Valid = false;
        };
        Vector<Range> ranges(spheres.size());
        const auto each = [&](const Range& range, auto&& visit) {
            if (!range.Valid)
                return;
            for (uint32_t z = range.Min.z; z <= range.Max.z; ++z)
                for (uint32_t y = range.Min.y; y <= range.Max.y; ++y)
                    for (uint32_t x = range.Min.x; x <= range.Max.x; ++x)
                        visit(ClusteredLightBuilder::Flatten(x, y, z, Dimensions));
        };
        for (size_t i = 0; i < spheres.size(); ++i)
        {
            const glm::vec3 center = glm::vec3(view * glm::vec4(glm::vec3(spheres[i]), 1));
            const float radius = spheres[i].w;
            if (-center.z + radius < desc.NearPlane || -center.z - radius > desc.FarPlane)
                continue;
            glm::vec2 low(-1), high(1);
            if (-center.z - radius > desc.NearPlane)
            {
                low = glm::vec2(std::numeric_limits<float>::max());
                high = -low;
                // Project the containing view-space cube: conservative even for off-axis and orthographic views.
                for (uint32_t corner = 0; corner < 8; ++corner)
                {
                    const glm::vec3 p = center + radius * glm::vec3(corner & 1 ? 1 : -1, corner & 2 ? 1 : -1, corner & 4 ? 1 : -1);
                    const glm::vec4 clip = projection * glm::vec4(p, 1);
                    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
                    low = glm::min(low, ndc);
                    high = glm::max(high, ndc);
                }
            }
            if (low.x > 1 || low.y > 1 || high.x < -1 || high.y < -1)
                continue;
            const auto tile = [&](glm::vec2 ndc) {
                const glm::vec2 pixel =
                  (glm::clamp(ndc, glm::vec2(-1), glm::vec2(1)) * 0.5f + 0.5f) * glm::vec2(desc.ViewportWidth, desc.ViewportHeight);
                return glm::min(glm::uvec2(pixel) / std::max(desc.TileSize, 1u), glm::uvec2(Dimensions) - 1u);
            };
            auto& range = ranges[i];
            range.Min = { tile(low), ClusteredLightBuilder::DepthToSlice(std::max(-center.z - radius, desc.NearPlane), desc.NearPlane, desc.FarPlane,
                                                                         Dimensions.z) };
            range.Max = { tile(high), ClusteredLightBuilder::DepthToSlice(std::min(-center.z + radius, desc.FarPlane), desc.NearPlane, desc.FarPlane,
                                                                          Dimensions.z) };
            range.Valid = true;
            each(range, [&](uint32_t cell) { Cells[cell].y = std::min(Cells[cell].y + 1u, 65u); });
        }
        uint32_t total = 0;
        for (auto& cell : Cells)
        {
            cell.x = total;
            if (cell.y > 64)
            {
                cell.y = 0xffffffffu;
                ++Overflow;
            }
            else
                total += cell.y;
        }
        Indices.resize(total);
        Vector<uint32_t> cursors(Cells.size(), 0);
        for (uint32_t i = 0; i < ranges.size(); ++i)
            each(ranges[i], [&](uint32_t cell) {
                if (Cells[cell].y != 0xffffffffu)
                    Indices[Cells[cell].x + cursors[cell]++] = i;
            });
    }

    bool DecalMath::IsValid(const DecalSettings& s, const glm::mat4& world)
    {
        bool finite = true;
        DecalSettings copy = s;
        copy.Visit([&](const char*, const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, float>)
                finite &= std::isfinite(value);
            else if constexpr (std::is_same_v<T, glm::vec2> || std::is_same_v<T, glm::vec3> || std::is_same_v<T, glm::vec4>)
                for (int i = 0; i < value.length(); ++i)
                    finite &= std::isfinite(value[i]);
        });
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                finite &= std::isfinite(world[c][r]);
        if (!finite || glm::determinant(world) == 0.0f || s.AngleFadeStart < 0.0f || s.AngleFadeEnd < s.AngleFadeStart || s.AngleFadeEnd > 180.0f)
            return false;
        const glm::mat4 inverse = glm::inverse(world);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(inverse[c][r]))
                    return false;
        if (s.Projection == DecalProjection::Box)
            return glm::all(glm::greaterThan(s.Size, glm::vec3(0.0f)));
        return s.Projection == DecalProjection::Cylinder && s.Height > 0.0f && s.BottomRadius > 0.0f && s.TopRadius > 0.0f &&
               s.ShellThickness > 0.0f && s.Arc > 0.0f && s.Arc <= 360.0f;
    }

    DecalProjectionSample DecalMath::Project(const DecalSettings& s, const glm::mat4& world, const glm::vec3& position,
                                             const glm::vec3& geometricNormal, float cameraDistance)
    {
        DecalProjectionSample result;
        if (!s.Enabled || !IsValid(s, world))
            return result;
        const glm::mat4 inverse = glm::inverse(world);
        const glm::vec3 p = glm::vec3(inverse * glm::vec4(position, 1.0f)) - s.Offset;
        glm::vec3 normal(0, 0, 1);
        float coverage = 1.0f;
        if (s.Projection == DecalProjection::Box)
        {
            const glm::vec3 remaining = s.Size * 0.5f - glm::abs(p);
            if (glm::any(glm::lessThan(remaining, glm::vec3(0.0f))))
                return result;
            result.UV = glm::vec2(p) / glm::vec2(s.Size) + 0.5f;
            coverage = Feather(std::min(remaining.x, remaining.y), s.EdgeFeather) * Feather(remaining.z, s.DepthFeather);
        }
        else
        {
            const float v = p.y / s.Height + 0.5f;
            const float radius = glm::length(glm::vec2(p.x, p.z));
            if (v < 0.0f || v > 1.0f || radius < 1e-7f)
                return result;
            const float expected = glm::mix(s.BottomRadius, s.TopRadius, v);
            const float radialRemaining = s.ShellThickness * 0.5f - std::abs(radius - expected);
            const float angle = glm::mod(std::atan2(p.x, p.z) - glm::radians(s.SeamRotation), glm::two_pi<float>());
            const float arc = glm::radians(s.Arc);
            if (radialRemaining < 0.0f || angle > arc)
                return result;
            result.UV = { angle / arc, v };
            normal = glm::normalize(glm::vec3(p.x / radius, -(s.TopRadius - s.BottomRadius) / s.Height, p.z / radius));
            coverage = Feather(std::min(v, 1.0f - v) * s.Height, s.EdgeFeather) * Feather(radialRemaining, s.DepthFeather);
            if (s.Arc < 360.0f)
                coverage *= Feather(std::min(angle, arc - angle) * expected, s.EdgeFeather);
        }
        result.Normal = glm::normalize(glm::transpose(glm::mat3(inverse)) * normal);
        const float facing = glm::dot(result.Normal, glm::normalize(geometricNormal));
        const float start = std::cos(glm::radians(std::clamp(s.AngleFadeStart, 0.0f, 180.0f)));
        const float end = std::cos(glm::radians(std::clamp(s.AngleFadeEnd, s.AngleFadeStart, 180.0f)));
        coverage *= start > end ? glm::smoothstep(end, start, facing) : (facing >= start ? 1.0f : 0.0f);
        if (s.DistanceFadeEnd > s.DistanceFadeStart)
            coverage *= 1.0f - glm::smoothstep(s.DistanceFadeStart, s.DistanceFadeEnd, cameraDistance);
        const float angle = glm::radians(s.UVRotation);
        const glm::vec2 uv = (result.UV - 0.5f) * s.UVScale;
        result.UV = glm::mat2(std::cos(angle), std::sin(angle), -std::sin(angle), std::cos(angle)) * uv + 0.5f + s.UVOffset;
        result.Coverage = coverage * std::clamp(s.Opacity * s.Tint.a, 0.0f, 1.0f);
        return result;
    }

    float DecalMath::LifetimeOpacity(const DecalSettings& s, float age)
    {
        if (!s.Enabled || !std::isfinite(age))
            return 0.0f;
        age = std::max(age, 0.0f);
        float opacity = s.FadeIn > 0.0f ? std::clamp(age / s.FadeIn, 0.0f, 1.0f) : 1.0f;
        if (s.Lifetime > 0.0f && age >= s.Lifetime)
            opacity *= s.FadeOut > 0.0f ? std::clamp(1.0f - (age - s.Lifetime) / s.FadeOut, 0.0f, 1.0f) : 0.0f;
        return opacity;
    }

    glm::vec3 DecalMath::CorrectColor(const glm::vec3& color, const DecalMaterialDesc& m)
    {
        if (m.CorrectionTint == glm::vec3(1) && m.Exposure == 0 && m.Hue == 0 && m.Saturation == 1 && m.Contrast == 1)
            return color;
        glm::vec3 c = color * m.CorrectionTint * std::exp2(m.Exposure);
        // Rotate chroma around neutral gray, then restore Rec.709 luminance.
        const glm::vec3 luma(0.2126f, 0.7152f, 0.0722f);
        const float y = glm::dot(c, luma);
        const glm::vec3 axis = glm::normalize(glm::vec3(1.0f));
        const float angle = glm::radians(m.Hue);
        c = c * std::cos(angle) + glm::cross(axis, c) * std::sin(angle) + axis * glm::dot(axis, c) * (1.0f - std::cos(angle));
        c += glm::vec3(y - glm::dot(c, luma));
        c = glm::mix(glm::vec3(y), c, m.Saturation);
        return glm::max((c - 0.18f) * m.Contrast + 0.18f, glm::vec3(0.0f));
    }

    void DecalMath::Blend(DecalSurface& s, const DecalMaterialDesc& m, float coverage, uint32_t responseMask)
    {
        const uint32_t mask = m.Channels & responseMask;
        const float a = std::clamp(coverage * m.Color.a, 0.0f, 1.0f);
        if (a <= 0.0f || mask == 0)
            return;
        const auto weight = [&](float strength) { return a * std::clamp(strength, 0.0f, 1.0f); };
        if (mask & 64u)
            s.Color = glm::mix(s.Color, CorrectColor(s.Color, m), weight(m.Strengths2.z));
        if (mask & 1u)
            s.Color = Combine(s.Color, glm::vec3(m.Color), m.ColorBlend, weight(m.Strengths.x));
        if ((mask & 2u) && weight(m.Strengths.y) > 0.0f)
        {
            const glm::vec3 geometric = glm::normalize(m.GeometricNormal);
            const glm::vec3 previous = geometric - s.Normal / std::max(glm::dot(s.Normal, geometric), 0.05f);
            const glm::vec3 gradient =
              m.ReplaceNormal ? glm::mix(previous, m.NormalGradient, weight(m.Strengths.y)) : previous + m.NormalGradient * weight(m.Strengths.y);
            s.Normal = glm::normalize(geometric - gradient);
        }
        if ((mask & 4u) && weight(m.Strengths.z) > 0.0f)
            s.Roughness =
              glm::mix(s.Roughness, std::clamp(Combine(s.Roughness, m.Roughness, m.RoughnessBlend, 1.0f), 0.0f, 1.0f), weight(m.Strengths.z));
        if ((mask & 8u) && weight(m.Strengths.w) > 0.0f)
            s.Metallic = glm::mix(s.Metallic, std::clamp(m.Metallic, 0.0f, 1.0f), weight(m.Strengths.w));
        if (mask & 16u)
            s.AmbientOcclusion *= glm::mix(1.0f, m.AmbientOcclusion, weight(m.Strengths2.x));
        if (mask & 32u)
            s.Emission = Combine(s.Emission, m.Emission * m.EmissionIntensity, m.EmissionBlend, weight(m.Strengths2.y));
        if (mask & 128u)
            s.Opacity += weight(m.Strengths2.w) * std::clamp(m.CoatingOpacity, 0.0f, 1.0f) * (1.0f - s.Opacity);
    }

    glm::vec4 DecalMath::Bounds(const DecalSettings& s, const glm::mat4& world)
    {
        glm::vec3 extent = s.Size * 0.5f;
        if (s.Projection == DecalProjection::Cylinder)
        {
            const float radius = std::max(s.BottomRadius, s.TopRadius) + s.ShellThickness * 0.5f;
            extent = { radius, s.Height * 0.5f, radius };
        }
        glm::vec3 transformed(0.0f);
        for (int i = 0; i < 3; ++i)
            transformed += glm::abs(glm::vec3(world[i])) * extent[i];
        return { glm::vec3(world * glm::vec4(s.Offset, 1.0f)), glm::length(transformed) };
    }
} // namespace Crowny
