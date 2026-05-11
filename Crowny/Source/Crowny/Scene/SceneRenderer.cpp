#include "cwpch.h"

#include "Crowny/Scene/SceneManager.h"
#include "Crowny/Scene/SceneRenderer.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/AccelerationStructure.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Query.h"
#include "Crowny/RenderAPI/RenderCommand.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/Renderer/ForwardRenderer.h"
#include "Crowny/Renderer/Renderer2D.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
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

    struct SceneRendererStats
    {
        uint32_t Vertices;
        uint32_t Triangles;

        float FrameTime;
        float Frames;
    };

    static SceneRendererData* s_Data;
    static SceneRendererStats s_Stats;

    SceneRenderer::SceneRenderer(const Ref<Scene>& scene, const Ref<RenderTarget>& renderTarget) : m_Scene(scene), m_RenderTarget(renderTarget) {}

    void SceneRenderer::Init()
    {
        s_Data = new SceneRendererData();
        // s_Data->Timer2DGeometry = TimerQuery::Create();
        // s_Data->Timer3DGeometry = TimerQuery::Create();

        // s_Data->PipelineQuery = PipelineQuery::Create();

#if raytracing
        static Ref<Shader> rayTraceShader = Importer::Get().Import<Shader>("Resources/Shaders/RayTrace.glsl");
        static const AssetHandle<Shader> rayTraceHandle = static_asset_cast<Shader>(gAssetManager->CreateAssetHandle(rayTraceShader));
        rayTraceShader->GetTechniques()[0]->GetRenderPasses()[0]->Compile();

        float vertices[] = { 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f };
        uint32_t indices[] = { 0, 1, 2 };
        const glm::mat3x4 transformMatrix(1.0f);
        Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create({sizeof(vertices), BufferUsage::BU_STATIC_DRAW, vertices});
        Ref<IndexBuffer> indexBuffer = IndexBuffer::Create({3, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, indices});

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
        gRenderAPI->SubmitCommandBuffer(cmdBuf, 0);
#endif

        // Ref<Asset> font = Importer::Get().Import("Resources/Fonts/Roboto/roboto-thin.ttf");
        // s_Data.GlobalFont = static_asset_cast<Font>(gAssetManager->CreateAssetHandle(font));

        // Editor grid
        {
            Ref<Shader> gridShader = Importer::Get().Import<Shader>("Resources/Shaders/Grid.glsl");
            gAssetManager->Save(gridShader, GRID_SHADER_PATH);
            const AssetHandle<Shader> gridHandle = static_asset_cast<Shader>(gAssetManager->CreateAssetHandle(gridShader));
            s_Data->GridMaterial = Material::Create(gridHandle);

            const float gridExtent = 500.0f;
            float gridVertices[] = {
                -gridExtent, 0.0f, -gridExtent, gridExtent, 0.0f, -gridExtent, gridExtent, 0.0f, gridExtent, -gridExtent, 0.0f, gridExtent,
            };
            uint32_t gridIndices[] = { 0, 1, 2, 0, 2, 3 };
            s_Data->GridVbo = VertexBuffer::Create({sizeof(gridVertices), BufferUsage::BU_STATIC_DRAW, gridVertices});
            s_Data->GridVbo->SetLayout(CreateRef<BufferLayout>(BufferLayout{ { ShaderDataType::Float3, "inPos" } }));
            s_Data->GridIbo = IndexBuffer::Create({6, IndexType::Index_32, BufferUsage::BU_STATIC_DRAW, gridIndices});
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
        const auto cameraView = m_Scene->m_Registry.view<TransformComponent, CameraComponent>();
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(ee);
            mainCamera = &camera.Camera;
            Entity entity(ee, m_Scene.get());
            cameraTransform = entity.GetWorldMatrix();
            break;
        }

        // Render the scene
        if (mainCamera)
            Render(*mainCamera, glm::inverse(cameraTransform));
    }

    void SceneRenderer::DrawGrid(const glm::mat4& viewProjection, const glm::vec3& cameraPos, const GridSettings& settings)
    {
        RenderAPI& rapi = (*gRenderAPI);
        s_Data->GridMaterial->SetMatrix("viewProjection", viewProjection);
        s_Data->GridMaterial->SetVector3("cameraPos", cameraPos);
        s_Data->GridMaterial->SetFloat("fineSize",   settings.FineSize);
        s_Data->GridMaterial->SetFloat("coarseSize", settings.CoarseSize);
        s_Data->GridMaterial->SetFloat("lineWidth",  settings.LineWidth);
        s_Data->GridMaterial->SetFloat("opacity",    settings.Opacity);
        s_Data->GridMaterial->SetInt("showAxes",     settings.ShowAxes ? 1 : 0);
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
        RenderAPI& rapi = (*gRenderAPI);
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
            auto objs = m_Scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : objs)
            {
                auto [transform, mesh] = m_Scene->m_Registry.get<TransformComponent, MeshRendererComponent>(ee);

                if (mesh.MeshHandle)
                {
                    Entity entity(ee, m_Scene.get());
                    ForwardRenderer::Submit(mesh.MeshHandle, mesh.Materials, entity.GetWorldMatrix());
                    // TODO: Update stats... triangle count has to take into account the draw mode
                }
            }

            // Procedural meshes (direct render path)
            auto procView = m_Scene->m_Registry.view<ProceduralMeshComponent, TransformComponent>();
            for (const entt::entity ee : procView)
            {
                auto [proc, transform] = procView.get<ProceduralMeshComponent, TransformComponent>(ee);
                if (proc.GpuMesh)
                {
                    Entity entity(ee, m_Scene.get());
                    AssetHandle<Mesh> handle = static_asset_cast<Mesh>(gAssetManager->CreateAssetHandle(proc.GpuMesh));
                    ForwardRenderer::Submit(handle, proc.Materials, entity.GetWorldMatrix());
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
            const auto spriteRendererComponents = m_Scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [transform, sprite] = m_Scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Entity entity(ee, m_Scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::FillRect(entity.GetWorldMatrix(), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr, sprite.Color,
                                     ((int32_t)ee) + 1);
                // Renderer2D::FillRect(glm::mat4(1.0f), sprite.Texture, sprite.Color, ((int32_t)ee) + 1);
                s_Stats.Vertices += 6;
                s_Stats.Triangles += 2;
            }
            const auto textComponents = m_Scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            for (const entt::entity ee : textComponents)
            {
                auto [transform, text] = m_Scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Entity entity(ee, m_Scene.get());
                CW_ENGINE_ASSERT(entity.IsValid());
                Renderer2D::DrawString(text, entity.GetWorldMatrix(), (int32_t)ee + 1);
                s_Stats.Vertices += (uint32_t)text.Text.size() * 6;
                s_Stats.Triangles += (uint32_t)text.Text.size() * 2;
            }
            Renderer2D::End();
        }

        s_Stats.Frames += 1;
        // s_Stats.FrameTime = ts;
        FrameMarkEnd("Editor update");
    }

    void SceneRenderer::UpdateProceduralMeshes()
    {
        ZoneScopedN("UpdateProceduralMeshes");
        auto view = m_Scene->m_Registry.view<ProceduralMeshComponent>();
        for (const entt::entity ee : view)
        {
            auto& proc = m_Scene->m_Registry.get<ProceduralMeshComponent>(ee);

            // Read back any completed GPU upload from a previous frame's render thread lambda.
            // PendingGpuResult is written by the render thread and read here on the sim thread
            // only after WaitForFrameDone() has returned, so no synchronisation is needed.
            if (proc.PendingGpuResult)
            {
                if (*proc.PendingGpuResult)
                {
                    proc.GpuMesh = *proc.PendingGpuResult;
                    proc.NeedsGpuUpload = false;
                }
                proc.PendingGpuResult = nullptr;
            }

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

            // Enqueue GPU resource creation/upload on the render thread.
            // Capture GpuMesh by value (it's a shared_ptr, so the Mesh object stays alive).
            // Write the result into a shared slot instead of a raw pointer into the component.
            Ref<Mesh> existingMesh = proc.GpuMesh;
            auto resultSlot = std::make_shared<Ref<Mesh>>();
            proc.PendingGpuResult = resultSlot;

            RenderThread* rt = gApplication->GetRenderThread();
            if (rt && rt->IsRunning())
            {
                rt->EnqueueResourceCommand([existingMesh, resultSlot, result]() mutable {
                    if (existingMesh)
                    {
                        // Reuse existing GPU mesh — update data and re-upload
                        existingMesh->SetMeshData(result);
                        existingMesh->UploadToGpu();
                        *resultSlot = existingMesh;
                    }
                    else
                    {
                        // First time: create GPU mesh with Dynamic usage for efficient updates
                        *resultSlot = Mesh::Create({result, MeshUsage::Dynamic | MeshUsage::CpuCached});
                    }
                });
            }
            else
            {
                // Single-threaded fallback: create/upload directly and apply immediately
                proc.PendingGpuResult = nullptr;
                if (proc.GpuMesh)
                {
                    proc.GpuMesh->SetMeshData(result);
                    proc.GpuMesh->UploadToGpu();
                }
                else
                {
                    proc.GpuMesh = Mesh::Create({result, MeshUsage::Dynamic | MeshUsage::CpuCached});
                }
                proc.NeedsGpuUpload = false;
            }
        }
    }

    RenderSnapshot SceneRenderer::ExtractSnapshot(bool drawGrid) const
    {
        Camera* mainCamera = nullptr;
        glm::mat4 cameraTransform;
        const auto cameraView = m_Scene->m_Registry.view<TransformComponent, CameraComponent>();
        // TODO: Requires improvement for multiple cameras, maybe a main camera checkbox?
        for (const entt::entity ee : cameraView)
        {
            auto [transform, camera] = cameraView.get<TransformComponent, CameraComponent>(ee);
            mainCamera = &camera.Camera;
            Entity entity(ee, m_Scene.get());
            cameraTransform = entity.GetWorldMatrix();
            break;
        }

        if (mainCamera)
            return ExtractSnapshot(*mainCamera, glm::inverse(cameraTransform), drawGrid);

        // No camera in scene — return a snapshot that carries the render target so
        // RenderFromSnapshot can still clear/present the frame without crashing.
        RenderSnapshot empty;
        empty.Target = m_RenderTarget;
        empty.DrawGrid = drawGrid;
        return empty;
    }

    RenderSnapshot SceneRenderer::ExtractSnapshot(const Camera& camera, const glm::mat4& viewTransform, bool drawGrid) const
    {
        ZoneScopedN("ExtractSnapshot");
        RenderSnapshot snapshot;
        snapshot.CameraPosition = camera.GetPosition();
        snapshot.ProjectionMatrix = camera.GetProjection();
        snapshot.ViewMatrix = viewTransform;
        snapshot.Target = m_RenderTarget;
        snapshot.Environment = m_Scene->GetEnvironment();
        snapshot.DrawGrid = drawGrid;

        // 3D mesh objects
        {
            auto objs = m_Scene->m_Registry.group<MeshRendererComponent>(entt::get<TransformComponent>);
            snapshot.MeshObjects.reserve(objs.size());
            for (const entt::entity ee : objs)
            {
                auto [transform, mesh] = m_Scene->m_Registry.get<TransformComponent, MeshRendererComponent>(ee);
                if (mesh.MeshHandle)
                {
                    Entity entity(ee, m_Scene.get());
                    snapshot.MeshObjects.push_back({ entity.GetWorldMatrix(), mesh.MeshHandle, mesh.Materials });
                }
            }
        }

        // Procedural mesh objects
        {
            auto procView = m_Scene->m_Registry.view<ProceduralMeshComponent, TransformComponent>();
            for (const entt::entity ee : procView)
            {
                auto [proc, transform] = procView.get<ProceduralMeshComponent, TransformComponent>(ee);
                if (proc.GpuMesh)
                {
                    Entity entity(ee, m_Scene.get());
                    // Wrap the Ref<Mesh> in an AssetHandle for the snapshot
                    AssetHandle<Mesh> handle = static_asset_cast<Mesh>(gAssetManager->CreateAssetHandle(proc.GpuMesh));
                    snapshot.MeshObjects.push_back({ entity.GetWorldMatrix(), handle, proc.Materials });
                }
            }
        }

        // 2D sprites
        {
            const auto spriteRendererComponents = m_Scene->m_Registry.group<SpriteRendererComponent>(entt::get<TransformComponent>);
            snapshot.Sprites.reserve(spriteRendererComponents.size());
            for (const entt::entity ee : spriteRendererComponents)
            {
                auto [transform, sprite] = m_Scene->m_Registry.get<TransformComponent, SpriteRendererComponent>(ee);
                Entity entity(ee, m_Scene.get());
                snapshot.Sprites.push_back(
                  { entity.GetWorldMatrix(), sprite.Texture ? sprite.Texture.GetInternalPtr() : nullptr, sprite.Color, ((int32_t)ee) + 1 });
            }
        }

        // Text
        {
            const auto textComponents = m_Scene->m_Registry.group<TextComponent>(entt::get<TransformComponent>);
            snapshot.Texts.reserve(textComponents.size());
            for (const entt::entity ee : textComponents)
            {
                auto [transform, text] = m_Scene->m_Registry.get<TransformComponent, TextComponent>(ee);
                Entity entity(ee, m_Scene.get());
                snapshot.Texts.push_back({ text, entity.GetWorldMatrix(), (int32_t)ee + 1 });
            }
        }

        return snapshot;
    }

    void SceneRenderer::RenderFromSnapshot(const RenderSnapshot& snapshot)
    {
        ZoneScopedN("RenderFromSnapshot");
        if (!snapshot.Target)
            return; // No render target — nothing to draw into this frame.

        RenderAPI& rapi = (*gRenderAPI);
        rapi.SetRenderTarget(snapshot.Target);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);

        // Forward pass (3D)
        {
            ForwardRenderer::SetPolygonMode(snapshot.OverridePolygonMode);
            ForwardRenderer::Begin();
            ForwardRenderer::BeginScene(snapshot.ProjectionMatrix, snapshot.ViewMatrix, snapshot.CameraPosition, snapshot.Environment);
            for (const auto& obj : snapshot.MeshObjects)
                ForwardRenderer::Submit(obj.MeshHandle, obj.Materials, obj.WorldMatrix);
            ForwardRenderer::Flush();
            ForwardRenderer::EndScene();
            ForwardRenderer::End();
            ForwardRenderer::SetPolygonMode(PolygonMode::Solid); // Reset after frame
        }

        if (snapshot.DrawGrid)
        {
            ZoneScopedN("Editor grid");
            DrawGrid(snapshot.ProjectionMatrix * snapshot.ViewMatrix, snapshot.CameraPosition, snapshot.Grid);
        }

        // 2D pass
        {
            Renderer2D::Begin(snapshot.ProjectionMatrix, snapshot.ViewMatrix);
            for (const auto& sprite : snapshot.Sprites)
            {
                Renderer2D::FillRect(sprite.WorldMatrix, sprite.Texture, sprite.Color, sprite.EntityId);
                s_Stats.Vertices += 6;
                s_Stats.Triangles += 2;
            }
            for (const auto& text : snapshot.Texts)
            {
                Renderer2D::DrawString(text.TextData, text.WorldMatrix, text.EntityId);
                s_Stats.Vertices += (uint32_t)text.TextData.Text.size() * 6;
                s_Stats.Triangles += (uint32_t)text.TextData.Text.size() * 2;
            }
            Renderer2D::End();
        }

        s_Stats.Frames += 1;
    }

    void SceneRenderer::SetRenderTarget(const Ref<RenderTarget>& renderTarget) { m_RenderTarget = renderTarget; }

    void SceneRenderer::SetScene(const Ref<Scene>& scene) { m_Scene = scene; }

} // namespace Crowny
