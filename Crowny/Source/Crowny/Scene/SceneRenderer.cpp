#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/AccelerationStructure.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"
#include "Crowny/Renderer/ComputeMaterial.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/GpuMaterial.h"
#include "Crowny/Renderer/GpuScene.h"
#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderGraphResources.h"
#include "Crowny/Renderer/RenderPipeline.h"
#include "Crowny/Renderer/Renderer2D.h"
#include "Crowny/Renderer/ShaderVariation.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Import/Importer.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/Renderer/Renderer.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <tracy/Tracy.hpp>

#define raytracing false

namespace Crowny
{
    using namespace Literals;

    namespace
    {
        std::atomic<uint64_t> s_NextHistoryOwnerId{ 1 };

        uint64_t AllocateHistoryOwnerId()
        {
            uint64_t value = s_NextHistoryOwnerId.fetch_add(1, std::memory_order_relaxed);
            if (value == 0)
                value = s_NextHistoryOwnerId.fetch_add(1, std::memory_order_relaxed);
            return value;
        }

        uint64_t CurrentSimulationFrameNumber()
        {
            const float frameCount = Time::GetFrameCount();
            return frameCount >= 1.0f ? static_cast<uint64_t>(frameCount) : 1u;
        }

        uint64_t CameraHistoryNamespace(uint64_t historyOwnerId, const Scene* scene, uint64_t cameraIdentity, uint64_t identityKind)
        {
            uint64_t value = historyOwnerId;
            value ^= reinterpret_cast<uint64_t>(scene) + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
            value ^= cameraIdentity + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
            value ^= identityKind + 0x9e3779b97f4a7c15ull + (value << 6u) + (value >> 2u);
            return value == 0 ? 1 : value;
        }

        uint64_t ExternalCameraHistoryNamespace(uint64_t historyOwnerId, const Scene* scene, const Camera* camera)
        {
            constexpr uint64_t externalCameraKind = 0x45585445524e414cull;
            return CameraHistoryNamespace(historyOwnerId, scene, reinterpret_cast<uint64_t>(camera), externalCameraKind);
        }

        uint64_t SceneCameraHistoryNamespace(uint64_t historyOwnerId, const Scene* scene, entt::entity cameraEntity)
        {
            constexpr uint64_t sceneCameraKind = 0x5343454e4543414dull;
            return CameraHistoryNamespace(historyOwnerId, scene, static_cast<uint64_t>(entt::to_integral(cameraEntity)), sceneCameraKind);
        }

        bool ProjectionChanged(const glm::mat4& first, const glm::mat4& second)
        {
            for (uint32_t column = 0; column < 4; column++)
                for (uint32_t row = 0; row < 4; row++)
                    if (std::abs(first[column][row] - second[column][row]) > 0.0001f)
                        return true;
            return false;
        }

        bool HasForwardOnlyOpaqueMaterial(const Vector<AssetHandle<Material>>& materials)
        {
            return std::any_of(materials.begin(), materials.end(), [](const AssetHandle<Material>& material) {
                return material && MaterialRenderClassifier::Classify(*material).IsForwardOnlyOpaque();
            });
        }

        bool IsRenderableObjectVisible(const RenderableObject& object, const VisibilityFrustum& frustum, RenderLayerMask visibilityMask)
        {
            return object.Visible && object.VisibilityLayers.Intersects(visibilityMask) &&
                   (object.BoundingSphere.w < 0.0f || frustum.IntersectsSphere(glm::vec3(object.BoundingSphere), object.BoundingSphere.w));
        }

        class PointShadowLayerAllocator
        {
        public:
            static constexpr uint32_t InvalidLayer = std::numeric_limits<uint32_t>::max();

            explicit PointShadowLayerAllocator(uint32_t layerCount = 16)
            {
                m_FreeLayers.reserve(layerCount);
                for (uint32_t layer = layerCount; layer > 0; layer--)
                    m_FreeLayers.push_back(layer - 1u);
            }

            uint32_t Acquire(RenderLightHandle light)
            {
                const auto existing = m_Allocations.find(light.GetValue());
                if (existing != m_Allocations.end())
                    return existing->second;
                if (m_FreeLayers.empty())
                    return InvalidLayer;
                const uint32_t layer = m_FreeLayers.back();
                m_FreeLayers.pop_back();
                m_Allocations.emplace(light.GetValue(), layer);
                return layer;
            }

            void ReleaseMissing(const UnorderedSet<uint32_t>& activeLights, uint64_t retireValue)
            {
                for (auto allocation = m_Allocations.begin(); allocation != m_Allocations.end();)
                {
                    if (activeLights.find(allocation->first) != activeLights.end())
                    {
                        ++allocation;
                        continue;
                    }
                    m_Retired.push_back({ allocation->second, retireValue });
                    allocation = m_Allocations.erase(allocation);
                }
            }

            void Collect(uint64_t completedValue)
            {
                for (auto retired = m_Retired.begin(); retired != m_Retired.end();)
                {
                    if (retired->RetireValue > completedValue)
                    {
                        ++retired;
                        continue;
                    }
                    m_FreeLayers.push_back(retired->Layer);
                    retired = m_Retired.erase(retired);
                }
            }

        private:
            struct RetiredLayer
            {
                uint32_t Layer = 0;
                uint64_t RetireValue = 0;
            };

            UnorderedMap<uint32_t, uint32_t> m_Allocations;
            Vector<uint32_t> m_FreeLayers;
            Vector<RetiredLayer> m_Retired;
        };

        enum class ShadowRenderTarget : uint8_t
        {
            Atlas,
            PointArray,
            DirectionalArray
        };

        struct ShadowRenderView
        {
            ShadowRenderTarget Target = ShadowRenderTarget::Atlas;
            glm::mat4 ViewProjection = glm::mat4(1.0f);
            glm::vec4 Viewport = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            uint32_t Layer = 0;
            GpuDrawList DrawList;
            GpuDrawBuffers DrawBuffers;
        };

        class GpuDrivenPassExecutor
        {
        public:
            void BeginFrame(const RenderView& view, const RenderBlackboard& blackboard, GpuScene& scene, const GpuDrawList& depthDrawList,
                            const GpuDrawBinLayout* drawBinLayout, const RenderSnapshot& snapshot, const Ref<EnvironmentMap>& environment,
                            const RenderPipelineSettings& settings)
            {
                m_View = view;
                m_Blackboard = &blackboard;
                m_Scene = &scene;
                m_DepthDrawList = &depthDrawList;
                m_DrawBinLayout = drawBinLayout;
                m_Snapshot = &snapshot;
                m_Environment = environment;
                m_Settings = settings;
                m_GpuInstanceCullingReady = false;
                m_GpuMeshletExpansionReady = false;
                m_GpuMeshletCullingReady = false;
                m_GpuDrawCompactionReady = false;
            }

            void Execute(StringView name, RenderGraphContext& context)
            {
                if (m_Blackboard == nullptr || m_Scene == nullptr || RenderAPI::TryGet() == nullptr)
                    return;
                if (name == "ClearVisibilityCounters")
                    Clear(context, "VisibilityCounters", 8);
                else if (name == "ClearMeshletCandidateCounters")
                    Clear(context, "MeshletCandidateCounters", 4);
                else if (name == "ClearDrawCounters")
                    Clear(context, "DrawCounters", 8);
                else if (name == "ClearIndirectDrawCounts")
                    ClearDrawBinCounts(context);
                else if (name == "ClearClusterLightCounters")
                    Clear(context, "ClusterLightCounters", 4);
                else if (name == "CullInstancesAndSelectLod")
                    CullInstances(context);
                else if (name == "ExpandVisibleMeshlets")
                    ExpandMeshlets(context);
                else if (name == "LateOcclusionAndMeshletCulling")
                    CullMeshlets(context);
                else if (name == "BinAndCompactIndirectDraws")
                    BinAndCompactDraws(context);
                else if (name == "BuildClusteredLightLists")
                    BuildClusters(context);
                else if (name == "GTAO")
                    RenderGtao(context);
                else if (name == "ReverseZDepthVelocity")
                    RenderDepth(context);
                else if (name == "BuildCurrentHiZ")
                    BuildHiZ(context);
                else if (name == "ForwardPlusOpaque")
                    RenderForwardPlus(context);
                else if (name == "DeferredGBuffer")
                    RenderDeferredGBuffer(context);
                else if (name == "DeferredPlusLighting8x8")
                    RenderDeferredLighting(context);
                else if (name == "ToonOutlines")
                    RenderToonOutlines(context);
                else if (name == "SkyAndForwardOnlyOpaque")
                    RenderSkyAndForwardOnlyOpaque(context);
                else if (name == "ForwardPlusTransparencyAndWorld2D")
                    RenderTransparency(context);
                else if (name == "TemporalResolve")
                    RenderTemporalResolve(context);
                else if (name == "Bloom")
                    RenderBloom(context);
                else if (name == "ExposureToneMapAndColorGrade")
                    RenderToneMap(context);
            }

            void RenderShadows(RenderGraphContext& context, const Vector<ShadowRenderView>& views, uint32_t viewCount)
            {
                viewCount = std::min<uint32_t>(viewCount, static_cast<uint32_t>(views.size()));
                if (viewCount == 0 || !Ensure(m_ShadowDepth, m_ShadowDepthAttempted, "Resources/Shaders/GpuShadowDepth.asset"))
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                if (!instances)
                    return;
                m_ShadowDepth.SetBuffer(0, 1, instances);
                BindMaterialTable(m_ShadowDepth, m_ShadowTextureVersion, context);

                for (uint32_t viewIndex = 0; viewIndex < viewCount; viewIndex++)
                {
                    const ShadowRenderView& view = views[viewIndex];
                    const Ref<GenericGpuBuffer>& instanceIds = view.DrawBuffers.GetInstanceIDBuffer();
                    const Ref<GenericGpuBuffer>& commands = view.DrawBuffers.GetCommandBuffer();
                    if (!instanceIds || !commands || view.DrawList.Commands.empty())
                        continue;
                    RenderGraphRenderTargetDesc attachments;
                    switch (view.Target)
                    {
                    case ShadowRenderTarget::Atlas:
                        attachments.Depth = Resource("ShadowAtlas");
                        break;
                    case ShadowRenderTarget::PointArray:
                        attachments.Depth = Resource("PointShadowArray");
                        attachments.FirstLayer = view.Layer;
                        break;
                    case ShadowRenderTarget::DirectionalArray:
                        attachments.Depth = Resource("DirectionalShadowArray");
                        attachments.FirstLayer = view.Layer;
                        break;
                    }
                    const Ref<RenderTarget> target = context.GetRenderTarget(attachments);
                    if (!target)
                        continue;
                    m_ShadowDepth.WriteUniformBlock(0, 0, &view.ViewProjection, sizeof(view.ViewProjection));
                    m_ShadowDepth.SetBuffer(0, 2, instanceIds);
                    RenderAPI::TryGet()->SetRenderTarget(target);
                    RenderAPI::TryGet()->SetViewport(view.Viewport.x, view.Viewport.y, view.Viewport.z, view.Viewport.w);
                    RenderAPI::TryGet()->ClearViewport(FBT_DEPTH, glm::vec4(0.0f), 0.0f);
                    DrawCpuOpaqueRuns(m_ShadowDepth, commands, view.DrawList, false, true);
                }
            }

        private:
            struct alignas(16) CullingConstants
            {
                std::array<glm::vec4, 6> FrustumPlanes{};
                glm::mat4 View = glm::mat4(1.0f);
                glm::mat4 Projection = glm::mat4(1.0f);
                glm::vec4 ViewportAndNearPlane = glm::vec4(1.0f);
                uint32_t InstanceCapacity = 0;
                uint32_t MaximumVisibleInstances = 0;
                uint32_t VisibilityLayerMask = 0xffffffffu;
                uint32_t HiZMipCount = 0;
                uint32_t CameraCut = 1;
                float OcclusionBias = 0.0005f;
                float MaximumLodErrorPixels = 1.0f;
                uint32_t Padding = 0;
            };

            struct alignas(16) ExpandConstants
            {
                uint32_t MaximumVisibleInstances = 0;
                uint32_t MaximumCandidates = 0;
                uint32_t Padding0 = 0;
                uint32_t Padding1 = 0;
            };

            struct alignas(16) MeshletCullingConstants
            {
                std::array<glm::vec4, 6> FrustumPlanes{};
                glm::mat4 View = glm::mat4(1.0f);
                glm::mat4 Projection = glm::mat4(1.0f);
                glm::vec4 CameraPosition = glm::vec4(0.0f);
                glm::vec4 ViewportAndNearPlane = glm::vec4(1.0f);
                uint32_t MaximumCandidates = 0;
                uint32_t MaximumDrawCount = 0;
                uint32_t VisibilityLayerMask = 0xffffffffu;
                uint32_t HiZMipCount = 0;
                float OcclusionBias = 0.0005f;
                uint32_t CameraCut = 1;
                uint32_t Padding0 = 0;
                uint32_t Padding1 = 0;
            };

            struct alignas(16) DrawBinCompactionConstants
            {
                uint32_t MaximumInputCommandCount = 0;
                uint32_t LookupMask = 0;
                uint32_t LookupCapacity = 0;
                uint32_t BinCount = 0;
                uint32_t MaterialCount = 0;
                uint32_t Padding0 = 0;
                uint32_t Padding1 = 0;
                uint32_t Padding2 = 0;
            };

            struct alignas(16) ClusterConstants
            {
                glm::mat4 View = glm::mat4(1.0f);
                glm::mat4 InverseProjection = glm::mat4(1.0f);
                glm::uvec4 DimensionsAndLightCount = glm::uvec4(0u);
                glm::uvec4 ViewportTileAndLimit = glm::uvec4(0u);
                glm::vec4 DepthAndScale = glm::vec4(0.0f);
                uint32_t VisibilityLayerMask = 0xffffffffu;
                uint32_t MaximumDirectionalLights = 8;
                uint32_t MaximumLightIndices = 0;
                uint32_t Padding = 0;
            };

            struct alignas(16) LightingViewConstants
            {
                glm::mat4 ViewProjection = glm::mat4(1.0f);
                glm::mat4 View = glm::mat4(1.0f);
                glm::mat4 InverseViewProjection = glm::mat4(1.0f);
                glm::vec4 CameraPositionPreExposure = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                glm::uvec4 ClusterDimensionsAndTileSize = glm::uvec4(1u, 1u, 24u, 16u);
                glm::vec4 ClusterDepthAndViewport = glm::vec4(0.01f, 1000.0f, 1.0f, 1.0f);
            };

            struct alignas(16) EnvironmentConstants
            {
                std::array<glm::vec4, 9> DiffuseSh{};
                float SpecularMipCount = 0.0f;
                float Intensity = 0.0f;
                glm::vec2 Padding = glm::vec2(0.0f);
            };

            struct alignas(16) HiZConstants
            {
                glm::uvec2 DestinationSize = glm::uvec2(1u);
                uint32_t SourceMip = 0;
                uint32_t CopySource = 0;
            };

            struct alignas(16) DepthViewConstants
            {
                glm::mat4 ViewProjection = glm::mat4(1.0f);
                glm::mat4 PreviousViewProjection = glm::mat4(1.0f);
            };

            struct alignas(16) GtaoConstants
            {
                glm::uvec2 OutputSize = glm::uvec2(1u);
                glm::uvec2 SourceSize = glm::uvec2(1u);
                float RadiusPixels = 8.0f;
                float Intensity = 1.0f;
                float DepthBias = 0.0005f;
                float Padding = 0.0f;
            };

            struct alignas(16) TemporalConstants
            {
                glm::uvec2 Resolution = glm::uvec2(1u);
                uint32_t HistoryValid = 0;
                float Feedback = 0.9f;
            };

            struct alignas(16) BloomConstants
            {
                glm::uvec2 OutputSize = glm::uvec2(1u);
                float Threshold = 1.0f;
                float Knee = 0.5f;
            };

            struct alignas(16) ToneMapConstants
            {
                float Exposure = 1.0f;
                float BloomIntensity = 0.0f;
                glm::vec2 Padding = glm::vec2(0.0f);
            };

            struct alignas(16) ToonOutlineConstants
            {
                glm::mat4 InverseViewProjection = glm::mat4(1.0f);
                glm::vec4 CameraPosition = glm::vec4(0.0f);
                glm::uvec4 ResolutionAndFlags = glm::uvec4(1u, 1u, 0u, 0u);
            };

            struct alignas(16) SkyConstants
            {
                glm::mat4 InverseViewProjection = glm::mat4(1.0f);
                glm::vec4 CameraPositionIntensity = glm::vec4(0.0f);
                glm::vec4 BackgroundColor = glm::vec4(0.025f, 0.03f, 0.04f, 1.0f);
            };

            static_assert(sizeof(LightingViewConstants) == 240);
            static_assert(sizeof(EnvironmentConstants) == 160);

            RenderGraphResourceHandle Resource(StringView name) const { return m_Blackboard->Get(name); }

            Ref<GenericGpuBuffer> Buffer(RenderGraphContext& context, StringView name) const
            {
                const RenderGraphResourceHandle handle = Resource(name);
                return handle ? context.GetBuffer(handle) : nullptr;
            }

            Ref<Texture> TextureResource(RenderGraphContext& context, StringView name) const
            {
                const RenderGraphResourceHandle handle = Resource(name);
                return handle ? context.GetTexture(handle) : nullptr;
            }

            bool Ensure(ComputeMaterial& material, bool& attempted, StringView assetPath)
            {
                if (material.IsValid())
                    return true;
                if (attempted || AssetManager::TryGet() == nullptr)
                    return false;
                attempted = true;
                const AssetHandle<Shader> shader = AssetManager::TryGet()->Load<Shader>(Path(assetPath));
                if (!material.Initialize(shader))
                    CW_ENGINE_ERROR("Failed to initialize {}: {}", assetPath, material.GetError());
                return material.IsValid();
            }

            bool Ensure(GraphicsMaterial& material, bool& attempted, StringView assetPath)
            {
                if (material.IsValid())
                    return true;
                if (attempted || AssetManager::TryGet() == nullptr)
                    return false;
                attempted = true;
                const AssetHandle<Shader> shader = AssetManager::TryGet()->Load<Shader>(Path(assetPath));
                if (!material.Initialize(shader))
                    CW_ENGINE_ERROR("Failed to initialize {}: {}", assetPath, material.GetError());
                return material.IsValid();
            }

            bool Ensure(GraphicsMaterial& material, bool& attempted, StringView assetPath, const ShaderVariation& variation)
            {
                if (material.IsValid())
                    return true;
                if (attempted || AssetManager::TryGet() == nullptr)
                    return false;
                attempted = true;
                const AssetHandle<Shader> shader = AssetManager::TryGet()->Load<Shader>(Path(assetPath));
                if (!material.Initialize(shader, variation))
                    CW_ENGINE_ERROR("Failed to initialize {} variation {}: {}", assetPath, variation.GetCanonicalKey(), material.GetError());
                return material.IsValid();
            }

            static constexpr size_t DepthProgramIndex(DepthPrepassProgram program) { return static_cast<size_t>(program); }
            static constexpr size_t DEPTH_PROGRAM_COUNT = DepthProgramIndex(DepthPrepassProgram::AnimatedObjectID) + 1u;

            GraphicsMaterial* ResolveDepthMaterial(DepthPrepassProgram program, bool masked)
            {
                if (masked)
                {
                    const size_t index = DepthProgramIndex(program);
                    ShaderVariation variation;
                    variation.Set("CW_DEPTH_ANIMATED", program == DepthPrepassProgram::Animated || program == DepthPrepassProgram::AnimatedObjectID);
                    variation.Set("CW_DEPTH_OBJECT_ID_ONLY",
                                  program == DepthPrepassProgram::StaticObjectID || program == DepthPrepassProgram::AnimatedObjectID);
                    return Ensure(m_MaskedDepth[index], m_MaskedDepthAttempted[index], "Resources/Shaders/GpuMaskedDepth.asset", variation)
                             ? &m_MaskedDepth[index]
                             : nullptr;
                }
                switch (program)
                {
                case DepthPrepassProgram::Static:
                    return Ensure(m_Depth, m_DepthAttempted, "Resources/Shaders/GpuDepthOnly.asset") ? &m_Depth : nullptr;
                case DepthPrepassProgram::Animated:
                    return Ensure(m_AnimatedDepth, m_AnimatedDepthAttempted, "Resources/Shaders/GpuAnimatedDepthOnly.asset")
                             ? &m_AnimatedDepth
                             : nullptr;
                case DepthPrepassProgram::StaticObjectID:
                    return Ensure(m_DepthObjectID, m_DepthObjectIDAttempted, "Resources/Shaders/GpuDepthObjectID.asset")
                             ? &m_DepthObjectID
                             : nullptr;
                case DepthPrepassProgram::AnimatedObjectID:
                    return Ensure(m_AnimatedDepthObjectID, m_AnimatedDepthObjectIDAttempted,
                                  "Resources/Shaders/GpuAnimatedDepthObjectID.asset")
                             ? &m_AnimatedDepthObjectID
                             : nullptr;
                }
                return nullptr;
            }

            bool EnsureTransparent(GraphicsMaterial& material, bool& attempted, bool additive)
            {
                if (material.IsValid())
                    return true;
                if (attempted || AssetManager::TryGet() == nullptr)
                    return false;
                attempted = true;
                const AssetHandle<Shader> shader = AssetManager::TryGet()->Load<Shader>("Resources/Shaders/ForwardPlusStandard.asset");
                Ref<BlendStateDesc> blend = CreateRef<BlendStateDesc>();
                blend->EnableBlending = true;
                blend->SrcBlend = BlendFactor::One;
                blend->DstBlend = additive ? BlendFactor::One : BlendFactor::InvSourceAlpha;
                blend->SrcBlendAlpha = BlendFactor::One;
                blend->DstBlendAlpha = additive ? BlendFactor::One : BlendFactor::InvSourceAlpha;
                Ref<DepthStencilStateDesc> depth = CreateRef<DepthStencilStateDesc>();
                depth->EnableDepthRead = true;
                depth->EnableDepthWrite = false;
                depth->DepthCompareFunction = CompareFunction::GREATER_EQUAL;
                if (!material.Initialize(shader, blend, depth))
                    CW_ENGINE_ERROR("Failed to initialize Forward+ {} transparency: {}", additive ? "additive" : "premultiplied",
                                    material.GetError());
                return material.IsValid();
            }

            void Clear(RenderGraphContext& context, StringView resourceName, uint32_t wordCount)
            {
                const Ref<GenericGpuBuffer> buffer = Buffer(context, resourceName);
                if (!buffer)
                    return;
                const std::array<uint32_t, 8> zero{};
                buffer->WriteData(0, std::min<uint32_t>(wordCount * sizeof(uint32_t), buffer->GetSize()), zero.data(), BWT_NORMAL);
            }

            void ClearDrawBinCounts(RenderGraphContext& context)
            {
                const Ref<GenericGpuBuffer> counts = Buffer(context, "IndirectDrawCounts");
                if (!counts || m_DrawBinLayout == nullptr)
                    return;
                const uint32_t wordCount = static_cast<uint32_t>(m_DrawBinLayout->GetBins().size()) * 2u;
                if (wordCount == 0)
                    return;
                m_ZeroDrawBinCounts.resize(wordCount, 0u);
                std::fill(m_ZeroDrawBinCounts.begin(), m_ZeroDrawBinCounts.end(), 0u);
                counts->WriteData(0, std::min<uint32_t>(wordCount * sizeof(uint32_t), counts->GetSize()), m_ZeroDrawBinCounts.data(), BWT_NORMAL);
            }

            void CullInstances(RenderGraphContext& context)
            {
                if (!Ensure(m_CullInstances, m_CullInstancesAttempted, "Resources/Shaders/GpuCullInstances.asset"))
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                const Ref<GenericGpuBuffer> visible = Buffer(context, "VisibleInstances");
                const Ref<GenericGpuBuffer> counters = Buffer(context, "VisibilityCounters");
                const Ref<GenericGpuBuffer> meshes = Buffer(context, "MeshTable");
                const Ref<GenericGpuBuffer> lods = Buffer(context, "MeshLodTable");
                if (!instances || !visible || !counters || !meshes || !lods)
                    return;

                CullingConstants constants;
                constants.FrustumPlanes =
                  VisibilityFrustum::FromViewProjection(m_View.Projection * m_View.View, RenderAPI::GetAPI() == RenderAPI::API::Vulkan).Planes;
                constants.View = m_View.View;
                constants.Projection = m_View.Projection;
                constants.ViewportAndNearPlane = { m_View.ViewportSize, NearPlane(), 0.0f };
                constants.InstanceCapacity = m_Scene->GetStats().InstanceCapacity;
                constants.MaximumVisibleInstances = visible->GetSize() / (sizeof(uint32_t) * 2u);
                constants.VisibilityLayerMask = m_View.VisibilityMask.Value;
                const RenderGraphResourceHandle previousHiZ = Resource("PreviousHiZ");
                const Ref<Texture> hiZ = previousHiZ ? context.GetTexture(previousHiZ) : nullptr;
                const bool historyValid = previousHiZ && context.IsHistoryValid(previousHiZ);
                constants.HiZMipCount = historyValid && hiZ ? hiZ->GetDesc().MipLevels + 1u : 0u;
                constants.CameraCut = m_View.CameraCut || !historyValid ? 1u : 0u;
                m_CullInstances.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_CullInstances.SetBuffer(0, 1, instances);
                m_CullInstances.SetBuffer(0, 2, visible);
                m_CullInstances.SetBuffer(0, 3, counters);
                m_CullInstances.SetTexture(0, 4, TextureResource(context, "PreviousHiZ"));
                m_CullInstances.SetBuffer(0, 5, meshes);
                m_CullInstances.SetBuffer(0, 6, lods);
                m_GpuInstanceCullingReady = m_CullInstances.Dispatch((constants.InstanceCapacity + 63u) / 64u);
            }

            void ExpandMeshlets(RenderGraphContext& context)
            {
                if (!m_GpuInstanceCullingReady ||
                    !Ensure(m_ExpandMeshlets, m_ExpandMeshletsAttempted, "Resources/Shaders/ExpandVisibleMeshlets.asset"))
                    return;
                const Ref<GenericGpuBuffer> visible = Buffer(context, "VisibleInstances");
                const Ref<GenericGpuBuffer> candidates = Buffer(context, "MeshletCandidates");
                if (!visible || !candidates)
                    return;
                ExpandConstants constants;
                constants.MaximumVisibleInstances = visible->GetSize() / (sizeof(uint32_t) * 2u);
                constants.MaximumCandidates = candidates->GetSize() / (sizeof(uint32_t) * 2u);
                m_ExpandMeshlets.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_ExpandMeshlets.SetBuffer(0, 1, Buffer(context, "InstanceTable"));
                m_ExpandMeshlets.SetBuffer(0, 2, visible);
                m_ExpandMeshlets.SetBuffer(0, 3, Buffer(context, "MeshTable"));
                m_ExpandMeshlets.SetBuffer(0, 4, Buffer(context, "MeshLodTable"));
                m_ExpandMeshlets.SetBuffer(0, 5, candidates);
                m_ExpandMeshlets.SetBuffer(0, 6, Buffer(context, "MeshletCandidateCounters"));
                m_ExpandMeshlets.SetBuffer(0, 7, Buffer(context, "VisibilityCounters"));
                m_GpuMeshletExpansionReady = m_ExpandMeshlets.Dispatch((constants.MaximumVisibleInstances + 63u) / 64u);
            }

            void CullMeshlets(RenderGraphContext& context)
            {
                if (!m_GpuMeshletExpansionReady ||
                    !Ensure(m_CullMeshlets, m_CullMeshletsAttempted, "Resources/Shaders/CullMeshletsAndBuildDraws.asset"))
                    return;
                const Ref<GenericGpuBuffer> candidates = Buffer(context, "MeshletCandidates");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "CulledIndirectCommands");
                if (!candidates || !commands)
                    return;
                MeshletCullingConstants constants;
                constants.FrustumPlanes =
                  VisibilityFrustum::FromViewProjection(m_View.Projection * m_View.View, RenderAPI::GetAPI() == RenderAPI::API::Vulkan).Planes;
                constants.View = m_View.View;
                constants.Projection = m_View.Projection;
                const glm::mat4 inverseView = glm::inverse(m_View.View);
                constants.CameraPosition = inverseView[3];
                constants.ViewportAndNearPlane = { m_View.ViewportSize, NearPlane(), 0.0f };
                constants.MaximumCandidates = candidates->GetSize() / (sizeof(uint32_t) * 2u);
                constants.MaximumDrawCount = commands->GetSize() / sizeof(DrawIndexedIndirectCommand);
                constants.VisibilityLayerMask = m_View.VisibilityMask.Value;
                const Ref<Texture> hiZ = TextureResource(context, "CurrentHiZ");
                constants.HiZMipCount = hiZ ? hiZ->GetDesc().MipLevels + 1u : 0u;
                constants.CameraCut = hiZ ? 0u : 1u;
                m_CullMeshlets.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_CullMeshlets.SetBuffer(0, 1, Buffer(context, "InstanceTable"));
                m_CullMeshlets.SetBuffer(0, 2, Buffer(context, "MeshletTable"));
                m_CullMeshlets.SetBuffer(0, 3, candidates);
                m_CullMeshlets.SetTexture(0, 4, TextureResource(context, "CurrentHiZ"));
                m_CullMeshlets.SetBuffer(0, 5, Buffer(context, "CulledDrawInstances"));
                m_CullMeshlets.SetBuffer(0, 6, commands);
                m_CullMeshlets.SetBuffer(0, 7, Buffer(context, "DrawSortKeys"));
                m_CullMeshlets.SetBuffer(0, 8, Buffer(context, "DrawCounters"));
                m_CullMeshlets.SetBuffer(0, 9, Buffer(context, "MeshletCandidateCounters"));
                m_GpuMeshletCullingReady = m_CullMeshlets.Dispatch((constants.MaximumCandidates + 63u) / 64u);
            }

            void BinAndCompactDraws(RenderGraphContext& context)
            {
                if (!m_GpuMeshletCullingReady || m_DrawBinLayout == nullptr || m_DrawBinLayout->GetBins().empty() ||
                    !Ensure(m_BinAndCompactDraws, m_BinAndCompactDrawsAttempted, "Resources/Shaders/BinAndCompactIndirectDraws.asset"))
                    return;
                const Ref<GenericGpuBuffer> culledCommands = Buffer(context, "CulledIndirectCommands");
                const Ref<GenericGpuBuffer> culledInstances = Buffer(context, "CulledDrawInstances");
                const Ref<GenericGpuBuffer> materials = Buffer(context, "MaterialTable");
                const Ref<GenericGpuBuffer> drawBins = Buffer(context, "DrawBinTable");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "IndirectCommands");
                const Ref<GenericGpuBuffer> visibleInstances = Buffer(context, "VisibleDrawInstances");
                const Ref<GenericGpuBuffer> counts = Buffer(context, "IndirectDrawCounts");
                if (!culledCommands || !culledInstances || !materials || !drawBins || !commands || !visibleInstances || !counts)
                    return;

                DrawBinCompactionConstants constants;
                constants.MaximumInputCommandCount = culledCommands->GetSize() / sizeof(DrawIndexedIndirectCommand);
                constants.LookupMask = m_DrawBinLayout->GetLookupMask();
                constants.LookupCapacity = static_cast<uint32_t>(m_DrawBinLayout->GetLookupEntries().size());
                constants.BinCount = static_cast<uint32_t>(m_DrawBinLayout->GetBins().size());
                constants.MaterialCount = m_Scene->GetMaterialCount();
                m_BinAndCompactDraws.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_BinAndCompactDraws.SetBuffer(0, 1, culledInstances);
                m_BinAndCompactDraws.SetBuffer(0, 2, culledCommands);
                m_BinAndCompactDraws.SetBuffer(0, 3, Buffer(context, "DrawSortKeys"));
                m_BinAndCompactDraws.SetBuffer(0, 4, Buffer(context, "DrawCounters"));
                m_BinAndCompactDraws.SetBuffer(0, 5, materials);
                m_BinAndCompactDraws.SetBuffer(0, 6, drawBins);
                m_BinAndCompactDraws.SetBuffer(0, 7, visibleInstances);
                m_BinAndCompactDraws.SetBuffer(0, 8, commands);
                m_BinAndCompactDraws.SetBuffer(0, 9, counts);
                m_GpuDrawCompactionReady = m_BinAndCompactDraws.Dispatch((constants.MaximumInputCommandCount + 63u) / 64u);
            }

            void BuildClusters(RenderGraphContext& context)
            {
                if (!Ensure(m_BuildClusters, m_BuildClustersAttempted, "Resources/Shaders/BuildClusteredLights.asset"))
                    return;
                const uint32_t width = std::max(static_cast<uint32_t>(m_View.ViewportSize.x), 1u);
                const uint32_t height = std::max(static_cast<uint32_t>(m_View.ViewportSize.y), 1u);
                const ClusteredLightGridDesc clusterDesc =
                  ClusteredLightBuilder::ResolveDesc(m_Settings, width, height, NearPlane(), 1000.0f, m_View.VisibilityMask);
                const glm::uvec3 dimensions = ClusteredLightBuilder::GetDimensions(clusterDesc);
                const uint64_t clusterCount = ClusteredLightBuilder::GetClusterCount(clusterDesc);
                const Ref<GenericGpuBuffer> lightIndices = Buffer(context, "ClusterLightIndices");
                if (!lightIndices || clusterCount > std::numeric_limits<uint32_t>::max())
                    return;
                ClusterConstants constants;
                constants.View = m_View.View;
                constants.InverseProjection = glm::inverse(m_View.Projection);
                constants.DimensionsAndLightCount = { dimensions.x, dimensions.y, dimensions.z, m_Scene->GetStats().LightCapacity };
                constants.ViewportTileAndLimit = { width, height, clusterDesc.TileSize, clusterDesc.MaxLightsPerCluster };
                constants.DepthAndScale = { clusterDesc.NearPlane, clusterDesc.FarPlane, 0.0f, 0.0f };
                constants.VisibilityLayerMask = clusterDesc.VisibilityMask.Value;
                constants.MaximumDirectionalLights = clusterDesc.MaxDirectionalLights;
                constants.MaximumLightIndices = lightIndices->GetSize() / sizeof(uint32_t);
                m_BuildClusters.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_BuildClusters.SetBuffer(0, 1, Buffer(context, "LightTable"));
                m_BuildClusters.SetBuffer(0, 2, Buffer(context, "ClusterCells"));
                m_BuildClusters.SetBuffer(0, 3, lightIndices);
                m_BuildClusters.SetBuffer(0, 4, Buffer(context, "DirectionalLightIndices"));
                m_BuildClusters.SetBuffer(0, 5, Buffer(context, "ClusterLightCounters"));
                m_BuildClusters.Dispatch((static_cast<uint32_t>(clusterCount) + 63u) / 64u);
            }

            void RenderGtao(RenderGraphContext& context)
            {
                if (!Ensure(m_Gtao, m_GtaoAttempted, "Resources/Shaders/Gtao.asset"))
                    return;
                const Ref<Texture> depth = TextureResource(context, "SceneDepth");
                const Ref<Texture> output = TextureResource(context, "AmbientOcclusion");
                if (!depth || !output)
                    return;
                GtaoConstants constants;
                constants.OutputSize = { output->GetWidth(), output->GetHeight() };
                constants.SourceSize = { depth->GetWidth(), depth->GetHeight() };
                m_Gtao.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_Gtao.SetTexture(0, 1, depth);
                m_Gtao.SetTexture(0, 2, TextureResource(context, "CurrentHiZ"));
                m_Gtao.SetLoadStoreTexture(0, 3, output);
                m_Gtao.Dispatch((constants.OutputSize.x + 7u) / 8u, (constants.OutputSize.y + 7u) / 8u);
            }

            void RenderDepth(RenderGraphContext& context)
            {
                if (m_DepthDrawList == nullptr || m_DepthDrawList->Commands.empty())
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                const Ref<GenericGpuBuffer> instanceIds = Buffer(context, "DepthInstanceIds");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "DepthIndirectCommands");
                if (!instances || !instanceIds || !commands)
                    return;

                const DepthPrepassOutputLayout outputLayout =
                  ResolveDepthPrepassOutputLayout(Resource("Velocity").IsValid(), Resource("ObjectID").IsValid());

                RenderGraphRenderTargetDesc attachments;
                attachments.Depth = Resource("SceneDepth");
                DepthViewConstants view;
                view.ViewProjection = m_View.Projection * m_View.View;
                view.PreviousViewProjection = m_View.PreviousViewProjection;
                attachments.ColorCount = outputLayout.ColorAttachmentCount;
                if (outputLayout.MotionVectorAttachment != DepthPrepassOutputLayout::NoAttachment)
                    attachments.Colors[outputLayout.MotionVectorAttachment] = Resource("Velocity");
                if (outputLayout.ObjectIDAttachment != DepthPrepassOutputLayout::NoAttachment)
                    attachments.Colors[outputLayout.ObjectIDAttachment] = Resource("ObjectID");
                const Ref<RenderTarget> outputTarget = context.GetRenderTarget(attachments);
                if (!outputTarget)
                    return;
                RenderAPI::TryGet()->SetRenderTarget(outputTarget);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                RenderAPI::TryGet()->ClearViewport(FBT_DEPTH | (attachments.ColorCount != 0 ? FBT_COLOR : 0), glm::vec4(0.0f), 0.0f);

                GraphicsMaterial* boundMaterial = nullptr;
                RenderTarget* boundTarget = outputTarget.get();
                Ref<RenderTarget> depthOnlyTarget;
                for (const GpuDrawRun& run : m_DepthDrawList->Runs)
                {
                    const bool depthPhase = run.Bin.Phase == RenderDrawPhase::Opaque || run.Bin.Phase == RenderDrawPhase::ForwardOpaque;
                    if (!depthPhase || !ParticipatesInDepthPrepass(run.Bin.Alpha) || run.CommandCount == 0)
                        continue;
                    const Ref<VertexBuffer> vertexBuffer = m_Scene->GetGeometryVertexBuffer(run.Bin.GeometryHeap);
                    const Ref<IndexBuffer> indexBuffer = m_Scene->GetGeometryIndexBuffer(run.Bin.GeometryHeap);
                    if (!vertexBuffer || !indexBuffer)
                        continue;
                    const bool animated = vertexBuffer->GetLayout()->HasAttribute(VertexAttribute::PreviousPosition);
                    const bool masked = run.Bin.Alpha == AlphaMode::Mask;
                    const DepthPrepassProgramSelection program = ResolveDepthPrepassProgram(outputLayout.Mode, animated);
                    DepthPrepassProgram selectedProgram = program.Primary;
                    GraphicsMaterial* depthMaterial = ResolveDepthMaterial(selectedProgram, masked);
                    bool depthOnlyFallback = false;
                    if (depthMaterial == nullptr && program.HasFallback)
                    {
                        selectedProgram = program.Fallback;
                        depthMaterial = ResolveDepthMaterial(selectedProgram, masked);
                        depthOnlyFallback = depthMaterial != nullptr;
                    }
                    if (depthMaterial == nullptr)
                        continue;
                    Ref<RenderTarget> target = outputTarget;
                    if (depthOnlyFallback)
                    {
                        if (!depthOnlyTarget)
                        {
                            RenderGraphRenderTargetDesc depthOnlyAttachments;
                            depthOnlyAttachments.Depth = Resource("SceneDepth");
                            depthOnlyTarget = context.GetRenderTarget(depthOnlyAttachments);
                        }
                        target = depthOnlyTarget;
                    }
                    if (!target)
                        continue;
                    if (boundTarget != target.get())
                    {
                        RenderAPI::TryGet()->SetRenderTarget(target, 0, depthOnlyFallback ? RT_DEPTH_STENCIL : RT_ALL);
                        RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                        boundTarget = target.get();
                        boundMaterial = nullptr;
                    }
                    if (depthMaterial != boundMaterial)
                    {
                        depthMaterial->WriteUniformBlock(0, 0, &view, sizeof(view));
                        depthMaterial->SetBuffer(0, 1, instances);
                        depthMaterial->SetBuffer(0, 2, instanceIds);
                        if (masked)
                            BindMaterialTable(*depthMaterial, m_MaskedDepthTextureVersions[DepthProgramIndex(selectedProgram)], context);
                        if (!depthMaterial->Bind())
                            continue;
                        boundMaterial = depthMaterial;
                    }
                    RenderAPI::TryGet()->SetVertexLayout(vertexBuffer->GetLayout());
                    Ref<VertexBuffer> boundVertexBuffer = vertexBuffer;
                    RenderAPI::TryGet()->SetVertexBuffers(0, &boundVertexBuffer, 1);
                    RenderAPI::TryGet()->SetIndexBuffer(indexBuffer);
                    RenderAPI::TryGet()->SetDrawMode(m_Scene->GetGeometryDrawMode(run.Bin.GeometryHeap));
                    RenderAPI::TryGet()->DrawIndexedIndirect(commands, run.FirstCommand * sizeof(DrawIndexedIndirectCommand), run.CommandCount);
                }
            }

            void BuildHiZ(RenderGraphContext& context)
            {
                if (!Ensure(m_BuildHiZ, m_BuildHiZAttempted, "Resources/Shaders/BuildHiZ.asset"))
                    return;
                const Ref<Texture> sceneDepth = TextureResource(context, "SceneDepth");
                const Ref<Texture> hiZ = TextureResource(context, "CurrentHiZ");
                if (!sceneDepth || !hiZ)
                    return;

                const uint32_t mipCount = hiZ->GetDesc().MipLevels + 1u;
                HiZConstants constants;
                constants.DestinationSize = { hiZ->GetWidth(), hiZ->GetHeight() };
                constants.SourceMip = 0;
                constants.CopySource = 1;
                m_BuildHiZ.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_BuildHiZ.SetTexture(0, 1, sceneDepth);
                m_BuildHiZ.SetLoadStoreTexture(0, 2, hiZ, TextureSurface(0, 1, 0, 1));
                m_BuildHiZ.Dispatch((constants.DestinationSize.x + 7u) / 8u, (constants.DestinationSize.y + 7u) / 8u);

                constants.CopySource = 0;
                m_BuildHiZ.SetTexture(0, 1, hiZ);
                for (uint32_t mip = 1; mip < mipCount; mip++)
                {
                    constants.DestinationSize = { std::max(hiZ->GetWidth() >> mip, 1u), std::max(hiZ->GetHeight() >> mip, 1u) };
                    constants.SourceMip = mip - 1u;
                    m_BuildHiZ.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                    m_BuildHiZ.SetLoadStoreTexture(0, 2, hiZ, TextureSurface(mip, 1, 0, 1));
                    m_BuildHiZ.Dispatch((constants.DestinationSize.x + 7u) / 8u, (constants.DestinationSize.y + 7u) / 8u);
                }
            }

            LightingViewConstants BuildLightingViewConstants() const
            {
                const uint32_t width = std::max(static_cast<uint32_t>(m_View.ViewportSize.x), 1u);
                const uint32_t height = std::max(static_cast<uint32_t>(m_View.ViewportSize.y), 1u);
                const ClusteredLightGridDesc clusterDesc =
                  ClusteredLightBuilder::ResolveDesc(m_Settings, width, height, NearPlane(), 1000.0f, m_View.VisibilityMask);
                const glm::uvec3 dimensions = ClusteredLightBuilder::GetDimensions(clusterDesc);
                LightingViewConstants constants;
                constants.ViewProjection = m_View.Projection * m_View.View;
                constants.View = m_View.View;
                constants.InverseViewProjection = glm::inverse(constants.ViewProjection);
                constants.CameraPositionPreExposure = glm::inverse(m_View.View)[3];
                constants.CameraPositionPreExposure.w = 1.0f;
                constants.ClusterDimensionsAndTileSize = { dimensions.x, dimensions.y, dimensions.z, clusterDesc.TileSize };
                constants.ClusterDepthAndViewport = { clusterDesc.NearPlane, clusterDesc.FarPlane, static_cast<float>(width),
                                                      static_cast<float>(height) };
                return constants;
            }

            EnvironmentConstants BuildEnvironmentConstants() const
            {
                EnvironmentConstants constants;
                if (m_Environment && m_Environment->GetPrefilteredMap())
                {
                    constants.DiffuseSh = m_Environment->GetDiffuseSh();
                    constants.SpecularMipCount = static_cast<float>(m_Environment->GetPrefilteredMap()->GetDesc().MipLevels);
                    constants.Intensity = 1.0f;
                }
                return constants;
            }

            void EnsureLightingResources()
            {
                if (!m_BrdfAttempted && AssetManager::TryGet() != nullptr)
                {
                    m_BrdfAttempted = true;
                    const AssetHandle<Texture> brdf = AssetManager::TryGet()->Load<Texture>("Resources/Textures/Brdf.asset");
                    m_BrdfLut = brdf ? brdf.GetInternalPtr() : nullptr;
                }
                if (!m_ShadowSampler)
                {
                    SamplerStateDesc desc;
                    desc.AddressMode = { TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE, TextureWrap::CLAMP_TO_EDGE };
                    desc.CompareFunc = CompareFunction::GREATER_EQUAL;
                    m_ShadowSampler = SamplerState::Create(desc);
                }
            }

            template <typename T> void BindSharedLighting(T& material, RenderGraphContext& context)
            {
                EnsureLightingResources();
                const LightingViewConstants view = BuildLightingViewConstants();
                const EnvironmentConstants environment = BuildEnvironmentConstants();
                material.WriteUniformBlock(0, 0, &view, sizeof(view));
                material.SetBuffer(0, 3, Buffer(context, "LightTable"));
                material.SetBuffer(0, 4, Buffer(context, "ClusterCells"));
                material.SetBuffer(0, 5, Buffer(context, "ClusterLightIndices"));
                material.SetBuffer(0, 6, Buffer(context, "DirectionalLightIndices"));
                material.SetBuffer(0, 7, Buffer(context, "ClusterLightCounters"));
                material.WriteUniformBlock(0, 8, &environment, sizeof(environment));
                material.SetTexture(0, 9, m_Environment ? m_Environment->GetPrefilteredMap() : nullptr);
                material.SetTexture(0, 10, m_BrdfLut);
                material.SetBuffer(0, 11, m_Scene->GetShadowLightBuffer());
                material.SetBuffer(0, 12, m_Scene->GetShadowViewBuffer());
                material.SetTexture(0, 13, TextureResource(context, "ShadowAtlas"));
                material.SetTexture(0, 14, TextureResource(context, "PointShadowArray"));
                material.SetTexture(0, 15, TextureResource(context, "DirectionalShadowArray"));
                material.SetSamplerState(0, 13, m_ShadowSampler);
                material.SetSamplerState(0, 14, m_ShadowSampler);
                material.SetSamplerState(0, 15, m_ShadowSampler);
            }

            template <typename T> void BindMaterialTable(T& material, uint64_t& textureVersion, RenderGraphContext& context)
            {
                material.SetBuffer(1, 0, Buffer(context, "MaterialTable"));
                if (textureVersion == m_Scene->GetBindlessTextureVersion())
                    return;
                const Vector<Ref<Texture>>& textures = m_Scene->GetBindlessTextures();
                material.SetTextureArray(1, 1, textures.empty() ? nullptr : textures.data(), static_cast<uint32_t>(textures.size()));
                textureVersion = m_Scene->GetBindlessTextureVersion();
            }

            void DrawCpuOpaqueRuns(GraphicsMaterial& material, const Ref<GenericGpuBuffer>& commands, const GpuDrawList& drawList, bool skipGpuBins,
                                   bool includeForwardOnly = false)
            {
                if (commands == nullptr || !material.Bind())
                    return;
                for (const GpuDrawRun& run : drawList.Runs)
                {
                    const bool supportedPhase =
                      run.Bin.Phase == RenderDrawPhase::Opaque || (includeForwardOnly && run.Bin.Phase == RenderDrawPhase::ForwardOpaque);
                    if (!supportedPhase || (run.Bin.Alpha != AlphaMode::Opaque && run.Bin.Alpha != AlphaMode::Mask) || run.CommandCount == 0)
                        continue;
                    if (skipGpuBins && m_DrawBinLayout != nullptr && m_DrawBinLayout->Contains(run.Bin))
                        continue;
                    const Ref<VertexBuffer> vertexBuffer = m_Scene->GetGeometryVertexBuffer(run.Bin.GeometryHeap);
                    const Ref<IndexBuffer> indexBuffer = m_Scene->GetGeometryIndexBuffer(run.Bin.GeometryHeap);
                    if (!vertexBuffer || !indexBuffer)
                        continue;
                    RenderAPI::TryGet()->SetVertexLayout(vertexBuffer->GetLayout());
                    Ref<VertexBuffer> boundVertexBuffer = vertexBuffer;
                    RenderAPI::TryGet()->SetVertexBuffers(0, &boundVertexBuffer, 1);
                    RenderAPI::TryGet()->SetIndexBuffer(indexBuffer);
                    RenderAPI::TryGet()->SetDrawMode(m_Scene->GetGeometryDrawMode(run.Bin.GeometryHeap));
                    RenderAPI::TryGet()->DrawIndexedIndirect(commands, run.FirstCommand * sizeof(DrawIndexedIndirectCommand), run.CommandCount);
                }
            }

            bool DrawGpuOpaqueBins(GraphicsMaterial& material, const Ref<GenericGpuBuffer>& commands, const Ref<GenericGpuBuffer>& counts)
            {
                if (!m_GpuDrawCompactionReady || m_DrawBinLayout == nullptr || commands == nullptr || counts == nullptr ||
                    m_DrawBinLayout->GetBins().empty() || !RenderAPI::TryGet()->GetCapabilities().HasCapability(CW_DRAW_INDIRECT_COUNT))
                    return false;
                for (const GpuDrawBin& bin : m_DrawBinLayout->GetBins())
                {
                    if (bin.Key.Phase != RenderDrawPhase::Opaque || (bin.Key.Alpha != AlphaMode::Opaque && bin.Key.Alpha != AlphaMode::Mask) ||
                        bin.CommandCapacity == 0)
                        continue;
                    if (!m_Scene->GetGeometryVertexBuffer(bin.Key.GeometryHeap) || !m_Scene->GetGeometryIndexBuffer(bin.Key.GeometryHeap))
                        return false;
                }
                if (!material.Bind())
                    return false;

                for (const GpuDrawBin& bin : m_DrawBinLayout->GetBins())
                {
                    if (bin.Key.Phase != RenderDrawPhase::Opaque || (bin.Key.Alpha != AlphaMode::Opaque && bin.Key.Alpha != AlphaMode::Mask) ||
                        bin.CommandCapacity == 0)
                        continue;
                    const Ref<VertexBuffer> vertexBuffer = m_Scene->GetGeometryVertexBuffer(bin.Key.GeometryHeap);
                    const Ref<IndexBuffer> indexBuffer = m_Scene->GetGeometryIndexBuffer(bin.Key.GeometryHeap);
                    RenderAPI::TryGet()->SetVertexLayout(vertexBuffer->GetLayout());
                    Ref<VertexBuffer> boundVertexBuffer = vertexBuffer;
                    RenderAPI::TryGet()->SetVertexBuffers(0, &boundVertexBuffer, 1);
                    RenderAPI::TryGet()->SetIndexBuffer(indexBuffer);
                    RenderAPI::TryGet()->SetDrawMode(m_Scene->GetGeometryDrawMode(bin.Key.GeometryHeap));
                    RenderAPI::TryGet()->DrawIndexedIndirectCount(commands, bin.FirstCommand * sizeof(DrawIndexedIndirectCommand), counts,
                                                                  bin.CountIndex * sizeof(uint32_t), bin.CommandCapacity);
                }
                return true;
            }

            void RenderForwardPlus(RenderGraphContext& context)
            {
                if (m_DepthDrawList == nullptr || m_DepthDrawList->Commands.empty() ||
                    !Ensure(m_ForwardPlus, m_ForwardPlusAttempted, "Resources/Shaders/ForwardPlusStandard.asset"))
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                const Ref<GenericGpuBuffer> instanceIds = Buffer(context, "DepthInstanceIds");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "DepthIndirectCommands");
                if (!instances || !instanceIds || !commands)
                    return;

                RenderGraphRenderTargetDesc attachments;
                attachments.Colors[0] = Resource("HdrColor");
                attachments.Colors[1] = Resource("MaterialID");
                attachments.ColorCount = Resource("MaterialID") ? 2 : 1;
                if (Resource("ObjectID"))
                {
                    attachments.Colors[2] = Resource("ObjectID");
                    attachments.ColorCount = 3;
                }
                attachments.Depth = Resource("SceneDepth");
                const Ref<RenderTarget> target = context.GetRenderTarget(attachments);
                if (!target)
                    return;

                m_ForwardPlus.SetBuffer(0, 1, instances);
                BindSharedLighting(m_ForwardPlus, context);
                m_ForwardPlus.SetTexture(
                  0, 16, TextureResource(context, "AmbientOcclusion") ? TextureResource(context, "AmbientOcclusion") : Texture::WHITE);
                BindMaterialTable(m_ForwardPlus, m_ForwardTextureVersion, context);
                const RenderSurfaceMask loadMask(static_cast<uint32_t>(RT_DEPTH_STENCIL) |
                                                 (Resource("ObjectID") ? static_cast<uint32_t>(RT_COLOR2) : 0u));
                RenderAPI::TryGet()->SetRenderTarget(target, 0, loadMask);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                RenderAPI::TryGet()->ClearViewport(FBT_COLOR, glm::vec4(0.0f), 0.0f, 0, static_cast<uint8_t>(RT_COLOR0 | RT_COLOR1));
                const Ref<GenericGpuBuffer> gpuInstanceIds = Buffer(context, "VisibleDrawInstances");
                const Ref<GenericGpuBuffer> gpuCommands = Buffer(context, "IndirectCommands");
                const Ref<GenericGpuBuffer> gpuCounts = Buffer(context, "IndirectDrawCounts");
                m_ForwardPlus.SetBuffer(0, 2, gpuInstanceIds);
                const bool gpuSubmitted = DrawGpuOpaqueBins(m_ForwardPlus, gpuCommands, gpuCounts);
                m_ForwardPlus.SetBuffer(0, 2, instanceIds);
                DrawCpuOpaqueRuns(m_ForwardPlus, commands, *m_DepthDrawList, gpuSubmitted);
            }

            void RenderDeferredGBuffer(RenderGraphContext& context)
            {
                if (m_DepthDrawList == nullptr || m_DepthDrawList->Commands.empty() ||
                    !Ensure(m_DeferredGBuffer, m_DeferredGBufferAttempted, "Resources/Shaders/DeferredPlusStandard.asset"))
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                const Ref<GenericGpuBuffer> instanceIds = Buffer(context, "DepthInstanceIds");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "DepthIndirectCommands");
                if (!instances || !instanceIds || !commands)
                    return;

                RenderGraphRenderTargetDesc attachments;
                attachments.Colors[0] = Resource("GBufferBaseColorAO");
                attachments.Colors[1] = Resource("GBufferNormalRoughMetal");
                attachments.Colors[2] = Resource("GBufferEmissive");
                attachments.Colors[3] = Resource("GBufferMaterialFlags");
                attachments.ColorCount = 4;
                if (Resource("ObjectID"))
                {
                    attachments.Colors[4] = Resource("ObjectID");
                    attachments.ColorCount = 5;
                }
                attachments.Depth = Resource("SceneDepth");
                const Ref<RenderTarget> target = context.GetRenderTarget(attachments);
                if (!target)
                    return;

                const LightingViewConstants view = BuildLightingViewConstants();
                m_DeferredGBuffer.WriteUniformBlock(0, 0, &view, sizeof(view));
                m_DeferredGBuffer.SetBuffer(0, 1, instances);
                BindMaterialTable(m_DeferredGBuffer, m_DeferredTextureVersion, context);
                const RenderSurfaceMask loadMask(static_cast<uint32_t>(RT_DEPTH_STENCIL) |
                                                 (Resource("ObjectID") ? static_cast<uint32_t>(RT_COLOR4) : 0u));
                RenderAPI::TryGet()->SetRenderTarget(target, 0, loadMask);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                RenderAPI::TryGet()->ClearViewport(FBT_COLOR, glm::vec4(0.0f), 0.0f, 0,
                                                   static_cast<uint8_t>(static_cast<uint32_t>(RT_COLOR0) | static_cast<uint32_t>(RT_COLOR1) |
                                                                        static_cast<uint32_t>(RT_COLOR2) | static_cast<uint32_t>(RT_COLOR3)));
                const Ref<GenericGpuBuffer> gpuInstanceIds = Buffer(context, "VisibleDrawInstances");
                const Ref<GenericGpuBuffer> gpuCommands = Buffer(context, "IndirectCommands");
                const Ref<GenericGpuBuffer> gpuCounts = Buffer(context, "IndirectDrawCounts");
                m_DeferredGBuffer.SetBuffer(0, 2, gpuInstanceIds);
                const bool gpuSubmitted = DrawGpuOpaqueBins(m_DeferredGBuffer, gpuCommands, gpuCounts);
                m_DeferredGBuffer.SetBuffer(0, 2, instanceIds);
                DrawCpuOpaqueRuns(m_DeferredGBuffer, commands, *m_DepthDrawList, gpuSubmitted);
            }

            void RenderDeferredLighting(RenderGraphContext& context)
            {
                if (!Ensure(m_DeferredLighting, m_DeferredLightingAttempted, "Resources/Shaders/DeferredPlusLighting.asset"))
                    return;
                BindSharedLighting(m_DeferredLighting, context);
                m_DeferredLighting.SetTexture(0, 16, TextureResource(context, "GBufferBaseColorAO"));
                m_DeferredLighting.SetTexture(0, 17, TextureResource(context, "GBufferNormalRoughMetal"));
                m_DeferredLighting.SetTexture(0, 18, TextureResource(context, "GBufferEmissive"));
                m_DeferredLighting.SetTexture(0, 19, TextureResource(context, "GBufferMaterialFlags"));
                m_DeferredLighting.SetTexture(0, 20, TextureResource(context, "SceneDepth"));
                m_DeferredLighting.SetLoadStoreTexture(0, 21, TextureResource(context, "HdrColor"));
                m_DeferredLighting.SetTexture(
                  0, 22, TextureResource(context, "AmbientOcclusion") ? TextureResource(context, "AmbientOcclusion") : Texture::WHITE);
                BindMaterialTable(m_DeferredLighting, m_DeferredLightingTextureVersion, context);
                const uint32_t width = std::max(static_cast<uint32_t>(m_View.ViewportSize.x), 1u);
                const uint32_t height = std::max(static_cast<uint32_t>(m_View.ViewportSize.y), 1u);
                m_DeferredLighting.Dispatch((width + 7u) / 8u, (height + 7u) / 8u);
            }

            void RenderToonOutlines(RenderGraphContext& context)
            {
                if (!Ensure(m_ToonOutlines, m_ToonOutlinesAttempted, "Resources/Shaders/ToonOutlines.asset"))
                    return;
                const Ref<Texture> depth = TextureResource(context, "SceneDepth");
                const Ref<Texture> materialId = TextureResource(context, "MaterialID");
                const Ref<Texture> hdrColor = TextureResource(context, "HdrColor");
                const Ref<GenericGpuBuffer> materials = Buffer(context, "MaterialTable");
                if (!depth || !materialId || !hdrColor || !materials)
                    return;

                const uint32_t width = std::max(static_cast<uint32_t>(m_View.ViewportSize.x), 1u);
                const uint32_t height = std::max(static_cast<uint32_t>(m_View.ViewportSize.y), 1u);
                ToonOutlineConstants constants;
                constants.InverseViewProjection = glm::inverse(m_View.Projection * m_View.View);
                constants.CameraPosition = glm::vec4(glm::inverse(m_View.View)[3]);
                constants.ResolutionAndFlags = { width, height, Resource("GBufferNormalRoughMetal") ? 1u : 0u, 0u };
                m_ToonOutlines.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_ToonOutlines.SetTexture(0, 1, depth);
                m_ToonOutlines.SetTexture(0, 2, materialId);
                m_ToonOutlines.SetTexture(
                  0, 3, TextureResource(context, "GBufferNormalRoughMetal") ? TextureResource(context, "GBufferNormalRoughMetal") : Texture::NORMAL);
                m_ToonOutlines.SetBuffer(0, 4, materials);
                m_ToonOutlines.SetLoadStoreTexture(0, 5, hdrColor);
                m_ToonOutlines.Dispatch((width + 7u) / 8u, (height + 7u) / 8u);
            }

            void RenderTransparency(RenderGraphContext& context)
            {
                if (m_DepthDrawList == nullptr || m_DepthDrawList->Commands.empty())
                    return;
                const Ref<GenericGpuBuffer> instances = Buffer(context, "InstanceTable");
                const Ref<GenericGpuBuffer> instanceIds = Buffer(context, "DepthInstanceIds");
                const Ref<GenericGpuBuffer> commands = Buffer(context, "DepthIndirectCommands");
                if (!instances || !instanceIds || !commands)
                    return;

                bool needsPremultiplied = false;
                bool needsAdditive = false;
                for (const GpuDrawRun& run : m_DepthDrawList->Runs)
                {
                    if (run.Bin.Phase != RenderDrawPhase::Transparent || run.CommandCount == 0)
                        continue;
                    needsAdditive |= run.Bin.Alpha == AlphaMode::Additive;
                    needsPremultiplied |= run.Bin.Alpha == AlphaMode::Premultiplied || run.Bin.Alpha == AlphaMode::WeightedOIT;
                }
                if (needsPremultiplied && !EnsureTransparent(m_ForwardPremultiplied, m_ForwardPremultipliedAttempted, false))
                    return;
                if (needsAdditive && !EnsureTransparent(m_ForwardAdditive, m_ForwardAdditiveAttempted, true))
                    return;

                auto prepare = [&](GraphicsMaterial& material, uint64_t& textureVersion) {
                    material.SetBuffer(0, 1, instances);
                    material.SetBuffer(0, 2, instanceIds);
                    BindSharedLighting(material, context);
                    material.SetTexture(0, 16,
                                        TextureResource(context, "AmbientOcclusion") ? TextureResource(context, "AmbientOcclusion") : Texture::WHITE);
                    BindMaterialTable(material, textureVersion, context);
                };
                if (needsPremultiplied)
                    prepare(m_ForwardPremultiplied, m_PremultipliedTextureVersion);
                if (needsAdditive)
                    prepare(m_ForwardAdditive, m_AdditiveTextureVersion);

                RenderGraphRenderTargetDesc attachments;
                attachments.Colors[0] = Resource("HdrColor");
                attachments.ColorCount = 1;
                attachments.Depth = Resource("SceneDepth");
                const Ref<RenderTarget> target = context.GetRenderTarget(attachments);
                if (!target)
                    return;
                RenderAPI::TryGet()->SetRenderTarget(target, 0, RT_DEPTH_STENCIL);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);

                GraphicsMaterial* boundMaterial = nullptr;
                for (const GpuDrawRun& run : m_DepthDrawList->Runs)
                {
                    if (run.Bin.Phase != RenderDrawPhase::Transparent || run.CommandCount == 0)
                        continue;
                    GraphicsMaterial* material = run.Bin.Alpha == AlphaMode::Additive ? &m_ForwardAdditive : &m_ForwardPremultiplied;
                    if (material != boundMaterial)
                    {
                        if (!material->Bind())
                            continue;
                        boundMaterial = material;
                    }
                    const Ref<VertexBuffer> vertexBuffer = m_Scene->GetGeometryVertexBuffer(run.Bin.GeometryHeap);
                    const Ref<IndexBuffer> indexBuffer = m_Scene->GetGeometryIndexBuffer(run.Bin.GeometryHeap);
                    if (!vertexBuffer || !indexBuffer)
                        continue;
                    RenderAPI::TryGet()->SetVertexLayout(vertexBuffer->GetLayout());
                    Ref<VertexBuffer> boundVertexBuffer = vertexBuffer;
                    RenderAPI::TryGet()->SetVertexBuffers(0, &boundVertexBuffer, 1);
                    RenderAPI::TryGet()->SetIndexBuffer(indexBuffer);
                    RenderAPI::TryGet()->SetDrawMode(m_Scene->GetGeometryDrawMode(run.Bin.GeometryHeap));
                    RenderAPI::TryGet()->DrawIndexedIndirect(commands, run.FirstCommand * sizeof(DrawIndexedIndirectCommand), run.CommandCount);
                }
            }

            void RenderTemporalResolve(RenderGraphContext& context)
            {
                if (!Ensure(m_TemporalResolve, m_TemporalResolveAttempted, "Resources/Shaders/TemporalResolve.asset"))
                    return;
                const Ref<Texture> current = TextureResource(context, "HdrColor");
                const Ref<Texture> depth = TextureResource(context, "SceneDepth");
                const Ref<Texture> velocity = TextureResource(context, "Velocity");
                const Ref<Texture> history = TextureResource(context, "TaaHistoryRead");
                const Ref<Texture> historyOutput = TextureResource(context, "TaaHistoryWrite");
                const Ref<Texture> resolved = TextureResource(context, "TemporalResolve");
                if (!current || !depth || !velocity || !history || !historyOutput || !resolved)
                    return;
                TemporalConstants constants;
                constants.Resolution = { resolved->GetWidth(), resolved->GetHeight() };
                constants.HistoryValid = context.IsHistoryValid(Resource("TaaHistoryRead")) && !m_View.CameraCut ? 1u : 0u;
                m_TemporalResolve.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_TemporalResolve.SetTexture(0, 1, current);
                m_TemporalResolve.SetTexture(0, 2, depth);
                m_TemporalResolve.SetTexture(0, 3, velocity);
                m_TemporalResolve.SetTexture(0, 4, history);
                m_TemporalResolve.SetLoadStoreTexture(0, 5, resolved);
                m_TemporalResolve.SetLoadStoreTexture(0, 6, historyOutput);
                m_TemporalResolve.Dispatch((constants.Resolution.x + 7u) / 8u, (constants.Resolution.y + 7u) / 8u);
            }

            void RenderBloom(RenderGraphContext& context)
            {
                if (!Ensure(m_Bloom, m_BloomAttempted, "Resources/Shaders/Bloom.asset"))
                    return;
                const Ref<Texture> source = TextureResource(context, "ResolvedColor");
                const Ref<Texture> output = TextureResource(context, "Bloom");
                if (!source || !output)
                    return;
                BloomConstants constants;
                constants.OutputSize = { output->GetWidth(), output->GetHeight() };
                m_Bloom.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                m_Bloom.SetTexture(0, 1, source);
                m_Bloom.SetLoadStoreTexture(0, 2, output);
                m_Bloom.Dispatch((constants.OutputSize.x + 7u) / 8u, (constants.OutputSize.y + 7u) / 8u);
            }

            void RenderToneMap(RenderGraphContext& context)
            {
                if (!Ensure(m_ToneMap, m_ToneMapAttempted, "Resources/Shaders/ToneMap.asset"))
                    return;
                const RenderGraphResourceHandle output = Resource("OutputTarget");
                const Ref<RenderTarget>& target = output ? context.GetRenderTarget(output) : nullptr;
                if (!target)
                    return;
                ToneMapConstants constants;
                constants.BloomIntensity = Resource("Bloom") ? 0.08f : 0.0f;
                m_ToneMap.SetTexture(0, 0, TextureResource(context, "ResolvedColor"));
                m_ToneMap.WriteUniformBlock(0, 1, &constants, sizeof(constants));
                m_ToneMap.SetTexture(0, 2, TextureResource(context, "ObjectID"));
                m_ToneMap.SetTexture(0, 3, TextureResource(context, "SceneDepth"));
                m_ToneMap.SetTexture(0, 4, TextureResource(context, "Bloom") ? TextureResource(context, "Bloom") : Texture::BLACK);
                RenderAPI::TryGet()->SetRenderTarget(target, 0, RT_ALL);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                RenderAPI::TryGet()->ClearViewport(FBT_COLOR, glm::vec4(0.0f), 0.0f, 0, 1u);
                if (m_ToneMap.Bind())
                    RenderAPI::TryGet()->Draw(0, 3, 1);
            }

            void RenderSkyAndForwardOnlyOpaque(RenderGraphContext& context)
            {
                RenderGraphRenderTargetDesc attachments;
                attachments.Colors[0] = Resource("HdrColor");
                attachments.ColorCount = 1;
                attachments.Depth = Resource("SceneDepth");
                const Ref<RenderTarget> target = context.GetRenderTarget(attachments);
                if (!target)
                    return;
                RenderAPI::TryGet()->SetRenderTarget(target, FBT_DEPTH, RT_ALL);
                RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
                if (Ensure(m_Sky, m_SkyAttempted, "Resources/Shaders/Sky.asset"))
                {
                    SkyConstants constants;
                    constants.InverseViewProjection = glm::inverse(m_View.Projection * m_View.View);
                    constants.CameraPositionIntensity = glm::inverse(m_View.View)[3];
                    constants.CameraPositionIntensity.w = m_Environment && m_Environment->GetEnvironmentCubemap() ? 1.0f : 0.0f;
                    m_Sky.WriteUniformBlock(0, 0, &constants, sizeof(constants));
                    m_Sky.SetTexture(0, 1, m_Environment ? m_Environment->GetEnvironmentCubemap() : nullptr);
                    if (m_Sky.Bind())
                        RenderAPI::TryGet()->Draw(0, 3, 1);
                }

                if (m_Snapshot == nullptr || !m_Scene->HasForwardOnlyOpaqueMaterials())
                    return;
                ForwardRenderer::SetPolygonMode(m_Snapshot->OverridePolygonMode);
                ForwardRenderer::Begin();
                ForwardRenderer::BeginForwardOnlyScene(m_Snapshot->ProjectionMatrix, m_Snapshot->ViewMatrix, m_Snapshot->CameraPosition,
                                                       m_Snapshot->Environment);
                ForwardRenderer::SetLights(m_Snapshot->LegacyLights.begin(), static_cast<uint32_t>(m_Snapshot->LegacyLights.Size()));
                const VisibilityFrustum frustum =
                  VisibilityFrustum::FromViewProjection(m_View.Projection * m_View.View, RenderAPI::GetAPI() == RenderAPI::API::Vulkan);
                for (const RenderableObject& object : m_Snapshot->MeshObjects)
                {
                    if (!IsRenderableObjectVisible(object, frustum, m_View.VisibilityMask))
                        continue;
                    ForwardRenderer::SubmitForwardOnlyOpaque(object.MeshHandle, object.Materials, object.WorldMatrix);
                }
                ForwardRenderer::Flush();
                ForwardRenderer::EndScene();
                ForwardRenderer::End();
                ForwardRenderer::SetPolygonMode(PolygonMode::Solid);
            }

            float NearPlane() const
            {
                const float candidate = std::abs(m_View.Projection[3][2]);
                return std::max(candidate, 0.001f);
            }

            RenderView m_View;
            const RenderBlackboard* m_Blackboard = nullptr;
            GpuScene* m_Scene = nullptr;
            const GpuDrawList* m_DepthDrawList = nullptr;
            const GpuDrawBinLayout* m_DrawBinLayout = nullptr;
            const RenderSnapshot* m_Snapshot = nullptr;
            Ref<EnvironmentMap> m_Environment;
            RenderPipelineSettings m_Settings;
            ComputeMaterial m_CullInstances;
            ComputeMaterial m_ExpandMeshlets;
            ComputeMaterial m_CullMeshlets;
            ComputeMaterial m_BinAndCompactDraws;
            ComputeMaterial m_BuildClusters;
            ComputeMaterial m_BuildHiZ;
            ComputeMaterial m_Gtao;
            ComputeMaterial m_DeferredLighting;
            ComputeMaterial m_ToonOutlines;
            ComputeMaterial m_TemporalResolve;
            ComputeMaterial m_Bloom;
            GraphicsMaterial m_Depth;
            GraphicsMaterial m_AnimatedDepth;
            GraphicsMaterial m_DepthObjectID;
            GraphicsMaterial m_AnimatedDepthObjectID;
            Array<GraphicsMaterial, DEPTH_PROGRAM_COUNT> m_MaskedDepth;
            GraphicsMaterial m_ShadowDepth;
            GraphicsMaterial m_ForwardPlus;
            GraphicsMaterial m_ForwardPremultiplied;
            GraphicsMaterial m_ForwardAdditive;
            GraphicsMaterial m_DeferredGBuffer;
            GraphicsMaterial m_ToneMap;
            GraphicsMaterial m_Sky;
            Ref<Texture> m_BrdfLut;
            Ref<SamplerState> m_ShadowSampler;
            Vector<uint32_t> m_ZeroDrawBinCounts;
            uint64_t m_ForwardTextureVersion = 0;
            uint64_t m_PremultipliedTextureVersion = 0;
            uint64_t m_AdditiveTextureVersion = 0;
            uint64_t m_DeferredTextureVersion = 0;
            uint64_t m_DeferredLightingTextureVersion = 0;
            uint64_t m_ShadowTextureVersion = 0;
            Array<uint64_t, DEPTH_PROGRAM_COUNT> m_MaskedDepthTextureVersions{};
            bool m_CullInstancesAttempted = false;
            bool m_ExpandMeshletsAttempted = false;
            bool m_CullMeshletsAttempted = false;
            bool m_BinAndCompactDrawsAttempted = false;
            bool m_GpuInstanceCullingReady = false;
            bool m_GpuMeshletExpansionReady = false;
            bool m_GpuMeshletCullingReady = false;
            bool m_GpuDrawCompactionReady = false;
            bool m_BuildClustersAttempted = false;
            bool m_BuildHiZAttempted = false;
            bool m_GtaoAttempted = false;
            bool m_DepthAttempted = false;
            bool m_AnimatedDepthAttempted = false;
            bool m_DepthObjectIDAttempted = false;
            bool m_AnimatedDepthObjectIDAttempted = false;
            Array<bool, DEPTH_PROGRAM_COUNT> m_MaskedDepthAttempted{};
            bool m_ShadowDepthAttempted = false;
            bool m_ForwardPlusAttempted = false;
            bool m_ForwardPremultipliedAttempted = false;
            bool m_ForwardAdditiveAttempted = false;
            bool m_DeferredGBufferAttempted = false;
            bool m_DeferredLightingAttempted = false;
            bool m_ToonOutlinesAttempted = false;
            bool m_TemporalResolveAttempted = false;
            bool m_BloomAttempted = false;
            bool m_ToneMapAttempted = false;
            bool m_SkyAttempted = false;
            bool m_BrdfAttempted = false;
        };

        struct SceneRendererThreadResources
        {
            struct HistoryConfiguration
            {
                RenderingPath Path = RenderingPath::Auto;
                bool MotionVectors = true;

                bool operator==(const HistoryConfiguration&) const = default;
            };

            SceneRendererThreadResources() : GraphResources(2, &GraphAllocator), SpotShadowAtlas(2048, 128), PointShadowLayers(16) {}

            RenderGraph Graph;
            RenderGraphGpuResourceAllocator GraphAllocator;
            RenderGraphResourceRegistry GraphResources;
            RenderPipelineAsset Pipeline;
            RenderBlackboard Blackboard;
            GpuDrivenPassExecutor GpuDrivenExecutor;
            GpuDrawList DepthDrawList;
            UnorderedSet<uint32_t> PendingShadowUpdates;
            UnorderedSet<uint32_t> RenderedShadowLights;
            ShadowUpdateScheduler ShadowScheduler;
            Vector<RenderLightHandle> ScheduledShadows;
            Vector<ShadowUpdateRequest> ShadowBudgetRequests;
            UnorderedSet<uint32_t> ScheduledShadowSet;
            UnorderedSet<uint32_t> ActiveSpotLights;
            UnorderedSet<uint32_t> ActivePointLights;
            Vector<GpuShadowLightData> ShadowLights;
            Vector<GpuShadowViewData> ShadowViews;
            Vector<ShadowRenderView> ShadowRenderViews;
            ShadowAtlasAllocator SpotShadowAtlas;
            PointShadowLayerAllocator PointShadowLayers;
            UnorderedSet<uint32_t> PreviousSpotLights;
            UnorderedMap<uint64_t, HistoryConfiguration> HistoryConfigurations;
        };

        thread_local Scope<SceneRendererThreadResources> s_RenderThreadResources;

        SceneRendererThreadResources& GetSceneRendererThreadResources()
        {
            if (!s_RenderThreadResources)
                s_RenderThreadResources = CreateScope<SceneRendererThreadResources>();
            return *s_RenderThreadResources;
        }
    } // namespace

    struct SceneRendererData
    {
        uint32_t ViewportWidth, ViewportHeight;

        Ref<Material> GridMaterial;
        Ref<VertexBuffer> GridVbo;
        Ref<IndexBuffer> GridIbo;

        // Ref<TimerQuery> Timer2DGeometry = nullptr;
        // Ref<TimerQuery> Timer3DGeometry = nullptr;

        // Ref<PipelineQuery> PipelineQuery = nullptr;

        // Ref<RayTracingPipeline> RayPipeline = nullptr;
        // Ref<AccelerationStructure> Accel = nullptr;

        // AssetHandle<Font> GlobalFont;
    };

    static SceneRendererData* s_Data;
    static uint32_t s_RendererInstances;
    static std::mutex s_StatisticsMutex;
    static SceneRenderStatistics s_Statistics;

    namespace
    {
        void PublishStatistics(const SceneRenderStatistics& statistics)
        {
            std::scoped_lock lock(s_StatisticsMutex);
            s_Statistics = statistics;
        }

        void Add2DStatistics(const RenderSnapshot& snapshot, SceneRenderStatistics& statistics)
        {
            statistics.VisibleVertices += static_cast<uint64_t>(snapshot.Sprites.Size()) * 6u;
            statistics.VisibleTriangles += static_cast<uint64_t>(snapshot.Sprites.Size()) * 2u;
            for (const RenderableText& text : snapshot.Texts)
            {
                statistics.VisibleVertices += static_cast<uint64_t>(text.TextData.Text.size()) * 6u;
                statistics.VisibleTriangles += static_cast<uint64_t>(text.TextData.Text.size()) * 2u;
            }
        }

        SceneRenderStatistics BuildLegacyStatistics(const RenderSnapshot& snapshot)
        {
            SceneRenderStatistics statistics;
            statistics.FrameNumber = snapshot.FrameNumber;
            const VisibilityFrustum frustum =
              VisibilityFrustum::FromViewProjection(snapshot.ProjectionMatrix * snapshot.ViewMatrix, RenderAPI::GetAPI() == RenderAPI::API::Vulkan);
            for (const RenderableObject& object : snapshot.MeshObjects)
            {
                if (!object.MeshHandle || !IsRenderableObjectVisible(object, frustum, RenderLayerMask::All()))
                    continue;
                statistics.VisibleInstances++;
                statistics.LogicalDraws++;
                const uint64_t elementCount =
                  object.MeshHandle->GetIndexCount() != 0 ? object.MeshHandle->GetIndexCount() : object.MeshHandle->GetVertexCount();
                statistics.VisibleVertices += elementCount;
                const DrawMode drawMode = object.MeshHandle->GetDrawMode();
                if (drawMode == DrawMode::TRIANGLE_LIST || drawMode == DrawMode::TRIANGLE_STRIP || drawMode == DrawMode::TRIANGLE_FAN)
                    statistics.VisibleTriangles += RenderAPI::GetPrimitiveCount(drawMode, elementCount);
            }
            Add2DStatistics(snapshot, statistics);

            const GpuSceneUploadStats& gpuStatistics = Renderer::GetGpuScene().GetStats();
            statistics.UploadedBytes = gpuStatistics.UploadedBytes;
            statistics.ActiveInstances = gpuStatistics.ActiveInstances;
            statistics.ActiveLights = gpuStatistics.ActiveLights;
            return statistics;
        }
    } // namespace

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene, const Ref<RenderTarget>& renderTarget,
                                 RenderHistoryReleaseSink* historyReleaseSink)
      : m_RenderTarget(renderTarget), m_Scene(scene), m_HistoryReleaseSink(historyReleaseSink),
        m_HistoryOwnerId(AllocateHistoryOwnerId())
    {
        s_RendererInstances++;
    }

    SceneRenderer::~SceneRenderer()
    {
        DispatchHistoryReleases();
        CW_ENGINE_ASSERT(s_RendererInstances != 0, "Scene renderer instance ownership is unbalanced");
        s_RendererInstances--;
        if (s_RendererInstances == 0)
        {
            delete s_Data;
            s_Data = nullptr;
        }
    }

    SceneRenderStatistics SceneRenderer::GetStatistics()
    {
        std::scoped_lock lock(s_StatisticsMutex);
        return s_Statistics;
    }

    void SceneRenderer::Init()
    {
        if (s_Data != nullptr)
            return;
        s_Data = new SceneRendererData();
        // s_Data->Timer2DGeometry = TimerQuery::Create();
        // s_Data->Timer3DGeometry = TimerQuery::Create();

        // s_Data->PipelineQuery = PipelineQuery::Create();

#if raytracing
        static Ref<Shader> rayTraceShader = Importer::Get().Import<Shader>("Resources/Shaders/RayTrace.glsl");
        static const AssetHandle<Shader> rayTraceHandle = static_asset_cast<Shader>(AssetManager::TryGet()->CreateAssetHandle(rayTraceShader));
        rayTraceShader->GetTechniques()[0]->GetRenderPasses()[0]->Compile();

        float vertices[] = { 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f };
        uint32_t indices[] = { 0, 1, 2 };
        const glm::mat3x4 transformMatrix(1.0f);
        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create({ sizeof(vertices), BufferUsage::BU_STATIC_DRAW, vertices });
        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create({ 3, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, indices });

        s_Data->RayPipeline = rayTraceShader->GetTechniques()[0]->GetRenderPasses()[0]->GetRayTracingPipeline();
        AccelerationGeometry geom;
        geom.Transform = transformMatrix;
        geom.UseTransform = true;
        geom.Type = GeometryType::Triangles;
        GeometryTriangles tris;
        tris.VertexBuffer = vertexBuffer;
        tris.IndexBuffer = indexBuffer;
        tris.IndexCount = 3;
        tris.VertexCount = 9;
        tris.IndexFormat = IndexType::Index_32;
        tris.VertexStride = 12;
        geom.GeometryData.Triangles = tris;

        static Ref<AccelerationStructure> blas = AccelerationStructure::Create({ geom }, false, 1, AccelerationStructBuildBits::PreferFastTrace);
        AccelerationInstance instance;
        instance.BottomLevelAccel = blas.get();
        instance.Transform = transformMatrix;
        s_Data->Accel = AccelerationStructure::Create({}, true, 1, AccelerationStructBuildBits::PreferFastTrace);

        Ref<CommandBuffer> cmdBuf = CommandBuffer::Create(GpuQueueType::GRAPHICS_QUEUE);
        blas->BuildBottomLevel(cmdBuf, &geom, 1, AccelerationStructBuildBits::PreferFastTrace);
        s_Data->Accel->BuildTopLevel(cmdBuf, &instance, 1, AccelerationStructBuildBits::PreferFastTrace);
        RenderAPI::TryGet()->SubmitCommandBuffer(cmdBuf, 0);
#endif

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Data.GlobalFont = static_asset_cast<Font>(AssetManager::TryGet()->CreateAssetHandle(font));

        // Editor grid
        {
            const AssetHandle<Shader> gridHandle = AssetManager::TryGet()->Load<Shader>(GRID_SHADER_PATH);
            s_Data->GridMaterial = Material::Create(gridHandle);

            const float gridExtent = 500.0f;
            float gridVertices[] = {
                -gridExtent, 0.0f, -gridExtent, gridExtent, 0.0f, -gridExtent, gridExtent, 0.0f, gridExtent, -gridExtent, 0.0f, gridExtent,
            };
            uint32_t gridIndices[] = { 0, 1, 2, 0, 2, 3 };
            s_Data->GridVbo = VertexBuffer::Create({ sizeof(gridVertices), BufferUsage::BU_STATIC_DRAW, gridVertices });
            s_Data->GridVbo->SetLayout(CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } }));
            s_Data->GridIbo = IndexBuffer::Create({ 6, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, gridIndices });
        }
    }

    void SceneRenderer::RenderEditor(const EditorCamera& camera, bool drawGrid, const GridSettings& gridSettings)
    {
        Render(camera, camera.GetViewMatrix(), drawGrid, gridSettings);
    }

    void SceneRenderer::Render()
    {
        // Get the main camera to render from the scene
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform;
        const auto cameraView = m_Scene->m_Registry.view<TransformComponent, CameraComponent, RelationshipComponent>();
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera, relationship] = cameraView.get<TransformComponent, CameraComponent, RelationshipComponent>(ee);
            mainCamera = &camera.Camera;
            cameraTransform = transform.GetWorldMatrix(relationship.Parent);
            break;
        }

        // Render the scene
        if (mainCamera)
            Render(*mainCamera, glm::inverse(cameraTransform));
    }

    void SceneRenderer::DrawGrid(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const GridSettings& settings)
    {
        RenderAPI& rapi = (*RenderAPI::TryGet());
        s_Data->GridMaterial->SetMatrix("viewProjection"_hstr, viewProjection);
        s_Data->GridMaterial->SetVector3("cameraPos"_hstr, cameraPos);
        s_Data->GridMaterial->SetFloat("fineSize"_hstr, settings.FineSize);
        s_Data->GridMaterial->SetFloat("coarseSize"_hstr, settings.CoarseSize);
        s_Data->GridMaterial->SetFloat("lineWidth"_hstr, settings.LineWidth);
        s_Data->GridMaterial->SetFloat("opacity"_hstr, settings.Opacity);
        s_Data->GridMaterial->SetInt("showAxes"_hstr, settings.ShowAxes ? 1 : 0);
        rapi.SetGraphicsPipeline(s_Data->GridMaterial->GetGraphicsPipeline());
        rapi.SetVertexBuffers(0, &s_Data->GridVbo, 1);
        rapi.SetVertexLayout(s_Data->GridVbo->GetLayout());
        rapi.SetIndexBuffer(s_Data->GridIbo);
        rapi.SetUniforms(s_Data->GridMaterial->GetUniformParams());
        rapi.DrawIndexed(0, s_Data->GridIbo->GetCount(), 0, 4);
    }

    void SceneRenderer::Render(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid, const GridSettings& gridSettings)
    {
        FrameMarkStart("Editor update");
        RenderAPI& rapi = (*RenderAPI::TryGet());
        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);
#if raytracing
        rapi.SetRayTracingPipeline(s_Data->RayPipeline);
        rapi.TraceRays(s_Data->ViewportWidth, s_Data->ViewportHeight);

        return;
#endif
        {
            ZoneScopedN("Forward begin");
            // TODO: Combine these, or name them better
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(camera, viewTransform, m_Scene->GetEnvironment());
        }
        {
            ZoneScopedN("Forward render");
            auto objs = m_Scene->m_Registry.view<MeshRendererComponent, TransformComponent, RelationshipComponent>();
            for (const entt::entity ee : objs)
            {
                auto [mesh, transform, relationship] = objs.get<MeshRendererComponent, TransformComponent, RelationshipComponent>(ee);

                const AnimationComponent* animation = m_Scene->m_Registry.try_get<AnimationComponent>(ee);
                const AssetHandle<Mesh> renderMesh =
                  animation != nullptr && animation->RuntimeMeshHandle ? animation->RuntimeMeshHandle : mesh.MeshHandle;
                if (renderMesh && mesh.Visible)
                {
                    ForwardRenderer::Submit(renderMesh, mesh.Materials, transform.GetWorldMatrix(relationship.Parent));
                    // TODO: Update stats... triangle count has to take into account the draw mode
                }
            }

            // Procedural meshes (direct render path)
            auto procView = m_Scene->m_Registry.view<ProceduralMeshComponent, TransformComponent, RelationshipComponent>();
            for (const entt::entity ee : procView)
            {
                auto [proc, transform, relationship] = procView.get<ProceduralMeshComponent, TransformComponent, RelationshipComponent>(ee);
                if (proc.RuntimeMeshHandle)
                {
                    ForwardRenderer::Submit(proc.RuntimeMeshHandle, proc.Materials, transform.GetWorldMatrix(relationship.Parent));
                }
            }
        }
        {
            ZoneScopedN("Forward end");
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();
        }

        if (drawGrid)
        {
            ZoneScopedN("Editor grid");
            DrawGrid(camera.GetProjection() * viewTransform, camera.GetPosition(), gridSettings);
        }

        {
            ZoneScopedN("2D render");
            Renderer2D::Begin(camera, viewTransform);
            const auto spriteRendererComponents = m_Scene->m_Registry.view<SpriteRendererComponent, TransformComponent, RelationshipComponent>();
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [sprite, transform, relationship] =
                  spriteRendererComponents.get<SpriteRendererComponent, TransformComponent, RelationshipComponent>(ee);
                Renderer2D::FillRect(transform.GetWorldMatrix(relationship.Parent), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr,
                                     sprite.Color, ((int32_t)ee) + 1);
            }
            const auto textComponents = m_Scene->m_Registry.view<TextComponent, TransformComponent, RelationshipComponent>();
            for (const entt::entity ee : textComponents)
            {
                auto [text, transform, relationship] = textComponents.get<TextComponent, TransformComponent, RelationshipComponent>(ee);
                Renderer2D::DrawString(text, transform.GetWorldMatrix(relationship.Parent), (int32_t)ee + 1);
            }
            Renderer2D::End();
        }

        FrameMarkEnd("Editor update");
    }

    void SceneRenderer::UpdateProceduralMeshes()
    {
        ZoneScopedN("UpdateProceduralMeshes");
        auto view = m_Scene->m_Registry.view<ProceduralMeshComponent>();
        for (const entt::entity ee : view)
        {
            auto& proc = m_Scene->m_Registry.get<ProceduralMeshComponent>(ee);

            if (proc.PendingGpuResult && proc.GpuUploadPending)
            {
                Ref<Mesh> uploadedMesh;
                if (proc.PendingGpuResult->TryConsume(uploadedMesh))
                {
                    proc.GpuUploadPending = false;
                    if (uploadedMesh)
                    {
                        proc.GpuMesh = std::move(uploadedMesh);
                        if (!proc.RuntimeMeshHandle || proc.RuntimeMeshHandle.GetInternalPtr() != proc.GpuMesh)
                            proc.RuntimeMeshHandle = static_asset_cast<Mesh>(AssetManager::TryGet()->CreateAssetHandle(proc.GpuMesh));
                        proc.NeedsGpuUpload = false;
                    }
                }
            }
            if (proc.GpuUploadPending)
                continue;

            if (!proc.Graph.IsLoaded())
                continue;

            NodeGraphAsset* asset = proc.Graph.Get();
            if (asset == nullptr || asset->GetGraph() == nullptr)
                continue;

            const Ref<NodeGraph> graph = asset->GetGraph();

            // Check if the graph has changed since the last evaluation.
            // NeedsEvaluation is a manual override flag that can also be used.
            const uint32_t currentVersion = graph->GetVersion();
            if (!proc.NeedsEvaluation && proc.LastEvaluatedVersion == currentVersion)
                continue;

            // Evaluate the node graph on the sim thread (CPU only, no GPU calls)
            Ref<MeshData> result = graph->EvaluateGeometry(proc.InputValues);
            if (!result || result->GetVertexCount() == 0)
            {
                proc.LastEvaluatedVersion = currentVersion;
                proc.NeedsEvaluation = false;
                continue;
            }

            proc.CpuMeshData = result;
            proc.NeedsEvaluation = false;
            proc.LastEvaluatedVersion = currentVersion;
            proc.NeedsGpuUpload = true;

            RenderThread* rt = Application::TryGet()->GetRenderThread();
            if (rt && rt->IsRunning())
            {
                if (!proc.PendingGpuResult)
                    proc.PendingGpuResult = std::make_shared<MeshUploadResult>();
                proc.PendingGpuResult->ResetForSubmission();
                proc.GpuUploadPending = true;

                MeshDesc uploadDescription;
                uploadDescription.Data = result;
                uploadDescription.Usage = MeshUsage::Dynamic | MeshUsage::CpuCached;
                rt->EnqueueMeshUpload(MeshUploadCommand(proc.GpuMesh, std::move(uploadDescription), proc.PendingGpuResult));
            }
            else
            {
                // Single-threaded fallback: create/upload directly and apply immediately
                if (proc.GpuMesh)
                {
                    proc.GpuMesh->SetMeshData(result);
                    proc.GpuMesh->UploadToGpu();
                }
                else
                {
                    proc.GpuMesh = Mesh::Create({ result, MeshUsage::Dynamic | MeshUsage::CpuCached });
                }
                if (!proc.RuntimeMeshHandle || proc.RuntimeMeshHandle.GetInternalPtr() != proc.GpuMesh)
                    proc.RuntimeMeshHandle = static_asset_cast<Mesh>(AssetManager::TryGet()->CreateAssetHandle(proc.GpuMesh));
                proc.NeedsGpuUpload = false;
                proc.GpuUploadPending = false;
            }
        }
    }

    void SceneRenderer::UpdateAnimations(Timestep timestep)
    {
        ZoneScopedN("UpdateAnimations");
        auto view = m_Scene->m_Registry.view<AnimationComponent, MeshRendererComponent>();
        for (const entt::entity handle : view)
        {
            auto [animation, meshRenderer] = view.get<AnimationComponent, MeshRendererComponent>(handle);

            if (!animation.Clip || !meshRenderer.MeshHandle)
            {
                if (animation.Player || animation.Deformer || animation.RuntimeMesh || animation.PendingGpuResult)
                    animation.ResetRuntime(true);
                continue;
            }

            const UUID sourceMeshId = meshRenderer.MeshHandle.GetUUID();
            const UUID clipId = animation.Clip.GetUUID();
            if (!animation.Player || !animation.Deformer || animation.RuntimeSourceMesh != sourceMeshId || animation.RuntimeClip != clipId)
            {
                animation.ResetRuntime(true);
                if (!meshRenderer.MeshHandle->IsCpuCached())
                {
                    CW_ENGINE_WARN("Animated mesh '{}' is not CPU cached. Reimport it with bones or morph targets enabled.",
                                   meshRenderer.MeshHandle->GetName());
                    continue;
                }

                animation.Deformer = CreateRef<MeshDeformer>();
                if (!animation.Deformer->Initialize(meshRenderer.MeshHandle->GetMeshData(), meshRenderer.MeshHandle->GetSkeleton(),
                                                    meshRenderer.MeshHandle->GetMorph()))
                {
                    animation.ResetRuntime(true);
                    continue;
                }
                animation.Player = CreateRef<AnimationPlayer>();
                animation.InitializeRuntimePlayback();
                animation.RuntimeSourceMesh = sourceMeshId;
                animation.RuntimeClip = clipId;
                animation.PendingGpuResult = std::make_shared<MeshUploadResult>();
            }

            RenderThread* renderThread = Application::TryGet()->GetRenderThread();
            Ref<Mesh> uploadedMesh;
            if (animation.PendingGpuResult && animation.GpuUploadPending && animation.PendingGpuResult->TryConsume(uploadedMesh))
            {
                animation.RuntimeMesh = std::move(uploadedMesh);
                animation.GpuUploadPending = false;
                if (animation.RuntimeMesh && !animation.RuntimeMeshHandle)
                    animation.RuntimeMeshHandle = static_asset_cast<Mesh>(AssetManager::TryGet()->CreateAssetHandle(animation.RuntimeMesh));
            }
            else if (animation.GpuUploadPending && (!renderThread || !renderThread->IsRunning()))
            {
                animation.PendingGpuResult->ResetForSubmission();
                animation.GpuUploadPending = false;
            }

            animation.Player->SetSpeed(animation.Speed);
            animation.Player->SetWrapMode(animation.WrapMode);
            animation.Player->Update(std::max(0.0f, timestep.GetSeconds()), meshRenderer.MeshHandle->GetSkeleton(),
                                     meshRenderer.MeshHandle->GetMorph());
            animation.SynchronizeRuntimePlayback();

            if (animation.ApplyRootMotion)
            {
                Entity entity(handle, m_Scene.get());
                const Transform& delta = animation.Player->GetRootMotionDelta();
                entity.SetPosition(entity.GetLocalPosition() + delta.GetPosition());
                entity.SetRotation(glm::normalize(entity.GetLocalRotation() * delta.GetRotation()));
            }

            // Animation time, events, and root motion must not depend on render-thread upload latency.
            // If the renderer is behind, retain the last uploaded deformation and publish the newest pose
            // as soon as its single in-flight upload completes.
            if (animation.GpuUploadPending)
                continue;

            const SkeletonPose* pose = meshRenderer.MeshHandle->GetSkeleton() ? &animation.Player->GetPose() : nullptr;
            if (!animation.Deformer->Deform(pose, animation.Player->GetMorphWeights()) || !animation.Deformer->WasLastDeformChanged())
                continue;

            const Ref<MeshData> output = animation.Deformer->GetOutputMeshData();
            const Ref<Mesh> existingMesh = animation.RuntimeMesh;
            const std::shared_ptr<MeshUploadResult> resultSlot = animation.PendingGpuResult;
            resultSlot->ResetForSubmission();
            animation.GpuUploadPending = true;

            MeshDesc runtimeDesc;
            runtimeDesc.Data = output;
            if (!existingMesh)
            {
                runtimeDesc.Usage = MeshUsage::Dynamic | MeshUsage::CpuCached;
                runtimeDesc.Topology = meshRenderer.MeshHandle->GetDrawMode();
                runtimeDesc.Morph = meshRenderer.MeshHandle->GetMorph();
                runtimeDesc.MeshSkeleton = meshRenderer.MeshHandle->GetSkeleton();
                runtimeDesc.SubMeshes = meshRenderer.MeshHandle->GetSubMeshes();
            }
            if (renderThread && renderThread->IsRunning())
            {
                renderThread->EnqueueMeshUpload(MeshUploadCommand(existingMesh, std::move(runtimeDesc), resultSlot));
            }
            else
            {
                if (animation.RuntimeMesh)
                {
                    animation.RuntimeMesh->SetMeshData(output);
                    animation.RuntimeMesh->UploadToGpu();
                }
                else
                {
                    animation.RuntimeMesh = Mesh::Create(runtimeDesc);
                    animation.RuntimeMeshHandle = static_asset_cast<Mesh>(AssetManager::TryGet()->CreateAssetHandle(animation.RuntimeMesh));
                }
                animation.GpuUploadPending = false;
            }
        }
    }

    RenderSnapshot SceneRenderer::ExtractSnapshot(bool drawGrid) const
    {
        RenderSnapshot snapshot;
        ExtractSnapshot(snapshot, drawGrid);
        return snapshot;
    }

    void SceneRenderer::ExtractSnapshot(RenderSnapshot& snapshot, bool drawGrid) const
    {
        const uint64_t frameNumber = snapshot.FrameNumber != 0 ? snapshot.FrameNumber : CurrentSimulationFrameNumber();
        snapshot.Clear();
        snapshot.FrameNumber = frameNumber;
        Camera* mainCamera = nullptr;
        entt::entity mainCameraEntity = entt::null;
        glm::mat4 cameraTransform;
        const auto cameraView = m_Scene->m_Registry.view<TransformComponent, CameraComponent, RelationshipComponent>();
        // TODO: Requires improvement for multiple cameras, maybe a main camera checkbox?
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera, relationship] = cameraView.get<TransformComponent, CameraComponent, RelationshipComponent>(ee);
            mainCamera = &camera.Camera;
            mainCameraEntity = ee;
            cameraTransform = transform.GetWorldMatrix(relationship.Parent);
            break;
        }

        if (mainCamera)
        {
            ExtractSnapshotWithHistory(snapshot, *mainCamera, glm::inverse(cameraTransform),
                                       SceneCameraHistoryNamespace(m_HistoryOwnerId, m_Scene.get(), mainCameraEntity), drawGrid);
            return;
        }

        // No camera in scene — return a snapshot that carries the render target so
        // RenderFromSnapshot can still clear/present the frame without crashing.
        snapshot.Target = m_RenderTarget;
        snapshot.HistoryOwnerId = m_HistoryOwnerId;
        snapshot.DrawGrid = drawGrid;
        AdvanceCameraHistoryEpoch(snapshot.FrameNumber);
        TransferHistoryReleases(snapshot);
        SyncRenderWorld(snapshot);
    }

    RenderSnapshot SceneRenderer::ExtractSnapshot(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid) const
    {
        RenderSnapshot snapshot;
        ExtractSnapshot(snapshot, camera, viewTransform, drawGrid);
        return snapshot;
    }

    void SceneRenderer::ExtractSnapshot(RenderSnapshot& snapshot, const Camera& camera, const glm::mat4& viewTransform, bool drawGrid) const
    {
        ExtractSnapshotWithHistory(snapshot, camera, viewTransform, ExternalCameraHistoryNamespace(m_HistoryOwnerId, m_Scene.get(), &camera),
                                   drawGrid);
    }

    void SceneRenderer::ExtractSnapshotWithHistory(RenderSnapshot& snapshot, const Camera& camera, const glm::mat4& viewTransform,
                                                   uint64_t historyNamespace, bool drawGrid) const
    {
        ZoneScopedN("ExtractSnapshot");
        const uint64_t frameNumber = snapshot.FrameNumber != 0 ? snapshot.FrameNumber : CurrentSimulationFrameNumber();
        snapshot.Clear();
        snapshot.FrameNumber = frameNumber;
        snapshot.ProjectionMatrix = camera.GetProjection();
        snapshot.ViewMatrix = viewTransform;
        snapshot.HistoryOwnerId = m_HistoryOwnerId;
        snapshot.HistoryNamespace = historyNamespace;
        AdvanceCameraHistoryEpoch(snapshot.FrameNumber);
        const glm::mat4 cameraWorld = glm::inverse(viewTransform);
        const glm::vec3 cameraPosition = glm::vec3(cameraWorld[3]);
        snapshot.CameraPosition = cameraPosition;
        const glm::vec3 cameraForward = glm::normalize(-glm::vec3(cameraWorld[2]));
        const auto history = m_CameraHistory.find(snapshot.HistoryNamespace);
        snapshot.CameraCut = history == m_CameraHistory.end();
        if (history != m_CameraHistory.end())
        {
            snapshot.PreviousViewProjection = history->second.Projection * history->second.View;
            snapshot.CameraCut = ProjectionChanged(history->second.Projection, snapshot.ProjectionMatrix) ||
                                 glm::distance(history->second.Position, cameraPosition) > 10.0f ||
                                 glm::dot(history->second.Forward, cameraForward) < 0.8660254f;
        }
        else
        {
            snapshot.PreviousViewProjection = snapshot.ProjectionMatrix * snapshot.ViewMatrix;
        }
        m_CameraHistory.insert_or_assign(snapshot.HistoryNamespace, CameraHistoryState{ snapshot.ViewMatrix, snapshot.ProjectionMatrix,
                                                                                        cameraPosition, cameraForward, m_CameraHistoryEpoch });
        snapshot.Target = m_RenderTarget;
        snapshot.Environment = m_Scene->GetEnvironment();
        snapshot.DrawGrid = drawGrid;
        TransferHistoryReleases(snapshot);
        SyncRenderWorld(snapshot);

        // 3D mesh objects
        {
            auto objs = m_Scene->m_Registry.view<MeshRendererComponent, TransformComponent, RelationshipComponent>();
            snapshot.MeshObjects.Reserve(objs.size_hint());
            for (const entt::entity ee : objs)
            {
                auto [mesh, transform, relationship] = objs.get<MeshRendererComponent, TransformComponent, RelationshipComponent>(ee);
                const AnimationComponent* animation = m_Scene->m_Registry.try_get<AnimationComponent>(ee);
                const AssetHandle<Mesh> renderMesh =
                  animation != nullptr && animation->RuntimeMeshHandle ? animation->RuntimeMeshHandle : mesh.MeshHandle;
                if (renderMesh)
                {
                    RenderableObject& object = snapshot.MeshObjects.Acquire();
                    object.WorldMatrix = transform.GetWorldMatrix(relationship.Parent);
                    const SphereBounds& bounds = animation != nullptr && animation->RuntimeMeshHandle && animation->Deformer
                                                   ? animation->Deformer->GetSphereBounds()
                                                   : renderMesh->GetSphereBounds();
                    object.BoundingSphere = VisibilityCulling::TransformSphere(bounds, object.WorldMatrix);
                    object.MeshHandle = renderMesh;
                    object.Materials.assign(mesh.Materials.begin(), mesh.Materials.end());
                    object.VisibilityLayers = mesh.VisibilityLayers;
                    object.Visible = mesh.Visible;
                }
            }
        }

        // Procedural mesh objects
        {
            auto procView = m_Scene->m_Registry.view<ProceduralMeshComponent, TransformComponent, RelationshipComponent>();
            for (const entt::entity ee : procView)
            {
                auto [proc, transform, relationship] = procView.get<ProceduralMeshComponent, TransformComponent, RelationshipComponent>(ee);
                if (proc.RuntimeMeshHandle)
                {
                    RenderableObject& object = snapshot.MeshObjects.Acquire();
                    object.WorldMatrix = transform.GetWorldMatrix(relationship.Parent);
                    object.BoundingSphere = VisibilityCulling::TransformSphere(proc.RuntimeMeshHandle->GetSphereBounds(), object.WorldMatrix);
                    object.MeshHandle = proc.RuntimeMeshHandle;
                    object.Materials.assign(proc.Materials.begin(), proc.Materials.end());
                    object.VisibilityLayers = RenderLayerMask::All();
                    object.Visible = true;
                }
            }
        }

        // 2D sprites
        {
            const auto spriteRendererComponents = m_Scene->m_Registry.view<SpriteRendererComponent, TransformComponent, RelationshipComponent>();
            snapshot.Sprites.Reserve(spriteRendererComponents.size_hint());
            snapshot.Ordered2D.Reserve(spriteRendererComponents.size_hint());
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [sprite, transform, relationship] =
                  spriteRendererComponents.get<SpriteRendererComponent, TransformComponent, RelationshipComponent>(ee);
                RenderableSprite& renderable = snapshot.Sprites.Acquire();
                renderable.WorldMatrix = transform.GetWorldMatrix(relationship.Parent);
                renderable.Texture = sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr;
                renderable.Color = sprite.Color;
                renderable.EntityId = ((int32_t)ee) + 1;
                Renderable2DOrder& order = snapshot.Ordered2D.Acquire();
                order.Type = Renderable2DType::Sprite;
                order.Index = static_cast<uint32_t>(snapshot.Sprites.Size() - 1u);
                order.SortingLayer = sprite.SortingLayer;
                order.OrderInLayer = sprite.OrderInLayer;
                order.StableOrder = static_cast<uint32_t>(entt::to_integral(ee));
            }
        }

        // Text
        {
            const auto textComponents = m_Scene->m_Registry.view<TextComponent, TransformComponent, RelationshipComponent>();
            snapshot.Texts.Reserve(textComponents.size_hint());
            snapshot.Ordered2D.Reserve(snapshot.Ordered2D.Size() + textComponents.size_hint());
            for (const entt::entity ee : textComponents)
            {
                auto [text, transform, relationship] = textComponents.get<TextComponent, TransformComponent, RelationshipComponent>(ee);
                RenderableText& renderable = snapshot.Texts.Acquire();
                renderable.TextData = text;
                renderable.WorldMatrix = transform.GetWorldMatrix(relationship.Parent);
                renderable.EntityId = (int32_t)ee + 1;
                Renderable2DOrder& order = snapshot.Ordered2D.Acquire();
                order.Type = Renderable2DType::Text;
                order.Index = static_cast<uint32_t>(snapshot.Texts.Size() - 1u);
                order.SortingLayer = text.SortingLayer;
                order.OrderInLayer = text.OrderInLayer;
                order.StableOrder = static_cast<uint32_t>(entt::to_integral(ee));
            }
        }

        std::sort(snapshot.Ordered2D.begin(), snapshot.Ordered2D.end(), Renderable2DOrderLess);
    }

    uint32_t SceneRenderer::GetResourceIndex(const AssetHandleData* identity, UnorderedMap<const AssetHandleData*, uint32_t>& resources,
                                             uint32_t& nextIndex) const
    {
        if (identity == nullptr)
            return 0;
        const auto existing = resources.find(identity);
        if (existing != resources.end())
            return existing->second;
        if (nextIndex > 0x00ffffffu)
        {
            CW_ENGINE_ERROR("Render resource table exhausted its 24-bit handle space");
            return 0;
        }
        const uint32_t index = nextIndex++;
        resources.emplace(identity, index);
        return index;
    }

    uint32_t SceneRenderer::GetMaterialSetIndex(const Vector<AssetHandle<Material>>& materials) const
    {
        if (materials.empty())
            return 0;

        const auto existing = m_MaterialSetIndices.find(materials);
        if (existing != m_MaterialSetIndices.end())
            return existing->second;

        const uint32_t count = static_cast<uint32_t>(materials.size());
        if (count > 0x00ffffffu || m_NextMaterialResourceIndex > 0x00ffffffu - count + 1u)
        {
            CW_ENGINE_ERROR("Render material table exhausted its 24-bit handle space");
            return 0;
        }

        const uint32_t baseIndex = m_NextMaterialResourceIndex;
        m_NextMaterialResourceIndex += count;
        MaterialSetKey key;
        key.Materials.reserve(materials.size());
        for (const AssetHandle<Material>& material : materials)
            key.Materials.push_back(material.GetHandleData().get());
        m_MaterialSetIndices.emplace(std::move(key), baseIndex);
        return baseIndex;
    }

    void SceneRenderer::TrackMeshResource(uint32_t index, const AssetHandle<Mesh>& mesh, RenderSnapshot& snapshot) const
    {
        if (index == 0)
            return;
        const uint64_t version = mesh ? mesh->GetGpuVersion() : 0;
        const auto resident = m_ResidentMeshResources.find(index);
        if (resident != m_ResidentMeshResources.end())
        {
            resident->second.LastSeenEpoch = m_RenderSyncEpoch;
            if (resident->second.Resource.GetHandleData().get() == mesh.GetHandleData().get() && resident->second.Version == version)
                return;
            resident->second.Resource = mesh;
            resident->second.Version = version;
        }
        else
            m_ResidentMeshResources.emplace(index, TrackedMeshResource{ mesh, version, m_RenderSyncEpoch });

        RenderMeshResourceChange& change = snapshot.MeshResourceChanges.Acquire();
        change.Index = index;
        change.Version = version;
        change.Type = RenderResourceChangeType::CreateOrUpdate;
        change.Resource = mesh;
    }

    void SceneRenderer::TrackMaterialResources(uint32_t baseIndex, const Vector<AssetHandle<Material>>& materials, RenderSnapshot& snapshot) const
    {
        if (baseIndex == 0)
            return;
        for (uint32_t slot = 0; slot < materials.size(); slot++)
        {
            const uint32_t index = baseIndex + slot;
            const AssetHandle<Material>& material = materials[slot];
            const uint64_t version = material ? material->GetParamVersion() : 0;
            const auto resident = m_ResidentMaterialResources.find(index);
            if (resident != m_ResidentMaterialResources.end())
            {
                resident->second.LastSeenEpoch = m_RenderSyncEpoch;
                if (resident->second.Resource.GetHandleData().get() == material.GetHandleData().get() && resident->second.Version == version)
                    continue;
                resident->second.Resource = material;
                resident->second.Version = version;
            }
            else
                m_ResidentMaterialResources.emplace(index, TrackedMaterialResource{ material, version, m_RenderSyncEpoch });

            RenderMaterialResourceChange& change = snapshot.MaterialResourceChanges.Acquire();
            change.Index = index;
            change.Version = version;
            change.Type = RenderResourceChangeType::CreateOrUpdate;
            change.Resource = material;
        }
    }

    void SceneRenderer::FinalizeResourceChanges(RenderSnapshot& snapshot) const
    {
        for (auto resident = m_ResidentMeshResources.begin(); resident != m_ResidentMeshResources.end();)
        {
            if (resident->second.LastSeenEpoch == m_RenderSyncEpoch)
            {
                ++resident;
                continue;
            }
            RenderMeshResourceChange& change = snapshot.MeshResourceChanges.Acquire();
            change.Index = resident->first;
            change.Version = resident->second.Version;
            change.Type = RenderResourceChangeType::Destroy;
            change.Resource = {};
            resident = m_ResidentMeshResources.erase(resident);
        }
        for (auto resident = m_ResidentMaterialResources.begin(); resident != m_ResidentMaterialResources.end();)
        {
            if (resident->second.LastSeenEpoch == m_RenderSyncEpoch)
            {
                ++resident;
                continue;
            }
            RenderMaterialResourceChange& change = snapshot.MaterialResourceChanges.Acquire();
            change.Index = resident->first;
            change.Version = resident->second.Version;
            change.Type = RenderResourceChangeType::Destroy;
            change.Resource = {};
            resident = m_ResidentMaterialResources.erase(resident);
        }
    }

    void SceneRenderer::SyncRenderWorld(RenderSnapshot& snapshot) const
    {
        ZoneScopedN("SyncRenderWorld");
        bool shadowCastersChanged = false;
        const size_t meshChangesBefore = snapshot.MeshResourceChanges.Size();
        const size_t materialChangesBefore = snapshot.MaterialResourceChanges.Size();
        m_RenderSyncEpoch++;
        if (m_RenderSyncEpoch == 0)
        {
            for (auto& [_, instance] : m_TrackedRenderInstances)
                instance.LastSeenEpoch = 0;
            for (auto& [_, light] : m_TrackedRenderLights)
                light.LastSeenEpoch = 0;
            for (auto& [_, mesh] : m_ResidentMeshResources)
                mesh.LastSeenEpoch = 0;
            for (auto& [_, material] : m_ResidentMaterialResources)
                material.LastSeenEpoch = 0;
            m_RenderSyncEpoch = 1;
        }

        auto syncInstance = [&](uint64_t sourceID, entt::entity entity, const AssetHandle<Mesh>& mesh, const Vector<AssetHandle<Material>>& materials,
                                const glm::mat4& transform, RenderInstanceFlags flags, RenderLayerMask visibilityLayers, float lodBias,
                                const SphereBounds* overrideBounds = nullptr) {
            if (!mesh)
                return;

            if (HasForwardOnlyOpaqueMaterial(materials))
                flags = flags | RenderInstanceFlags::ForceLod0;
            const AssetHandleData* meshIdentity = mesh.GetHandleData().get();
            const uint32_t objectID = static_cast<uint32_t>(entt::to_integral(entity)) + 1u;

            const SphereBounds& localBounds = overrideBounds != nullptr ? *overrideBounds : mesh->GetSphereBounds();
            const glm::vec4 worldBounds = VisibilityCulling::TransformSphere(localBounds, transform);

            RenderInstanceDesc desc;
            desc.Transform = transform;
            desc.BoundingSphere = worldBounds;
            desc.MeshHandle = GetResourceIndex(meshIdentity, m_MeshResourceIndices, m_NextMeshResourceIndex);
            desc.MaterialHandle = GetMaterialSetIndex(materials);
            TrackMeshResource(desc.MeshHandle, mesh, snapshot);
            TrackMaterialResources(desc.MaterialHandle, materials, snapshot);
            desc.ObjectID = { objectID };
            desc.Flags = flags;
            desc.VisibilityLayers = visibilityLayers;
            desc.LodBias = lodBias;

            const auto tracked = m_TrackedRenderInstances.find(sourceID);
            if (tracked == m_TrackedRenderInstances.end())
            {
                TrackedRenderInstance instance;
                instance.Handle = m_RenderWorld.CreateInstance(desc);
                instance.Transform = transform;
                instance.BoundingSphere = worldBounds;
                instance.MeshResourceIndex = desc.MeshHandle;
                instance.MaterialResourceIndex = desc.MaterialHandle;
                instance.ObjectID = objectID;
                instance.Flags = flags;
                instance.VisibilityLayers = visibilityLayers;
                instance.LodBias = lodBias;
                instance.LastSeenEpoch = m_RenderSyncEpoch;
                m_TrackedRenderInstances.emplace(sourceID, std::move(instance));
                shadowCastersChanged = shadowCastersChanged || HasFlag(flags, RenderInstanceFlags::CastShadows);
                return;
            }

            TrackedRenderInstance& instance = tracked->second;
            instance.LastSeenEpoch = m_RenderSyncEpoch;
            const bool transformChanged = instance.Transform != transform || instance.BoundingSphere != worldBounds;
            const bool drawChanged = instance.MeshResourceIndex != desc.MeshHandle || instance.MaterialResourceIndex != desc.MaterialHandle ||
                                     instance.ObjectID != objectID || instance.Flags != flags || instance.VisibilityLayers != visibilityLayers ||
                                     instance.LodBias != lodBias;
            if (!transformChanged && !drawChanged)
                return;

            const bool wasShadowCaster = HasFlag(instance.Flags, RenderInstanceFlags::CastShadows);
            const bool isShadowCaster = HasFlag(flags, RenderInstanceFlags::CastShadows);
            shadowCastersChanged = shadowCastersChanged || wasShadowCaster || isShadowCaster;
            m_RenderWorld.UpdateInstance(instance.Handle, desc);
            instance.Transform = transform;
            instance.BoundingSphere = worldBounds;
            instance.MeshResourceIndex = desc.MeshHandle;
            instance.MaterialResourceIndex = desc.MaterialHandle;
            instance.ObjectID = objectID;
            instance.Flags = flags;
            instance.VisibilityLayers = visibilityLayers;
            instance.LodBias = lodBias;
        };

        const auto meshView = m_Scene->m_Registry.view<MeshRendererComponent, TransformComponent, RelationshipComponent>();
        for (const entt::entity entity : meshView)
        {
            const auto [meshRenderer, transform, relationship] =
              meshView.get<MeshRendererComponent, TransformComponent, RelationshipComponent>(entity);
            const AnimationComponent* animation = m_Scene->m_Registry.try_get<AnimationComponent>(entity);
            const AssetHandle<Mesh> mesh =
              animation != nullptr && animation->RuntimeMeshHandle ? animation->RuntimeMeshHandle : meshRenderer.MeshHandle;
            const SphereBounds* animatedBounds =
              animation != nullptr && animation->RuntimeMeshHandle && animation->Deformer ? &animation->Deformer->GetSphereBounds() : nullptr;
            RenderInstanceFlags flags = RenderInstanceFlags::None;
            if (meshRenderer.Visible)
                flags = flags | RenderInstanceFlags::Visible;
            if (meshRenderer.CastShadows)
                flags = flags | RenderInstanceFlags::CastShadows;
            if (meshRenderer.ReceiveShadows)
                flags = flags | RenderInstanceFlags::ReceiveShadows;
            if (meshRenderer.MotionVectors)
                flags = flags | RenderInstanceFlags::MotionVectors;
            syncInstance(meshRenderer.InstanceId, entity, mesh, meshRenderer.Materials, transform.GetWorldMatrix(relationship.Parent), flags,
                         meshRenderer.VisibilityLayers, meshRenderer.LodBias, animatedBounds);
        }

        const auto proceduralView = m_Scene->m_Registry.view<ProceduralMeshComponent, TransformComponent, RelationshipComponent>();
        for (const entt::entity entity : proceduralView)
        {
            const auto [procedural, transform, relationship] =
              proceduralView.get<ProceduralMeshComponent, TransformComponent, RelationshipComponent>(entity);
            syncInstance(procedural.InstanceId, entity, procedural.RuntimeMeshHandle, procedural.Materials,
                         transform.GetWorldMatrix(relationship.Parent),
                         RenderInstanceFlags::Visible | RenderInstanceFlags::CastShadows | RenderInstanceFlags::ReceiveShadows |
                           RenderInstanceFlags::MotionVectors,
                         RenderLayerMask::All(), 0.0f);
        }

        for (auto tracked = m_TrackedRenderInstances.begin(); tracked != m_TrackedRenderInstances.end();)
        {
            if (tracked->second.LastSeenEpoch == m_RenderSyncEpoch)
            {
                ++tracked;
                continue;
            }
            shadowCastersChanged = shadowCastersChanged || HasFlag(tracked->second.Flags, RenderInstanceFlags::CastShadows);
            m_RenderWorld.DestroyInstance(tracked->second.Handle);
            tracked = m_TrackedRenderInstances.erase(tracked);
        }

        shadowCastersChanged = shadowCastersChanged || snapshot.MeshResourceChanges.Size() != meshChangesBefore ||
                               snapshot.MaterialResourceChanges.Size() != materialChangesBefore;
        if (shadowCastersChanged)
        {
            m_ShadowCasterRevision++;
            if (m_ShadowCasterRevision == 0)
                m_ShadowCasterRevision = 1;
        }

        const auto lightView = m_Scene->m_Registry.view<LightComponent, TransformComponent, RelationshipComponent>();
        snapshot.LegacyLights.Reserve(lightView.size_hint());
        for (const entt::entity entity : lightView)
        {
            const auto [component, transform, relationship] = lightView.get<LightComponent, TransformComponent, RelationshipComponent>(entity);

            const glm::mat4 worldTransform = transform.GetWorldMatrix(relationship.Parent);
            RenderLightDesc desc;
            desc.Type = component.Type;
            desc.Position = glm::vec3(worldTransform[3]);
            const glm::vec3 forward = -glm::vec3(worldTransform[2]);
            desc.Direction = glm::dot(forward, forward) > 0.000001f ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, -1.0f);
            desc.Color = component.Color;
            if (component.UseColorTemperature)
                desc.Color *= RenderLightWorld::ColorTemperatureToLinearRgb(component.Temperature);
            desc.Intensity = component.Intensity;
            desc.Range = component.Range;
            desc.SpotInnerAngle = component.SpotInnerAngle;
            desc.SpotOuterAngle = component.SpotOuterAngle;
            desc.SourceRadius = component.SourceRadius;
            desc.VisibilityLayers = component.VisibilityLayers;
            desc.ObjectID = { static_cast<uint32_t>(entt::to_integral(entity)) + 1u };
            desc.Shadows = component.Shadows;
            desc.Flags = RenderLightFlags::None;
            if (component.Enabled)
                desc.Flags = desc.Flags | RenderLightFlags::Enabled;
            if (component.AffectDiffuse)
                desc.Flags = desc.Flags | RenderLightFlags::AffectDiffuse;
            if (component.AffectSpecular)
                desc.Flags = desc.Flags | RenderLightFlags::AffectSpecular;
            if (component.Volumetric)
                desc.Flags = desc.Flags | RenderLightFlags::Volumetric;

            const RenderLightData data = RenderLightWorld::BuildLightData(desc, desc.ObjectID);
            snapshot.LegacyLights.Acquire() = data;
            const auto tracked = m_TrackedRenderLights.find(component.InstanceId);
            RenderLightHandle lightHandle;
            bool requiresShadowRedraw = !component.Shadows.CacheStaticCasters;
            if (tracked == m_TrackedRenderLights.end())
            {
                TrackedRenderLight light;
                light.Handle = m_RenderLightWorld.CreateLight(desc);
                light.Data = data;
                light.Shadows = component.Shadows;
                light.ShadowCasterRevision = m_ShadowCasterRevision;
                light.LastSeenEpoch = m_RenderSyncEpoch;
                lightHandle = light.Handle;
                requiresShadowRedraw = true;
                m_TrackedRenderLights.emplace(component.InstanceId, light);
            }
            else
            {
                tracked->second.LastSeenEpoch = m_RenderSyncEpoch;
                lightHandle = tracked->second.Handle;
                const bool dataChanged = std::memcmp(&tracked->second.Data, &data, sizeof(RenderLightData)) != 0;
                const bool shadowSettingsChanged = !(tracked->second.Shadows == component.Shadows);
                requiresShadowRedraw = RequiresShadowCacheRedraw(component.Shadows, dataChanged || shadowSettingsChanged, m_ShadowCasterRevision,
                                                                 tracked->second.ShadowCasterRevision);
                if (dataChanged || shadowSettingsChanged)
                {
                    m_RenderLightWorld.UpdateLight(tracked->second.Handle, desc);
                    tracked->second.Data = data;
                    tracked->second.Shadows = component.Shadows;
                    requiresShadowRedraw = true;
                }
                tracked->second.ShadowCasterRevision = m_ShadowCasterRevision;
            }

            if (component.Enabled && component.Type != LightType::Directional && component.Shadows.Mode != LightShadowMode::Disabled &&
                lightHandle.IsValid())
            {
                ShadowUpdateRequest& request = snapshot.ShadowUpdateRequests.Acquire();
                request.Light = lightHandle;
                request.Type = component.Type;
                request.Resolution = component.Shadows.Resolution;
                request.Importance = component.Shadows.Importance;
                request.RequiresRedraw = requiresShadowRedraw;
                request.Settings = component.Shadows;
            }
            else if (component.Enabled && component.Type == LightType::Directional && component.Shadows.Mode != LightShadowMode::Disabled &&
                     lightHandle.IsValid() && !snapshot.DirectionalShadow.IsValid() && snapshot.HistoryNamespace != 0 &&
                     std::abs(snapshot.ProjectionMatrix[2][3] + 1.0f) < 0.01f)
            {
                DirectionalShadowRenderData& shadow = snapshot.DirectionalShadow;
                shadow.Light = lightHandle;
                shadow.Settings = component.Shadows;
                shadow.CascadeSettings.CascadeCount = 3;
                shadow.CascadeSettings.Resolution = std::max<uint32_t>(component.Shadows.Resolution, 1024u);
                const glm::mat4 cameraWorld = glm::inverse(snapshot.ViewMatrix);
                const float verticalFov = 2.0f * std::atan(1.0f / std::max(std::abs(snapshot.ProjectionMatrix[1][1]), 0.0001f));
                const float aspect = std::abs(snapshot.ProjectionMatrix[1][1] / std::max(std::abs(snapshot.ProjectionMatrix[0][0]), 0.0001f));
                const float cameraNear =
                  std::max(std::abs(snapshot.ProjectionMatrix[3][2] / std::max(std::abs(snapshot.ProjectionMatrix[2][2]), 0.0001f)), 0.001f);
                m_DirectionalCascadeScratch.clear();
                DirectionalShadowCascadeBuilder::Build(cameraWorld, verticalFov, aspect, cameraNear, desc.Direction, shadow.CascadeSettings,
                                                       m_DirectionalCascadeScratch);
                shadow.CascadeCount = static_cast<uint32_t>(std::min<size_t>(m_DirectionalCascadeScratch.size(), shadow.Cascades.size()));
                for (uint32_t cascade = 0; cascade < shadow.CascadeCount; cascade++)
                    shadow.Cascades[cascade] = m_DirectionalCascadeScratch[cascade];
                shadow.RequiresRedraw = true;
            }
        }

        for (auto tracked = m_TrackedRenderLights.begin(); tracked != m_TrackedRenderLights.end();)
        {
            if (tracked->second.LastSeenEpoch == m_RenderSyncEpoch)
            {
                ++tracked;
                continue;
            }
            m_RenderLightWorld.DestroyLight(tracked->second.Handle);
            tracked = m_TrackedRenderLights.erase(tracked);
        }

        FinalizeResourceChanges(snapshot);

        m_RenderWorld.DrainChanges(m_RenderWorldChangeScratch);
        snapshot.RenderWorldChanges.Reserve(m_RenderWorldChangeScratch.size());
        for (const RenderWorldChange& change : m_RenderWorldChangeScratch)
            snapshot.RenderWorldChanges.Acquire() = change;

        m_RenderLightWorld.DrainChanges(m_RenderLightChangeScratch);
        snapshot.RenderLightChanges.Reserve(m_RenderLightChangeScratch.size());
        for (const RenderLightChange& change : m_RenderLightChangeScratch)
            snapshot.RenderLightChanges.Acquire() = change;
    }

    void SceneRenderer::ResetTrackedRenderWorld()
    {
        for (const auto& [_, instance] : m_TrackedRenderInstances)
            m_RenderWorld.DestroyInstance(instance.Handle);
        m_TrackedRenderInstances.clear();
        for (const auto& [_, light] : m_TrackedRenderLights)
            m_RenderLightWorld.DestroyLight(light.Handle);
        m_TrackedRenderLights.clear();
        m_ShadowCasterRevision = 1;
    }

    void SceneRenderer::RenderLegacySnapshot(const RenderSnapshot& snapshot)
    {
        ZoneScopedN("RenderFromSnapshot");
        if (!snapshot.Target)
            return; // No render target — nothing to draw into this frame.

        RenderAPI& rapi = (*RenderAPI::TryGet());
        rapi.SetRenderTarget(snapshot.Target);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);

        // Forward pass (3D)
        {
            ForwardRenderer::SetPolygonMode(snapshot.OverridePolygonMode);
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(snapshot.ProjectionMatrix, snapshot.ViewMatrix, snapshot.CameraPosition, snapshot.Environment);
            ForwardRenderer::SetLights(snapshot.LegacyLights.begin(), static_cast<uint32_t>(snapshot.LegacyLights.Size()));
            const VisibilityFrustum frustum =
              VisibilityFrustum::FromViewProjection(snapshot.ProjectionMatrix * snapshot.ViewMatrix, RenderAPI::GetAPI() == RenderAPI::API::Vulkan);
            for (const auto& obj : snapshot.MeshObjects)
            {
                if (IsRenderableObjectVisible(obj, frustum, RenderLayerMask::All()))
                    ForwardRenderer::Submit(obj.MeshHandle, obj.Materials, obj.WorldMatrix);
            }
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();
            ForwardRenderer::SetPolygonMode(PolygonMode::Solid); // Reset after frame
        }

        RenderLegacyOverlays(snapshot);
        PublishStatistics(BuildLegacyStatistics(snapshot));
    }

    void SceneRenderer::RenderLegacyOverlays(const RenderSnapshot& snapshot)
    {
        if (!snapshot.Target)
            return;
        RenderAPI& rapi = *RenderAPI::TryGet();
        rapi.SetRenderTarget(snapshot.Target, 0, RT_ALL);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);

        if (snapshot.DrawGrid)
        {
            ZoneScopedN("Editor grid");
            DrawGrid(snapshot.ProjectionMatrix * snapshot.ViewMatrix, snapshot.CameraPosition, snapshot.Grid);
        }

        // 2D pass
        {
            Renderer2D::Begin(snapshot.ProjectionMatrix, snapshot.ViewMatrix);
            const auto drawSprite = [&](const RenderableSprite& sprite) {
                Renderer2D::FillRect(sprite.WorldMatrix, sprite.Texture, sprite.Color, sprite.EntityId);
            };
            const auto drawText = [&](const RenderableText& text) { Renderer2D::DrawString(text.TextData, text.WorldMatrix, text.EntityId); };

            if (snapshot.Ordered2D.Empty())
            {
                // Snapshots produced by older native integrations do not contain the combined order list.
                for (const RenderableSprite& sprite : snapshot.Sprites)
                    drawSprite(sprite);
                for (const RenderableText& text : snapshot.Texts)
                    drawText(text);
            }
            else
            {
                for (const Renderable2DOrder& item : snapshot.Ordered2D)
                {
                    if (item.Type == Renderable2DType::Sprite)
                    {
                        if (item.Index < snapshot.Sprites.Size())
                            drawSprite(snapshot.Sprites[item.Index]);
                    }
                    else if (item.Index < snapshot.Texts.Size())
                    {
                        drawText(snapshot.Texts[item.Index]);
                    }
                }
            }
            Renderer2D::End();
        }
    }

    void SceneRenderer::ShutdownRenderThreadResources()
    {
        s_RenderThreadResources.reset();
    }

    void SceneRenderer::ReleaseRenderThreadHistory(uint64_t historyNamespace)
    {
        if (historyNamespace == 0 || !s_RenderThreadResources)
            return;

        s_RenderThreadResources->GraphResources.ReleaseHistory(historyNamespace);
        s_RenderThreadResources->HistoryConfigurations.erase(historyNamespace);
    }

    void SceneRenderer::RenderFromSnapshot(const RenderSnapshot& snapshot)
    {
        ZoneScopedN("RenderGraphFrame");
        for (uint64_t historyNamespace : snapshot.ReleasedHistoryNamespaces)
            ReleaseRenderThreadHistory(historyNamespace);
        if (!snapshot.Target && !s_RenderThreadResources)
            return;

        SceneRendererThreadResources& threadResources = GetSceneRendererThreadResources();
        RenderGraph& renderGraph = threadResources.Graph;
        RenderGraphResourceRegistry& graphResources = threadResources.GraphResources;
        if (!snapshot.Target)
            return;
        renderGraph.Reset();
        GpuScene& gpuScene = Renderer::GetGpuScene();
        gpuScene.BeginFrame(snapshot.FrameNumber);
        gpuScene.Apply(snapshot.RenderWorldChanges, snapshot.RenderLightChanges);
        gpuScene.ApplyResources(snapshot.MeshResourceChanges, snapshot.MaterialResourceChanges);
        const RenderCapabilities& capabilities = RenderAPI::TryGet()->GetCapabilities();
        const RenderFeatureTier featureTier = capabilities.GetFeatureTier();
        if (featureTier == RenderFeatureTier::Compatibility)
        {
            if (threadResources.HistoryConfigurations.erase(snapshot.HistoryNamespace) != 0)
                graphResources.InvalidateHistory(snapshot.HistoryNamespace);
            RenderLegacySnapshot(snapshot);
            return;
        }

        RenderGraphTextureDesc targetDesc;
        targetDesc.Width = snapshot.Target->GetProperties().Width;
        targetDesc.Height = snapshot.Target->GetProperties().Height;
        targetDesc.Samples = std::max(1u, snapshot.Target->GetProperties().Samples);
        const RenderGraphResourceHandle target =
          renderGraph.ImportTexture("CameraTarget", targetDesc, reinterpret_cast<uint64_t>(snapshot.Target.get()),
                                    RenderGraphResourceState::ColorAttachment, RenderGraphResourceState::ColorAttachment);

        const RenderGraphPassHandle applyChanges = renderGraph.AddPass("ApplyRenderWorldChanges", RenderGraphQueue::Transfer,
                                                                       [](RenderGraphPassBuilder& builder) { builder.SetSideEffect(); });

        RenderGraphResourceHandle instanceTable;
        if (gpuScene.GetInstanceBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetInstanceBuffer()->GetSize(), sizeof(RenderInstanceData), GpuBufferType::Structured };
            instanceTable = renderGraph.ImportBuffer("PersistentInstances", desc, reinterpret_cast<uint64_t>(gpuScene.GetInstanceBuffer().get()),
                                                     RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }
        RenderGraphResourceHandle lightTable;
        if (gpuScene.GetLightBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetLightBuffer()->GetSize(), sizeof(RenderLightData), GpuBufferType::Structured };
            lightTable = renderGraph.ImportBuffer("PersistentLights", desc, reinterpret_cast<uint64_t>(gpuScene.GetLightBuffer().get()),
                                                  RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }
        RenderGraphResourceHandle meshTable;
        if (gpuScene.GetMeshBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetMeshBuffer()->GetSize(), sizeof(GpuMeshRecord), GpuBufferType::Structured };
            meshTable = renderGraph.ImportBuffer("PersistentMeshes", desc, reinterpret_cast<uint64_t>(gpuScene.GetMeshBuffer().get()),
                                                 RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }
        RenderGraphResourceHandle meshLodTable;
        if (gpuScene.GetMeshLodBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetMeshLodBuffer()->GetSize(), sizeof(GpuMeshLodData), GpuBufferType::Structured };
            meshLodTable = renderGraph.ImportBuffer("PersistentMeshLods", desc, reinterpret_cast<uint64_t>(gpuScene.GetMeshLodBuffer().get()),
                                                    RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }
        RenderGraphResourceHandle meshletTable;
        if (gpuScene.GetMeshletBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetMeshletBuffer()->GetSize(), sizeof(GpuMeshletData), GpuBufferType::Structured };
            meshletTable = renderGraph.ImportBuffer("PersistentMeshlets", desc, reinterpret_cast<uint64_t>(gpuScene.GetMeshletBuffer().get()),
                                                    RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }
        RenderGraphResourceHandle materialTable;
        if (gpuScene.GetMaterialBuffer())
        {
            const RenderGraphBufferDesc desc{ gpuScene.GetMaterialBuffer()->GetSize(), sizeof(GpuMaterialData), GpuBufferType::Structured };
            materialTable = renderGraph.ImportBuffer("PersistentMaterials", desc, reinterpret_cast<uint64_t>(gpuScene.GetMaterialBuffer().get()),
                                                     RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }

        RenderPipelineAsset& pipeline = threadResources.Pipeline;
        RenderBlackboard& blackboard = threadResources.Blackboard;
        RenderView view;
        view.View = snapshot.ViewMatrix;
        view.Projection = snapshot.ProjectionMatrix;
        view.PreviousViewProjection = snapshot.PreviousViewProjection;
        view.ViewportSize = { static_cast<float>(targetDesc.Width), static_cast<float>(targetDesc.Height) };
        view.EnableObjectID = snapshot.EnableObjectID;
        view.EnableMotionVectors = snapshot.EnableMotionVectors;
        view.CameraCut = snapshot.CameraCut || snapshot.FrameNumber <= 1;
        view.Path = pipeline.ResolvePath(capabilities);
        const SceneRendererThreadResources::HistoryConfiguration historyConfiguration{ view.Path, view.EnableMotionVectors };
        const auto [historyState, newHistoryState] =
          threadResources.HistoryConfigurations.try_emplace(snapshot.HistoryNamespace, historyConfiguration);
        if (!newHistoryState && historyState->second != historyConfiguration)
        {
            historyState->second = historyConfiguration;
            view.CameraCut = true;
        }

        const bool gpuDrawTier = featureTier == RenderFeatureTier::GPUDriven || featureTier == RenderFeatureTier::Future;
        GpuDrawBinLayoutDesc drawBinDesc;
        if (gpuDrawTier && capabilities.MaxDrawIndirectCount != 0)
        {
            drawBinDesc.MaximumCommands = pipeline.GetSettings().MaxIndirectCommands;
            drawBinDesc.MaximumBins = std::min(drawBinDesc.MaximumCommands, 4096u);
            drawBinDesc.MaximumDrawsPerCall = capabilities.MaxDrawIndirectCount;
        }
        gpuScene.PrepareGpuDrawBins(drawBinDesc);
        const bool gpuDrawBinsEnabled = gpuDrawTier && gpuScene.HasGpuDrawBins() && materialTable.IsValid();
        RenderGraphResourceHandle drawBinTable;
        if (gpuDrawBinsEnabled)
        {
            const Ref<GenericGpuBuffer>& buffer = gpuScene.GetGpuDrawBinBuffer();
            drawBinTable = renderGraph.ImportBuffer(
              "PersistentDrawBins", { buffer->GetSize(), sizeof(GpuDrawBinLookupEntry), GpuBufferType::Structured },
              reinterpret_cast<uint64_t>(buffer.get()), RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
        }

        GpuDrawList& depthDrawList = threadResources.DepthDrawList;
        depthDrawList.Clear();
        RenderGraphResourceHandle depthInstanceIds;
        RenderGraphResourceHandle depthCommands;
        if (featureTier != RenderFeatureTier::Compatibility)
        {
            gpuScene.BuildCpuDrawList(view, depthDrawList);
            const GpuDrawBuffers& drawBuffers = gpuScene.GetCpuDrawBuffers();
            if (drawBuffers.GetInstanceIDBuffer())
            {
                const Ref<GenericGpuBuffer>& buffer = drawBuffers.GetInstanceIDBuffer();
                depthInstanceIds = renderGraph.ImportBuffer(
                  "DepthInstanceIds", { buffer->GetSize(), sizeof(GpuVisibleDrawInstance), GpuBufferType::Structured },
                  reinterpret_cast<uint64_t>(buffer.get()), RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
            }
            if (drawBuffers.GetCommandBuffer())
            {
                const Ref<GenericGpuBuffer>& buffer = drawBuffers.GetCommandBuffer();
                depthCommands = renderGraph.ImportBuffer(
                  "DepthIndirectCommands", { buffer->GetSize(), sizeof(DrawIndexedIndirectCommand), GpuBufferType::IndirectDraw },
                  reinterpret_cast<uint64_t>(buffer.get()), RenderGraphResourceState::IndirectArgument, RenderGraphResourceState::IndirectArgument);
            }
        }

        RenderPipelineGraphDesc graphDesc;
        graphDesc.Width = targetDesc.Width;
        graphDesc.Height = targetDesc.Height;
        graphDesc.Path = view.Path;
        graphDesc.OutputTarget = target;
        graphDesc.InstanceTable = instanceTable;
        graphDesc.LightTable = lightTable;
        graphDesc.MeshTable = meshTable;
        graphDesc.MeshLodTable = meshLodTable;
        graphDesc.MeshletTable = meshletTable;
        graphDesc.MaterialTable = materialTable;
        graphDesc.DrawBinTable = drawBinTable;
        graphDesc.DepthInstanceIds = depthInstanceIds;
        graphDesc.DepthIndirectCommands = depthCommands;
        graphDesc.Prerequisite = applyChanges;
        graphDesc.DrawBinCount = gpuDrawBinsEnabled ? static_cast<uint32_t>(gpuScene.GetGpuDrawBinLayout().GetBins().size()) : 0u;
        graphDesc.DrawBinLookupCapacity = gpuDrawBinsEnabled ? static_cast<uint32_t>(gpuScene.GetGpuDrawBinLayout().GetLookupEntries().size()) : 0u;
        graphDesc.EnableGpuDrawBins = gpuDrawBinsEnabled;
        graphDesc.EnablePostProcessing = true;
        GpuDrivenPassExecutor& gpuDrivenExecutor = threadResources.GpuDrivenExecutor;
        if (featureTier == RenderFeatureTier::VulkanBaseline || featureTier == RenderFeatureTier::GPUDriven ||
            featureTier == RenderFeatureTier::Future)
        {
            graphDesc.PassExecutor = [&](StringView name, RenderGraphContext& context) { gpuDrivenExecutor.Execute(name, context); };
        }
        Vector<RenderLightHandle>& scheduledShadows = threadResources.ScheduledShadows;
        uint64_t scheduledShadowPixels = 0;
        graphDesc.ScheduledShadowRenderer = [&](RenderGraphContext& context) {
            const ShadowUpdateBudget mediumBudget;
            UnorderedSet<uint32_t>& pendingShadowUpdates = threadResources.PendingShadowUpdates;
            UnorderedSet<uint32_t>& renderedShadowLights = threadResources.RenderedShadowLights;
            Vector<ShadowUpdateRequest>& budgetRequests = threadResources.ShadowBudgetRequests;
            budgetRequests.clear();
            budgetRequests.reserve(snapshot.ShadowUpdateRequests.Size());
            for (const ShadowUpdateRequest& request : snapshot.ShadowUpdateRequests)
            {
                if (request.RequiresRedraw)
                    pendingShadowUpdates.insert(request.Light.GetValue());
                ShadowUpdateRequest copy = request;
                copy.RequiresRedraw = copy.RequiresRedraw || pendingShadowUpdates.contains(copy.Light.GetValue());
                budgetRequests.push_back(copy);
            }
            threadResources.ShadowScheduler.Schedule(budgetRequests.data(), static_cast<uint32_t>(budgetRequests.size()), mediumBudget,
                                                     scheduledShadows, scheduledShadowPixels);
            UnorderedSet<uint32_t>& scheduledSet = threadResources.ScheduledShadowSet;
            scheduledSet.clear();
            scheduledSet.reserve(scheduledShadows.size());
            for (RenderLightHandle light : scheduledShadows)
                scheduledSet.insert(light.GetValue());

            Vector<ShadowRenderView>& shadowRenderViews = threadResources.ShadowRenderViews;
            uint32_t shadowRenderViewCount = 0;
            auto addShadowRenderView = [&](const glm::mat4& shadowView, const glm::mat4& shadowProjection, ShadowRenderTarget target, uint32_t layer,
                                           const glm::vec4& viewport, uint32_t resolution) {
                if (shadowRenderViewCount >= shadowRenderViews.size())
                    shadowRenderViews.emplace_back();
                ShadowRenderView& output = shadowRenderViews[shadowRenderViewCount++];
                output.Target = target;
                output.ViewProjection = shadowProjection * shadowView;
                output.Viewport = viewport;
                output.Layer = layer;
                RenderView cullingView;
                cullingView.View = shadowView;
                cullingView.Projection = shadowProjection;
                cullingView.ViewportSize = glm::vec2(static_cast<float>(std::max(resolution, 1u)));
                cullingView.CameraCut = true;
                gpuScene.BuildCpuDrawList(cullingView, output.DrawList, &output.DrawBuffers, true);
            };

            ShadowAtlasAllocator& spotShadowAtlas = threadResources.SpotShadowAtlas;
            PointShadowLayerAllocator& pointShadowLayers = threadResources.PointShadowLayers;
            UnorderedSet<uint32_t>& previousSpotLights = threadResources.PreviousSpotLights;
            UnorderedSet<uint32_t>& activeSpotLights = threadResources.ActiveSpotLights;
            UnorderedSet<uint32_t>& activePointLights = threadResources.ActivePointLights;
            activeSpotLights.clear();
            activePointLights.clear();
            activeSpotLights.reserve(snapshot.ShadowUpdateRequests.Size());
            activePointLights.reserve(snapshot.ShadowUpdateRequests.Size());
            for (const ShadowUpdateRequest& request : snapshot.ShadowUpdateRequests)
            {
                if (request.Type == LightType::Spot)
                    activeSpotLights.insert(request.Light.GetValue());
                else if (request.Type == LightType::Point)
                    activePointLights.insert(request.Light.GetValue());
            }
            auto removeInactive = [&](UnorderedSet<uint32_t>& lights) {
                for (auto light = lights.begin(); light != lights.end();)
                {
                    if (activeSpotLights.contains(*light) || activePointLights.contains(*light))
                        ++light;
                    else
                        light = lights.erase(light);
                }
            };
            removeInactive(pendingShadowUpdates);
            removeInactive(renderedShadowLights);
            for (uint32_t previousLight : previousSpotLights)
            {
                if (activeSpotLights.find(previousLight) != activeSpotLights.end())
                    continue;
                spotShadowAtlas.Release(
                  RenderLightHandle::FromParts(previousLight & RenderLightHandle::IndexMask, previousLight >> RenderLightHandle::IndexBits),
                  snapshot.FrameNumber);
            }
            previousSpotLights.swap(activeSpotLights);
            pointShadowLayers.ReleaseMissing(activePointLights, snapshot.FrameNumber);
            const uint64_t completedFrame = snapshot.FrameNumber > 2 ? snapshot.FrameNumber - 2u : 0u;
            spotShadowAtlas.Collect(completedFrame);
            pointShadowLayers.Collect(completedFrame);

            uint32_t shadowLightCount = snapshot.DirectionalShadow.IsValid() ? snapshot.DirectionalShadow.Light.GetIndex() + 1u : 0u;
            for (const ShadowUpdateRequest& request : snapshot.ShadowUpdateRequests)
                shadowLightCount = std::max(shadowLightCount, request.Light.GetIndex() + 1u);
            Vector<GpuShadowLightData>& shadowLights = threadResources.ShadowLights;
            Vector<GpuShadowViewData>& shadowViews = threadResources.ShadowViews;
            shadowLights.clear();
            shadowLights.resize(shadowLightCount);
            shadowViews.clear();
            shadowViews.reserve(4u + snapshot.ShadowUpdateRequests.Size() * 6u);
            if (snapshot.DirectionalShadow.IsValid())
            {
                const uint32_t viewOffset = static_cast<uint32_t>(shadowViews.size());
                ShadowGpuDataBuilder::BuildDirectionalArray(snapshot.DirectionalShadow.Cascades.data(), snapshot.DirectionalShadow.CascadeCount,
                                                            snapshot.DirectionalShadow.Settings, shadowViews);
                shadowLights[snapshot.DirectionalShadow.Light.GetIndex()] = ShadowGpuDataBuilder::BuildLightRecord(
                  viewOffset, snapshot.DirectionalShadow.CascadeCount, LightType::Directional, snapshot.DirectionalShadow.Settings);
                if (snapshot.DirectionalShadow.RequiresRedraw)
                {
                    for (uint32_t cascade = 0; cascade < snapshot.DirectionalShadow.CascadeCount; cascade++)
                    {
                        const DirectionalShadowCascade& shadow = snapshot.DirectionalShadow.Cascades[cascade];
                        addShadowRenderView(shadow.View, shadow.Projection, ShadowRenderTarget::DirectionalArray, cascade,
                                            glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), snapshot.DirectionalShadow.CascadeSettings.Resolution);
                    }
                }
            }

            for (const ShadowUpdateRequest& request : snapshot.ShadowUpdateRequests)
            {
                RenderLightData light;
                if (!gpuScene.TryGetLight(request.Light, light))
                    continue;
                const uint32_t viewOffset = static_cast<uint32_t>(shadowViews.size());
                if (request.Type == LightType::Spot)
                {
                    const ShadowAtlasAllocation allocation = spotShadowAtlas.Acquire(request.Light, request.Resolution);
                    if (!allocation.IsValid())
                        continue;
                    const LocalShadowView view = LocalShadowViewBuilder::BuildSpot(light, request.Settings);
                    shadowViews.push_back(ShadowGpuDataBuilder::BuildSpot(view, allocation, spotShadowAtlas.GetAtlasSize(), request.Settings));
                    const bool scheduled = scheduledSet.contains(request.Light.GetValue());
                    if (scheduled || renderedShadowLights.contains(request.Light.GetValue()))
                        shadowLights[request.Light.GetIndex()] =
                          ShadowGpuDataBuilder::BuildLightRecord(viewOffset, 1, LightType::Spot, request.Settings);
                    if (scheduled)
                    {
                        const float inverseAtlasSize = 1.0f / static_cast<float>(spotShadowAtlas.GetAtlasSize());
                        addShadowRenderView(view.View, view.Projection, ShadowRenderTarget::Atlas, 0,
                                            { allocation.X * inverseAtlasSize, allocation.Y * inverseAtlasSize, allocation.Size * inverseAtlasSize,
                                              allocation.Size * inverseAtlasSize },
                                            allocation.Size);
                        pendingShadowUpdates.erase(request.Light.GetValue());
                        renderedShadowLights.insert(request.Light.GetValue());
                    }
                }
                else if (request.Type == LightType::Point)
                {
                    const uint32_t layer = pointShadowLayers.Acquire(request.Light);
                    if (layer == PointShadowLayerAllocator::InvalidLayer)
                        continue;
                    std::array<LocalShadowView, 6> faceViews;
                    LocalShadowViewBuilder::BuildPoint(light, request.Settings, faceViews);
                    ShadowGpuDataBuilder::BuildPoint(faceViews, layer, request.Settings, shadowViews);
                    const bool scheduled = scheduledSet.contains(request.Light.GetValue());
                    if (scheduled || renderedShadowLights.contains(request.Light.GetValue()))
                        shadowLights[request.Light.GetIndex()] =
                          ShadowGpuDataBuilder::BuildLightRecord(viewOffset, 6, LightType::Point, request.Settings);
                    if (scheduled)
                    {
                        for (uint32_t face = 0; face < faceViews.size(); face++)
                            addShadowRenderView(faceViews[face].View, faceViews[face].Projection, ShadowRenderTarget::PointArray, layer * 6u + face,
                                                glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), request.Resolution);
                        pendingShadowUpdates.erase(request.Light.GetValue());
                        renderedShadowLights.insert(request.Light.GetValue());
                    }
                }
            }
            gpuScene.UploadShadowData(shadowLights.data(), static_cast<uint32_t>(shadowLights.size()), shadowViews.data(),
                                      static_cast<uint32_t>(shadowViews.size()));
            gpuDrivenExecutor.RenderShadows(context, shadowRenderViews, shadowRenderViewCount);
        };
        if (featureTier == RenderFeatureTier::Compatibility)
            graphDesc.CompatibilityRenderer = [&](RenderGraphContext&) { RenderLegacySnapshot(snapshot); };
        else
        {
            graphDesc.FinalComposition = [&](RenderGraphContext&) { RenderLegacyOverlays(snapshot); };
        }
        pipeline.BuildFrameGraph(renderGraph, view, graphDesc, blackboard);
        gpuDrivenExecutor.BeginFrame(view, blackboard, gpuScene, depthDrawList, gpuDrawBinsEnabled ? &gpuScene.GetGpuDrawBinLayout() : nullptr,
                                     snapshot, snapshot.Environment, pipeline.GetSettings());

        const RenderGraphCompileResult& compiledGraph = renderGraph.Compile();
        const bool resourceFrameBegun = graphResources.BeginFrame(compiledGraph, snapshot.FrameNumber, snapshot.HistoryNamespace, view.CameraCut);
        bool resourcesReady = resourceFrameBegun;
        if (resourcesReady)
        {
            resourcesReady = graphResources.BindExternalRenderTarget(target, snapshot.Target);
            auto bindBuffer = [&](RenderGraphResourceHandle handle, const Ref<GenericGpuBuffer>& buffer) {
                if (resourcesReady && handle.IsValid())
                    resourcesReady = graphResources.BindExternalBuffer(handle, buffer);
            };
            bindBuffer(instanceTable, gpuScene.GetInstanceBuffer());
            bindBuffer(lightTable, gpuScene.GetLightBuffer());
            bindBuffer(meshTable, gpuScene.GetMeshBuffer());
            bindBuffer(meshLodTable, gpuScene.GetMeshLodBuffer());
            bindBuffer(meshletTable, gpuScene.GetMeshletBuffer());
            bindBuffer(materialTable, gpuScene.GetMaterialBuffer());
            bindBuffer(drawBinTable, gpuScene.GetGpuDrawBinBuffer());
            bindBuffer(depthInstanceIds, gpuScene.GetCpuDrawBuffers().GetInstanceIDBuffer());
            bindBuffer(depthCommands, gpuScene.GetCpuDrawBuffers().GetCommandBuffer());
        }
        const bool executed = resourcesReady && renderGraph.Execute(nullptr, &graphResources);
        if (resourceFrameBegun)
        {
            if (executed)
                graphResources.EndFrame();
            else
                graphResources.AbortFrame();
        }
        if (!executed)
        {
            if (!compiledGraph.Succeeded)
                CW_ENGINE_ERROR("Render graph compilation failed: {}", compiledGraph.Error);
            else if (graphResources.HasAllocationFailure())
                CW_ENGINE_ERROR("{}", graphResources.GetAllocationError());
            else
                CW_ENGINE_ERROR("Render graph execution failed");
            RenderLegacySnapshot(snapshot);
        }
        else
        {
            SceneRenderStatistics statistics;
            statistics.FrameNumber = snapshot.FrameNumber;
            statistics.LogicalDraws = static_cast<uint32_t>(depthDrawList.Commands.size());
            for (const GpuDrawRun& run : depthDrawList.Runs)
            {
                if (run.FirstCommand >= depthDrawList.Commands.size())
                    continue;
                const DrawMode drawMode = gpuScene.GetGeometryDrawMode(run.Bin.GeometryHeap);
                const uint32_t availableCommands = static_cast<uint32_t>(depthDrawList.Commands.size()) - run.FirstCommand;
                const uint32_t endCommand = run.FirstCommand + std::min(run.CommandCount, availableCommands);
                for (uint32_t commandIndex = run.FirstCommand; commandIndex < endCommand; commandIndex++)
                {
                    const DrawIndexedIndirectCommand& command = depthDrawList.Commands[commandIndex];
                    statistics.VisibleVertices += static_cast<uint64_t>(command.IndexCount) * command.InstanceCount;
                    if (drawMode == DrawMode::TRIANGLE_LIST || drawMode == DrawMode::TRIANGLE_STRIP || drawMode == DrawMode::TRIANGLE_FAN)
                        statistics.VisibleTriangles += RenderAPI::GetPrimitiveCount(drawMode, command.IndexCount) * command.InstanceCount;
                }
            }
            Add2DStatistics(snapshot, statistics);

            const GpuSceneUploadStats& gpuStatistics = gpuScene.GetStats();
            statistics.UploadedBytes = gpuStatistics.UploadedBytes;
            statistics.VisibleInstances = gpuStatistics.VisibleInstances;
            statistics.ActiveInstances = gpuStatistics.ActiveInstances;
            statistics.ActiveLights = gpuStatistics.ActiveLights;

            const RenderGraphExecutionStats& graphStatistics = renderGraph.GetExecutionStats();
            statistics.RenderPasses = graphStatistics.ExecutedPasses;
            statistics.GraphicsPasses = graphStatistics.GraphicsPasses;
            statistics.ComputePasses = graphStatistics.ComputePasses;
            statistics.TransferPasses = graphStatistics.TransferPasses;
            statistics.Barriers = graphStatistics.ScheduledBarriers;
            statistics.RenderGraphCpuTimeMs = graphStatistics.CpuTimeMs;
            statistics.RenderGraphSucceeded = graphStatistics.Succeeded;
            PublishStatistics(statistics);
        }
    }

    void SceneRenderer::SetRenderTarget(const Ref<RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }

    void SceneRenderer::AdvanceCameraHistoryEpoch(uint64_t frameNumber) const
    {
        if (frameNumber == 0)
        {
            m_LastCameraHistoryFrameNumber = 0;
            m_CameraHistoryEpoch++;
        }
        else if (m_LastCameraHistoryFrameNumber == 0)
        {
            m_LastCameraHistoryFrameNumber = frameNumber;
            m_CameraHistoryEpoch++;
        }
        else if (frameNumber > m_LastCameraHistoryFrameNumber)
        {
            m_CameraHistoryEpoch += frameNumber - m_LastCameraHistoryFrameNumber;
            m_LastCameraHistoryFrameNumber = frameNumber;
        }
        else if (frameNumber < m_LastCameraHistoryFrameNumber)
        {
            // A restarted frame timeline begins a new logical epoch without
            // making every retained view appear billions of frames old.
            m_LastCameraHistoryFrameNumber = frameNumber;
            m_CameraHistoryEpoch++;
        }
        if (m_CameraHistoryEpoch == 0)
            m_CameraHistoryEpoch = 1;

        constexpr uint64_t historyRetentionEpochs = 120;
        for (auto stale = m_CameraHistory.begin(); stale != m_CameraHistory.end();)
        {
            if (m_CameraHistoryEpoch - stale->second.LastSeenEpoch <= historyRetentionEpochs)
            {
                ++stale;
                continue;
            }
            m_PendingHistoryReleases.push_back(stale->first);
            stale = m_CameraHistory.erase(stale);
        }
    }

    void SceneRenderer::TransferHistoryReleases(RenderSnapshot& snapshot) const
    {
        snapshot.ReleasedHistoryNamespaces.Reserve(m_PendingHistoryReleases.size());
        for (uint64_t historyNamespace : m_PendingHistoryReleases)
            snapshot.ReleasedHistoryNamespaces.Acquire() = historyNamespace;
        m_PendingHistoryReleases.clear();
    }

    void SceneRenderer::DispatchHistoryReleases()
    {
        RenderHistoryReleaseSink* releaseSink = m_HistoryReleaseSink;
        if (releaseSink == nullptr)
        {
            Application* application = Application::TryGet();
            RenderThread* renderThread = application != nullptr ? application->GetRenderThread() : nullptr;
            if (renderThread != nullptr && renderThread->IsRunning())
                releaseSink = renderThread;
        }

        auto release = [&](uint64_t historyNamespace) {
            if (releaseSink != nullptr)
                releaseSink->QueueHistoryRelease(historyNamespace);
            else
                ReleaseRenderThreadHistory(historyNamespace);
        };
        for (uint64_t historyNamespace : m_PendingHistoryReleases)
            release(historyNamespace);
        for (const auto& [historyNamespace, _] : m_CameraHistory)
            release(historyNamespace);

        m_PendingHistoryReleases.clear();
        m_CameraHistory.clear();
    }

    void SceneRenderer::SetScene(const Ref<Scene>& scene)
    {
        if (m_Scene == scene)
            return;
        ResetTrackedRenderWorld();
        m_PendingHistoryReleases.reserve(m_PendingHistoryReleases.size() + m_CameraHistory.size());
        for (const auto& [historyNamespace, _] : m_CameraHistory)
            m_PendingHistoryReleases.push_back(historyNamespace);
        m_CameraHistory.clear();
        m_CameraHistoryEpoch = 0;
        m_LastCameraHistoryFrameNumber = 0;
        m_Scene = scene;
    }

} // namespace Crowny
