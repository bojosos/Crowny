#include "cwpch.h"

#include "Crowny/Renderer/DecalRenderer.h"

#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/RenderSnapshot.h"
#include "Crowny/Renderer/VisibilityCulling.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    namespace
    {
        template <typename T> T Parameter(const Material& material, const char* name, T fallback)
        {
            return material.HasBinding(String(name)) ? material.GetDataParam<T>(name) : fallback;
        }

        GpuDecalMaterial CaptureMaterial(const Material& material)
        {
            GpuDecalMaterial m;
            m.Color = Parameter(material, "decalColor", glm::vec4(1));
            m.Emission = Parameter(material, "decalEmission", glm::vec4(0));
            m.Surface = { Parameter(material, "decalRoughness", 0.5f), Parameter(material, "decalMetallic", 0.0f),
                          Parameter(material, "decalAO", 1.0f), Parameter(material, "decalCoating", 1.0f) };
            m.Strengths = Parameter(material, "decalStrengths", glm::vec4(1));
            m.Strengths2 = Parameter(material, "decalStrengths2", glm::vec4(1));
            m.CorrectionTintExposure = Parameter(material, "decalCorrectionTint", glm::vec4(1));
            m.CorrectionTintExposure.w = Parameter(material, "decalExposure", 0.0f);
            m.Correction = { glm::radians(Parameter(material, "decalHue", 0.0f)), Parameter(material, "decalSaturation", 1.0f),
                             Parameter(material, "decalContrast", 1.0f), Parameter(material, "decalMaskThreshold", 0.0f) };
            m.Mask.x = Parameter(material, "decalMaskSoftness", 0.0f);
            m.Modes = { Parameter(material, "decalChannels", 1), Parameter(material, "decalColorBlend", 0),
                        Parameter(material, "decalRoughnessBlend", 0), Parameter(material, "decalEmissionBlend", 2) };
            m.Textures2.y = Parameter(material, "decalReplaceNormal", 0);
            return m;
        }
    } // namespace

    void DecalRenderer::Extract(Scene& scene, RenderSnapshot& snapshot)
    {
        auto decals = scene.GetAllEntitiesWith<DecalComponent>();
        if (decals.begin() == decals.end())
            return;
        // DFS intervals make subtree targeting independent of depth and parent count in the shader.
        UnorderedMap<UUID, glm::uvec2> intervals;
        uint32_t next = 1;
        std::function<void(Entity)> visit = [&](Entity entity) {
            if (!entity || intervals.contains(entity.GetUuid()))
                return;
            const uint32_t begin = next++;
            intervals[entity.GetUuid()] = { begin, begin + 1 };
            for (auto child : entity.GetChildren())
                visit(child);
            intervals[entity.GetUuid()].y = next;
        };
        auto entities = scene.GetAllEntitiesWith<IDComponent>();
        for (auto handle : entities)
        {
            Entity entity{ handle, &scene };
            if (!entity.GetParent())
                visit(entity);
        }
        for (auto handle : entities)
            visit(Entity{ handle, &scene });
        for (auto handle : entities)
        {
            Entity entity{ handle, &scene };
            uint32_t layers = 0;
            if (entity.HasComponent<MeshRendererComponent>())
            {
                const auto& receiver = entity.GetComponent<MeshRendererComponent>();
                layers = receiver.ReceiveDecals ? receiver.DecalLayers : 0;
            }
            else if (entity.HasComponent<ProceduralMeshComponent>())
            {
                const auto& receiver = entity.GetComponent<ProceduralMeshComponent>();
                layers = receiver.ReceiveDecals ? receiver.DecalLayers : 0;
            }
            else
                continue;
            snapshot.DecalReceivers.Acquire() = { static_cast<uint32_t>(entt::to_integral(handle)) + 1u, layers, intervals[entity.GetUuid()].x,
                                                  0xffffffffu };
        }
        std::sort(snapshot.DecalReceivers.begin(), snapshot.DecalReceivers.end(), [](const auto& a, const auto& b) { return a.x < b.x; });
        const auto frustum = VisibilityFrustum::FromViewProjection(snapshot.ProjectionMatrix * snapshot.ViewMatrix);
        for (auto handle : decals)
        {
            Entity entity{ handle, &scene };
            const auto& s = entity.GetComponent<DecalComponent>();
            if (!s.Enabled || !s.Material || s.Material->GetDomain() != MaterialDomain::Decal)
                continue;
            const glm::mat4 world = entity.GetComponent<TransformComponent>().GetWorldMatrix(entity.GetParent());
            if (!DecalMath::IsValid(s, world))
                continue;
            const auto bounds = DecalMath::Bounds(s, world);
            if (!frustum.IntersectsSphere(glm::vec3(bounds), bounds.w))
                continue;
            glm::uvec2 target(0, 0xffffffffu);
            if (s.TargetMode != DecalTargetMode::Layers)
            {
                auto found = intervals.find(s.Target);
                if (found == intervals.end())
                    continue;
                target = found->second;
                if (s.TargetMode == DecalTargetMode::Entity)
                    target.y = target.x + 1;
            }
            auto& decal = snapshot.Decals.Acquire();
            decal = {};
            decal.Id = entity.GetUuid();
            decal.SortOrder = s.SortOrder;
            decal.MaterialRevision = s.Material->GetParamVersion();
            decal.Bounds = bounds;
            auto& d = decal.Data;
            d.WorldToLocal = glm::inverse(world);
            d.SizeProjection = { s.Size, static_cast<float>(s.Projection) };
            d.Cylinder = { s.BottomRadius, s.TopRadius, s.Height, s.ShellThickness };
            d.OffsetArc = { s.Offset, glm::radians(s.Arc) };
            d.Fades = { std::max(s.EdgeFeather, 0.0f), std::max(s.DepthFeather, 0.0f), std::cos(glm::radians(s.AngleFadeStart)),
                        std::cos(glm::radians(s.AngleFadeEnd)) };
            d.Tint = s.Tint;
            d.Tint.a *= s.Opacity * DecalMath::LifetimeOpacity(s, s.Age);
            d.UV = { s.UVScale, s.UVOffset };
            d.Controls = { glm::radians(s.UVRotation), glm::radians(s.SeamRotation), s.DistanceFadeStart, s.DistanceFadeEnd };
            d.Metadata = { s.ReceiverLayers, target, 0 };
            decal.Material = CaptureMaterial(*s.Material);
            for (uint32_t slot = 0; slot < 5; ++slot)
                decal.Textures[slot] = s.Material->GetTexture(0, slot);
        }
        std::sort(snapshot.Decals.begin(), snapshot.Decals.end(),
                  [](const auto& a, const auto& b) { return a.SortOrder != b.SortOrder ? a.SortOrder < b.SortOrder : a.Id < b.Id; });
    }

    template <typename T> void DecalRenderer::Upload(Ref<GenericGpuBuffer>& buffer, Vector<T>& previous, const Vector<T>& data)
    {
        const uint32_t bytes = static_cast<uint32_t>(data.size() * sizeof(T));
        if (!buffer || buffer->GetBufferSize() < std::max(bytes, uint32_t(sizeof(T))))
        {
            GenericGpuBufferDesc desc;
            desc.ElementCount = std::max(static_cast<uint32_t>(data.size()), 1u);
            desc.ElementSize = sizeof(T);
            desc.Type = GpuBufferType::Structured;
            desc.Usage = BufferUsage::BU_DYNAMIC_DRAW;
            buffer = GenericGpuBuffer::Create(desc);
            previous.clear();
        }
        if (bytes && (previous.size() != data.size() || std::memcmp(previous.data(), data.data(), bytes) != 0))
        {
            buffer->WriteData(0, bytes, data.data(), BWT_DISCARD);
            m_Stats.UploadedBytes += bytes;
            previous = data;
        }
        if (data.empty())
            previous.clear();
    }

    void DecalRenderer::Prepare(const RenderSnapshot& snapshot, GpuScene* gpuScene)
    {
        m_Stats = {};
        uint64_t revision = 14695981039346656037ull;
        const auto hashBytes = [&](const void* data, size_t size) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i)
                revision = (revision ^ bytes[i]) * 1099511628211ull;
        };
        for (const auto& source : snapshot.Decals)
        {
            hashBytes(&source.Data, sizeof(source.Data));
            hashBytes(&source.Material, sizeof(source.Material));
            hashBytes(&source.MaterialRevision, sizeof(source.MaterialRevision));
            if (source.Data.Controls.w > source.Data.Controls.z)
                hashBytes(&snapshot.CameraPosition, sizeof(snapshot.CameraPosition));
            for (const auto& texture : source.Textures)
            {
                auto* identity = texture.get();
                hashBytes(&identity, sizeof(identity));
            }
        }
        if (!snapshot.Decals.Empty())
            hashBytes(snapshot.DecalReceivers.begin(), snapshot.DecalReceivers.Size() * sizeof(glm::uvec4));
        for (uint64_t released : snapshot.ReleasedHistoryNamespaces)
            m_ViewRevisions.erase(released);
        auto [history, inserted] = m_ViewRevisions.try_emplace(snapshot.HistoryNamespace);
        auto& previousView = history->second;
        m_HistoryChanged = inserted || previousView.Revision != revision;
        m_HistoryInvalidation = { 1, 1, 0, 0 };
        const auto includeBounds = [&](const glm::vec4& sphere, const glm::mat4& viewProjection) {
            for (uint32_t corner = 0; corner < 8; ++corner)
            {
                const glm::vec3 p = glm::vec3(sphere) + sphere.w * glm::vec3(corner & 1 ? 1 : -1, corner & 2 ? 1 : -1, corner & 4 ? 1 : -1);
                const glm::vec4 clip = viewProjection * glm::vec4(p, 1);
                if (clip.w <= 0.00001f)
                {
                    m_HistoryInvalidation = { 0, 0, 1, 1 };
                    return;
                }
                const glm::vec2 uv = glm::vec2(clip) / clip.w * 0.5f + 0.5f;
                m_HistoryInvalidation = { glm::min(glm::vec2(m_HistoryInvalidation), uv),
                                          glm::max(glm::vec2(m_HistoryInvalidation.z, m_HistoryInvalidation.w), uv) };
            }
        };
        if (m_HistoryChanged)
        {
            const glm::mat4 currentViewProjection = snapshot.ProjectionMatrix * snapshot.ViewMatrix;
            for (const auto& sphere : previousView.Bounds)
            {
                includeBounds(sphere, snapshot.PreviousViewProjection);
                includeBounds(sphere, currentViewProjection);
            }
            for (const auto& decal : snapshot.Decals)
                includeBounds(decal.Bounds, currentViewProjection);
            const glm::vec2 padding =
              snapshot.Target ? 2.0f / glm::vec2(snapshot.Target->GetProperties().Width, snapshot.Target->GetProperties().Height) : glm::vec2(0);
            m_HistoryInvalidation += glm::vec4(-padding, padding);
        }
        if (previousView.DebugView != snapshot.PipelineSettings.DecalDebugView)
            m_HistoryInvalidation = { 0, 0, 1, 1 };
        previousView.Revision = revision;
        previousView.DebugView = snapshot.PipelineSettings.DecalDebugView;
        previousView.Bounds.clear();
        for (const auto& decal : snapshot.Decals)
            previousView.Bounds.push_back(decal.Bounds);
        Vector<GpuDecalData> decals;
        Vector<glm::vec4> bounds;
        Vector<GpuDecalMaterial> materials;
        Vector<glm::uvec4> receivers(snapshot.DecalReceivers.begin(), snapshot.DecalReceivers.end());
        m_Textures.clear();
        m_Textures.push_back(Texture::WHITE);
        for (const auto& source : snapshot.Decals)
        {
            auto material = source.Material;
            const size_t textureCountBefore = m_Textures.size();
            uint32_t indices[5]{};
            bool admitted = true;
            for (uint32_t slot = 0; slot < 5; ++slot)
            {
                const Ref<Texture> texture = source.Textures[slot] ? source.Textures[slot] : slot == 1 ? Texture::NORMAL : Texture::WHITE;
                auto found = std::find(m_Textures.begin(), m_Textures.end(), texture);
                if (found == m_Textures.end())
                {
                    if (!gpuScene && m_Textures.size() == 256)
                    {
                        admitted = false;
                        break;
                    }
                    indices[slot] = static_cast<uint32_t>(m_Textures.size());
                    m_Textures.push_back(texture);
                }
                else
                    indices[slot] = static_cast<uint32_t>(found - m_Textures.begin());
            }
            if (!admitted)
            {
                m_Textures.resize(textureCountBefore);
                ++m_Stats.RejectedMaterials;
                continue;
            }
            material.Textures = { indices[0], indices[1], indices[2], indices[3] };
            material.Textures2.x = indices[4];
            auto data = source.Data;
            data.Metadata.w = static_cast<uint32_t>(materials.size());
            decals.push_back(data);
            bounds.push_back(source.Bounds);
            materials.push_back(material);
        }
        if (gpuScene)
        {
            Vector<uint32_t> descriptorIndices;
            gpuScene->SetDecalTextures(m_Textures, descriptorIndices);
            size_t admitted = 0;
            for (size_t i = 0; i < decals.size(); ++i)
            {
                auto material = materials[i];
                bool valid = true;
                for (int channel = 0; channel < 4; ++channel)
                {
                    material.Textures[channel] = descriptorIndices[material.Textures[channel]];
                    valid &= material.Textures[channel] != UINT32_MAX;
                }
                material.Textures2.x = descriptorIndices[material.Textures2.x];
                valid &= material.Textures2.x != UINT32_MAX;
                if (!valid)
                {
                    ++m_Stats.RejectedMaterials;
                    continue;
                }
                decals[admitted] = decals[i];
                decals[admitted].Metadata.w = static_cast<uint32_t>(admitted);
                bounds[admitted] = bounds[i];
                materials[admitted++] = material;
            }
            decals.resize(admitted);
            bounds.resize(admitted);
            materials.resize(admitted);
        }
        m_Counts = { decals.size(), receivers.size(), 0, 0 };
        m_Stats.Visible = static_cast<uint32_t>(decals.size());
        m_Stats.TextureCount = static_cast<uint32_t>(m_Textures.size());
        const uint32_t width = snapshot.Target ? snapshot.Target->GetProperties().Width : 1u;
        const uint32_t height = snapshot.Target ? snapshot.Target->GetProperties().Height : 1u;
        const auto gridDesc = ClusteredLightBuilder::ResolveDesc(snapshot.PipelineSettings, width, height);
        m_GridConstants.ViewProjection = snapshot.ProjectionMatrix * snapshot.ViewMatrix;
        m_GridConstants.CameraPosition = glm::vec4(snapshot.CameraPosition, float(snapshot.PipelineSettings.DecalDebugView));
        m_GridConstants.DepthRow =
          -glm::vec4(snapshot.ViewMatrix[0][2], snapshot.ViewMatrix[1][2], snapshot.ViewMatrix[2][2], snapshot.ViewMatrix[3][2]);
        m_GridConstants.DepthViewport = { gridDesc.NearPlane, gridDesc.FarPlane, float(width), float(height) };
        if (!decals.empty())
            m_Grid.Build(gridDesc, snapshot.ViewMatrix, snapshot.ProjectionMatrix, bounds);
        else
        {
            m_Grid.Cells.clear();
            m_Grid.Indices.clear();
            m_Grid.Overflow = 0;
        }
        m_GridConstants.Dimensions = { m_Grid.Dimensions, gridDesc.TileSize };
        m_Counts.z = static_cast<uint32_t>(m_Grid.Cells.size());
        m_Stats.OverflowClusters = m_Grid.Overflow;
        if (gpuScene && !decals.empty() &&
            m_GpuGrid.Build(gridDesc, snapshot.ViewMatrix, snapshot.ProjectionMatrix, bounds, static_cast<uint32_t>(m_Grid.Indices.size())))
        {
            m_Cells = m_GpuGrid.GetCells();
            m_Indices = m_GpuGrid.GetIndices();
            m_Stats.GpuBuiltLists = true;
            if (snapshot.ValidateDecalLists && !m_GpuGrid.MatchesReference(m_Grid.Cells, m_Grid.Indices))
            {
                ++m_Stats.ListValidationFailures;
                CW_ENGINE_ERROR("GPU decal lists differ from the CPU reference.");
            }
            m_Stats.UploadedBytes += bounds.size() * sizeof(glm::vec4);
            // Clear CPU upload mirrors so a later compatibility fallback uploads its own ordering.
            m_PreviousCells.clear();
            m_PreviousIndices.clear();
        }
        else
        {
            Upload(m_Cells, m_PreviousCells, m_Grid.Cells);
            Upload(m_Indices, m_PreviousIndices, m_Grid.Indices);
        }
        Upload(m_Decals, m_PreviousDecals, decals);
        Upload(m_Materials, m_PreviousMaterials, materials);
        Upload(m_Receivers, m_PreviousReceivers, receivers);
        if (m_Stats.RejectedMaterials)
            CW_ENGINE_WARN("Decal texture capacity exceeded: {} decals rejected", m_Stats.RejectedMaterials);
    }

    void DecalRenderer::Bind(GraphicsMaterial& material)
    {
        material.WriteUniformBlock(2, 0, &m_Counts, sizeof(m_Counts));
        material.SetBuffer(2, 1, m_Decals);
        material.SetBuffer(2, 2, m_Materials);
        material.SetBuffer(2, 4, m_Receivers);
        material.WriteUniformBlock(2, 7, &m_GridConstants, sizeof(m_GridConstants));
        material.SetBuffer(2, 8, m_Cells);
        material.SetBuffer(2, 9, m_Indices);
    }

    bool DecalRenderer::HasCoatings() const
    {
        return std::any_of(m_PreviousMaterials.begin(), m_PreviousMaterials.end(), [](const auto& material) {
            return (material.Modes.x & 128u) != 0 && material.Surface.w > 0 && material.Strengths2.w > 0;
        });
    }

    void DecalRenderer::BindCoatingMode(GraphicsMaterial& material, uint32_t mode) const
    {
        glm::uvec4 counts = m_Counts;
        counts.w = mode;
        material.WriteUniformBlock(2, 0, &counts, sizeof(counts));
    }

    void DecalRenderer::PrepareCompatibility(const RenderSnapshot& snapshot)
    {
        Prepare(snapshot);
        if (!m_Atlas.Update(m_Textures))
        {
            m_Stats.RejectedMaterials += m_Counts.x;
            m_Counts.x = 0;
            m_Stats.Visible = 0;
        }
    }

    void DecalRenderer::BindCompatibility(Material& material, uint32_t objectId, uint32_t coatingMode)
    {
        for (uint32_t pass = 0; pass < material.GetPassCount(); ++pass)
        {
            const auto pipeline = material.GetGraphicsPipeline(pass);
            if (!pipeline)
                continue;
            auto uniforms = material.GetUniformParams(pass);
            auto info = pipeline->GetParamInfo();
            if (!info->HasBinding(UniformParamInfo::ParamType::Buffer, 2, 1))
                continue;
            const auto write = [&](uint32_t slot, const glm::uvec4& data) {
                auto block = uniforms->GetUniformBlockBuffer(2, slot);
                if (block)
                {
                    block->Write(0, &data, sizeof(data));
                    block->FlushToGpu();
                }
            };
            glm::uvec4 counts = m_Counts;
            counts.w = coatingMode;
            write(0, counts);
            write(6, glm::uvec4(objectId, material.GetDecalResponseMask(), 0u, 0u));
            uniforms->SetBuffer(2, 1, m_Decals);
            uniforms->SetBuffer(2, 2, m_Materials);
            uniforms->SetBuffer(2, 4, m_Receivers);
            uniforms->SetTexture(2, 3, m_Atlas.GetTexture());
            uniforms->SetBuffer(2, 5, m_Atlas.GetRects());
            auto gridBlock = uniforms->GetUniformBlockBuffer(2, 7);
            if (gridBlock)
            {
                gridBlock->Write(0, &m_GridConstants, sizeof(m_GridConstants));
                gridBlock->FlushToGpu();
            }
            uniforms->SetBuffer(2, 8, m_Cells);
            uniforms->SetBuffer(2, 9, m_Indices);
        }
    }
} // namespace Crowny
