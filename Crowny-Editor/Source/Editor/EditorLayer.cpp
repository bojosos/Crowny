#include "cwepch.h"

#include "Editor/EditorLayer.h"

#include "Crowny/Application/EngineRuntime.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/ImGui/ImGuiMenu.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/ManagedReload.h"
#include "Crowny/Scripting/Managed/ManagedScripting.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Editor/PrefabUtils.h"
#include "Editor/ViewportPicking.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/AudioMixerPanel.h"
#include "Panels/ComponentEditor.h"
#include "Panels/ConsolePanel.h"
#include "Panels/EditorPanelRegistry.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#ifdef CW_WITH_NODES
#include "Panels/NodeEditor/NodeEditorPanel.h"
#endif

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"

#include "Editor/ColliderOverlay.h"
#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Renderer/Font.h"

#include "Build/BuildManager.h"
#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"

#ifdef CW_PLATFORM_WIN32
#include "Editor/Script/VisualStudioCodeEditor.h"
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/fmt/fmt.h>

#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{
    EditorCamera EditorLayer::s_EditorCamera = EditorCamera(30.0f, 1280.0f / 720.0f, 0.001f, 30000.0f);

    class RecentScenesMenu : public ImGuiMenu
    {
    public:
        RecentScenesMenu(const String& title, EditorLayer* layer) : ImGuiMenu(title), m_Layer(layer) {}

        virtual void Render() override
        {
            if (ImGui::BeginMenu(m_Title.c_str()))
            {
                const Ref<ProjectSettings> settings = Editor::Get().GetProjectSettings();
                bool renderedScene = false;
                for (const UUID& sceneId : settings->RecentSceneIds)
                {
                    Path sourcePath;
                    if (!ProjectLibrary::Get().TryGetSourcePath(sceneId, AssetType::Scene, sourcePath))
                        continue;
                    renderedScene = true;
                    if (ImGui::MenuItem(sourcePath.filename().string().c_str()))
                        m_Layer->OpenScene(sceneId);
                }
                if (!renderedScene)
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("No recent scenes");
                    ImGui::EndDisabled();
                }
                ImGui::EndMenu();
            }
        }

    private:
        EditorLayer* m_Layer;
    };

    class EditHistoryMenu : public ImGuiMenu
    {
    public:
        explicit EditHistoryMenu(std::function<bool()> editorEnabled) : ImGuiMenu("Edit"), m_EditorEnabled(std::move(editorEnabled)) {}

        void Render() override
        {
            if (!ImGui::BeginMenu(m_Title.c_str()))
                return;

            const bool editing = m_EditorEnabled && m_EditorEnabled();
            const bool canUndo = editing && UndoRedo::IsStartedUp() && UndoRedo::Get().CanUndo();
            const bool canRedo = editing && UndoRedo::IsStartedUp() && UndoRedo::Get().CanRedo();
            const String undoLabel = canUndo ? "Undo " + UndoRedo::Get().GetUndoName() : "Undo";
            const String redoLabel = canRedo ? "Redo " + UndoRedo::Get().GetRedoName() : "Redo";
            if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo))
                UndoRedo::Get().Undo();
            if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Shift+Z / Ctrl+Y", false, canRedo))
                UndoRedo::Get().Redo();
            ImGui::EndMenu();
        }

    private:
        std::function<bool()> m_EditorEnabled;
    };

    EditorLayer::EditorLayer() : Layer("EditorLayer"), m_SceneRenderer(nullptr) {}

    EditorLayer::~EditorLayer() = default;

    void EditorLayer::OnAttach()
    {
        static const String imguiIniFilename = (Application::TryGet()->GetWorkingDirectory() / "imgui.ini").string();
        ImGui::GetIO().IniFilename = imguiIniFilename.c_str();

        RegisterBuiltinNodeTypes();

        EditorAssets::Load();

        Editor::StartUp([this](const Path& path, FileWatch::Change changeType) {
            if (changeType != FileWatch::FileModified && changeType != FileWatch::FileAdded && changeType != FileWatch::FileNewRenamed)
                return;
            {
                Lock lock(m_FileWatchMutex);
                m_FileWatchQueue.push_back(path);
                if (path.extension() == ".cs")
                    m_AssemblyReloadDebouncer.Notify();
            }
        });

        m_MenuBar = CreateScope<ImGuiMenuBar>();
        ImGuiMenu& fileMenu = m_MenuBar->AddMenu("File");
        fileMenu.AddItem("New Project", {}, [this]() { m_NewProject = true; });
        fileMenu.AddItem("Open Project", {}, [this]() { m_OpenProject = true; });
        fileMenu.AddItem("Save project", {}, [this]() {
            Editor::Get().SaveProject();
            AddNotification("Project saved.", NotificationKind::Success);
        });
        fileMenu.AddItem("New Scene", "Ctrl+Shift+N", [this]() { CreateNewScene(); });
        fileMenu.AddItem("Open Scene", "Ctrl+Shift+O", [this]() { OpenScene(); });
        fileMenu.AddMenu(CreateScope<RecentScenesMenu>("Open Recent", this));
        fileMenu.AddItem("Save Scene", "Ctrl+S", [this]() { SaveActiveScene(); });
        fileMenu.AddItem("Save Scene as", "Ctrl+Shift+S", [this]() { SaveActiveSceneAs(); });
        fileMenu.AddItem("Exit", "Alt+F4", []() { Application::TryGet()->Exit(); });

        m_MenuBar->AddMenu(CreateScope<EditHistoryMenu>([this]() { return m_SceneState == SceneState::Edit; }));

        // Has to be done before hierarchy and asset browser panels
        m_Panels = CreateScope<EditorPanelRegistry>();
        m_InspectorPanel = &m_Panels->Add(InspectorPanel::Registration);
        m_HierarchyPanel = &m_Panels->Add(HierarchyPanel::Registration, [this](Entity primary, const Vector<Entity>& entities) {
            m_InspectorPanel->SetSelectedEntities(primary, entities);
        });
        m_ViewportPanel = &m_Panels->Add(
          ViewportPanel::Registration, [this]() { return m_HierarchyPanel->GetSelectedEntity(); },
          [this]() -> const Vector<Entity>& { return m_HierarchyPanel->GetSelectedEntities(); });
        m_ViewportPanel->SetEventCallback(CW_BIND_EVENT_FN(OnViewportEvent));
        m_ConsolePanel = &m_Panels->Add(ConsolePanel::Registration);
        m_AssetBrowser = &m_Panels->Add(AssetBrowserPanel::Registration, [this](const Path& path) { m_InspectorPanel->SetSelectedAssetPath(path); });
        m_AudioMixerPanel = &m_Panels->Add(AudioMixerPanel::Registration);
#ifdef CW_WITH_NODES
        m_NodeEditorPanel = &m_Panels->Add(NodeEditorPanel::Registration);

        m_InspectorPanel->SetOpenNodeEditorCallback([this](AssetHandle<NodeGraphAsset> graphAsset) {
            m_NodeEditorPanel->SetGraph(graphAsset);
            m_NodeEditorPanel->Show();
        });
#endif

        ImGuiMenu& buildMenu = m_MenuBar->AddMenu("Build");
        buildMenu.AddItem("Rebuild game assembly", "Ctrl+Shift+B", [this]() {
            const bool built = RebuildAssemblies();
            AddNotification(built ? "Game scripts rebuilt." : "Game script build failed.",
                            built ? NotificationKind::Success : NotificationKind::Error);
        });
        buildMenu.AddItem("Build game", "Ctrl+B", [this]() { BuildGame(); });

        ImGuiMenu& viewMenu = m_MenuBar->AddMenu(m_Panels->CreateMenu("View"));
        viewMenu.AddItem("Viewport settings", {}, [this]() { m_ShowViewportSettings = true; });

        ImGuiMenu& workspaceMenu = m_MenuBar->AddMenu("Workspace");
        workspaceMenu.AddItem("Command palette", "Ctrl+P", [this]() {
            m_ShowCommandPalette = true;
            m_FocusCommandPalette = true;
        });
        workspaceMenu.AddItem("Settings", "Ctrl+,", [this]() { m_ShowSettings = true; });
        workspaceMenu.AddItem("Reset layout", {}, [this]() { ResetWorkspaceLayout(); });

        UndoRedo::StartUp();
        UndoRedo::Get().SetActionAppliedCallback([this](const Ref<UndoAction>& action) {
            if (!action || !m_HierarchyPanel)
                return;
            Entity focus = action->GetFocusEntity();
            const Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
            if (focus && activeScene && focus.GetScene() == activeScene.get())
                m_HierarchyPanel->SetSelectedEntity(focus);
        });

        ApplyEditorSettings();

        if (m_Temp == nullptr) // No scene was auto-loaded
            m_Temp = CreateRef<Scene>("Scene");

        m_SceneRenderer = new SceneRenderer(nullptr, nullptr);
        m_SceneLifecycleListener = SceneManager::TryGet()->AddLifecycleListener([this](const SceneLifecycleEvent& event) {
            if (event.Type == SceneLifecycleEventType::ActiveChanged || event.Type == SceneLifecycleEventType::ExecutionStateChanged)
            {
                const Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
                UndoRedo::Get().SetSceneContext(activeScene, event.State == SceneExecutionState::Edit);
            }

            if (event.Type == SceneLifecycleEventType::ActiveChanged)
            {
                const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
                m_SceneRenderer->SetScene(scene);

                const UUID wanted = SceneManager::TryGet()->GetEditSelection();
                Entity selected = scene && !wanted.Empty() ? scene->TryGetEntityFromUuid(wanted) : Entity{};
                m_HierarchyPanel->SetSelectedEntity(selected ? selected : (scene ? scene->GetRootEntity() : Entity{}));
                return;
            }

            if (event.Type != SceneLifecycleEventType::ExecutionStateChanged)
                return;

            switch (event.State)
            {
            case SceneExecutionState::Edit:
                m_SceneState = SceneState::Edit;
                m_ViewportPanel->EnableGizmo();
                m_GameMode = false;
                Application::TryGet()->GetTime().ResetSimulation();
                break;
            case SceneExecutionState::Play:
                m_SceneState = SceneState::Play;
                m_ViewportPanel->DisableGizmo();
                break;
            case SceneExecutionState::PlayPaused:
                m_SceneState = SceneState::PausePlay;
                m_ViewportPanel->EnableGizmo();
                break;
            case SceneExecutionState::Simulate:
                m_SceneState = SceneState::Simulate;
                m_ViewportPanel->DisableGizmo();
                break;
            }
        });
        UndoRedo::Get().SetSceneContext(SceneManager::TryGet()->GetActiveScene(),
                                        SceneManager::TryGet()->GetExecutionState() == SceneExecutionState::Edit);
    }

    void EditorLayer::CreateRenderTarget()
    {
        TextureDesc colorParams;
        colorParams.Width = 1337;
        colorParams.Height = 509;
        colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;
        colorParams.DebugName = "EditorLayer/ViewportColor";

        TextureDesc objectId;
        objectId.Width = 1337;
        objectId.Height = 509;
        objectId.Format = TextureFormat::R32I;
        objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);
        objectId.DebugName = "EditorLayer/ViewportObjectId";

        TextureDesc depthParams;
        depthParams.Width = 1337;
        depthParams.Height = 509;
        depthParams.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
        depthParams.Format = TextureFormat::DEPTH24STENCIL8;
        depthParams.DebugName = "EditorLayer/ViewportDepth";

        Ref<Texture> color1 = Texture::Create(colorParams);
        Ref<Texture> color2 = Texture::Create(objectId);
        Ref<Texture> depth = Texture::Create(depthParams);
        RenderTextureDesc rtProps;
        rtProps.ColorSurfaces[0].Texture = color1;
        rtProps.ColorSurfaces[1].Texture = color2;
        rtProps.DepthSurface.Texture = depth;
        rtProps.Width = 1337;
        rtProps.Height = 509;

        m_RenderTarget = RenderTexture::Create(rtProps);
    }

    bool EditorLayer::OnViewportEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<ImGuiViewportSceneDraggedEvent>([this](ImGuiViewportSceneDraggedEvent& fileDragEvent) {
            const FileEntry* fileEntry = fileDragEvent.GetFileEntry();
            if (fileEntry->Metadata == nullptr)
                return true;
            const AssetType assetType = fileEntry->Metadata->Type;
            if (assetType == AssetType::Scene)
                OpenScene(fileEntry->Metadata->Uuid);
            else if (assetType == AssetType::Material)
            {
                Entity entity = PickEntity(fileDragEvent.GetScreenPosition());
                if (entity && entity.HasComponent<MeshRendererComponent>())
                {
                    const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                    entity.GetComponent<MeshRendererComponent>().SetMaterial(0, static_asset_cast<Material>(assetHandle));
                }
            }
            else if (assetType == AssetType::Mesh)
            {
                Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
                Entity entity = activeScene->CreateEntity(fileEntry->Filepath.filename().string());
                MeshRendererComponent& meshRenderer = entity.AddComponent<MeshRendererComponent>();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                meshRenderer.MeshHandle = static_asset_cast<Mesh>(assetHandle);
            }
            else if (assetType == AssetType::AudioClip)
            {
                Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
                Entity entity = activeScene->CreateEntity(fileEntry->Filepath.filename().string());
                AudioSourceComponent& audioSourceComponent = entity.AddComponent<AudioSourceComponent>();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                audioSourceComponent.SetClip(static_asset_cast<AudioClip>(assetHandle));
            }
            else if (assetType == AssetType::Prefab)
            {
                Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                AssetHandle<Prefab> prefab = static_asset_cast<Prefab>(assetHandle);
                Entity root = activeScene->GetRootEntity();
                PrefabUtils::InstantiatePrefab(prefab, root);
            }
            return true;
        });
        return true;
    }

    void EditorLayer::OnDetach()
    {
        if (SceneManager::TryGet() != nullptr && SceneManager::TryGet()->GetExecutionState() != SceneExecutionState::Edit)
            SceneManager::TryGet()->Stop();

        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        settings->ShowImGuiDemoWindow = m_ShowDemoWindow;
        settings->ShowPhysicsColliders = m_ShowColliders;
        settings->AutoLoadLastProject = m_AutoLoadLastProject;
        settings->ShowEntityDebugInfo = m_ShowEntityDebugInfo;
        settings->ShowAssetInfo = m_ShowAssetInfo;
        settings->ShowScriptDebugInfo = m_ShowScriptDebugInfo;

        settings->WireframeMode = m_WireframeMode;
        settings->ShowRenderingStatistics = m_ShowRenderingStatistics;
        settings->ShowGrid = m_ShowGrid;
        settings->ShowGridAxes = m_ShowGridAxes;
        settings->GridFineSize = m_GridFineSize;
        settings->GridCoarseSize = m_GridCoarseSize;
        settings->GridLineWidth = m_GridLineWidth;
        settings->GridOpacity = m_GridOpacity;
        settings->ColliderColor = m_ColliderColor;
        settings->EnableConsoleInfoMessages = m_ConsolePanel->IsMessageLevelEnabled(ConsoleBuffer::Message::Level::Info);
        settings->EnableConsoleWarningMessages = m_ConsolePanel->IsMessageLevelEnabled(ConsoleBuffer::Message::Level::Warn);
        settings->EnableConsoleErrorMessages = m_ConsolePanel->IsMessageLevelEnabled(ConsoleBuffer::Message::Level::Error);

        settings->CollapseConsole = m_ConsolePanel->IsCollapseEnabled();
        settings->ScrollToBottom = m_ConsolePanel->IsScrollToBottomEnabled();
        if (CodeEditorManager::IsStartedUp())
            settings->CodeEditorPath = CodeEditorManager::Get().GetActiveEditorPath();

        EditorAssets::Unload();
        if (Editor::Get().IsProjectLoaded())
        {
            SaveProjectSettings();
            const Ref<Scene>& activeScene = SceneManager::TryGet()->GetActiveScene();
            if (activeScene && !activeScene->GetFilepath().empty())
                SaveActiveScene();
            Editor::Get().SaveProject();
        }
        if (m_SceneLifecycleListener != 0 && SceneManager::TryGet() != nullptr)
        {
            SceneManager::TryGet()->RemoveLifecycleListener(m_SceneLifecycleListener);
            m_SceneLifecycleListener = 0;
        }
        if (m_AssetBrowser != nullptr)
            m_AssetBrowser->Unload();
        BuildManager::Shutdown();
        CodeEditorManager::Shutdown();
        Editor::Shutdown();

        delete m_SceneRenderer;
        m_SceneRenderer = nullptr;
        m_MenuBar.reset();
        m_Panels.reset();
        m_InspectorPanel = nullptr;
        m_HierarchyPanel = nullptr;
        m_ViewportPanel = nullptr;
        m_ConsolePanel = nullptr;
        m_AssetBrowser = nullptr;
        m_AudioMixerPanel = nullptr;
#ifdef CW_WITH_NODES
        m_NodeEditorPanel = nullptr;
#endif
        if (UndoRedo::IsStartedUp())
            UndoRedo::Shutdown();
    }

    Entity EditorLayer::PickEntity()
    {
        const ImVec2 mousePosition = ImGui::GetMousePos();
        return PickEntity(glm::vec2(mousePosition.x, mousePosition.y));
    }

    Entity EditorLayer::PickEntity(const glm::vec2& screenPosition)
    {
        SceneManager* sceneManager = SceneManager::TryGet();
        if (sceneManager == nullptr || m_ViewportPanel == nullptr || !m_RenderTarget)
            return {};

        const Ref<Scene> scene = sceneManager->GetActiveScene();
        if (!scene)
            return {};

        const Ref<Texture> objectIdTexture = m_RenderTarget->GetColorTexture(1);
        if (!objectIdTexture || objectIdTexture->GetFormat() != TextureFormat::R32I)
            return {};

        const ViewportTextureExtent textureExtent{ objectIdTexture->GetWidth(), objectIdTexture->GetHeight() };
        const std::optional<ViewportPickPixel> pixel =
          ResolveViewportPickPixel(screenPosition, m_ViewportPanel->GetViewportBounds(), textureExtent);
        if (!pixel)
            return {};

        int32_t objectId = 0;
        if (!objectIdTexture->ReadPixel(pixel->X, pixel->Y, &objectId, sizeof(objectId)) || objectId <= 0)
            return {};

        const entt::entity handle = static_cast<entt::entity>(static_cast<uint32_t>(objectId - 1));
        const Entity entity(handle, scene.get());
        return entity ? entity : Entity{};
    }

    void EditorLayer::HandleRenderTargetResize()
    {
        RenderAPI* renderAPI = RenderAPI::TryGet();
        if (renderAPI == nullptr || m_ViewportPanel == nullptr || m_SceneRenderer == nullptr)
            return;

        const glm::vec2 displaySize = m_ViewportPanel->GetViewportSize();
        const std::optional<ViewportTextureExtent> viewportExtent = ResolveViewportTextureExtent(displaySize);
        if (!viewportExtent)
            return;

        const bool resizeRequired = !m_RenderTarget || m_RenderTarget->GetProperties().Width != viewportExtent->Width ||
                                    m_RenderTarget->GetProperties().Height != viewportExtent->Height;
        if (m_ViewportPanel->IsShown() && resizeRequired)
        {
            TextureDesc colorParams;
            colorParams.Width = viewportExtent->Width;
            colorParams.Height = viewportExtent->Height;
            colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;
            colorParams.DebugName = "EditorLayer/ViewportColor";

            TextureDesc objectId;
            objectId.Width = viewportExtent->Width;
            objectId.Height = viewportExtent->Height;
            objectId.Format = TextureFormat::R32I;
            objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);
            objectId.DebugName = "EditorLayer/ViewportObjectId";

            TextureDesc depthParams;
            depthParams.Width = viewportExtent->Width;
            depthParams.Height = viewportExtent->Height;
            depthParams.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            depthParams.Format = TextureFormat::DEPTH24STENCIL8;
            depthParams.DebugName = "EditorLayer/ViewportDepth";

            Ref<Texture> color1 = Texture::Create(colorParams);
            Ref<Texture> color2 = Texture::Create(objectId);
            Ref<Texture> depth = Texture::Create(depthParams);
            RenderTextureDesc rtProps;
            rtProps.ColorSurfaces[0].Texture = color1;
            rtProps.ColorSurfaces[1].Texture = color2;
            rtProps.DepthSurface.Texture = depth;
            rtProps.Width = viewportExtent->Width;
            rtProps.Height = viewportExtent->Height;
            const Ref<RenderTexture> resizedRenderTarget = color1 && color2 && depth ? RenderTexture::Create(rtProps) : nullptr;
            if (resizedRenderTarget)
            {
                m_RenderTarget = resizedRenderTarget;
                if (SceneManager* sceneManager = SceneManager::TryGet())
                {
                    if (const Ref<Scene> scene = sceneManager->GetActiveScene())
                        scene->OnViewportResize(viewportExtent->Width, viewportExtent->Height);
                }
            }
        }

        m_ViewportSize = displaySize;
        if (!m_RenderTarget)
            return;
        m_SceneRenderer->SetRenderTarget(m_RenderTarget);

        renderAPI->SetRenderTarget(m_RenderTarget);
        renderAPI->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        renderAPI->ClearRenderTarget(FBT_COLOR | FBT_DEPTH);
    }

    RenderSnapshot& EditorLayer::AcquireSnapshot()
    {
        RenderThread* renderThread = Application::TryGet()->GetRenderThread();
        if (renderThread && renderThread->IsRunning())
            return renderThread->BeginFrame();

        m_FallbackSnapshot.Clear();
        return m_FallbackSnapshot;
    }

    void EditorLayer::SubmitSnapshot(RenderSnapshot& snapshot)
    {
        // The editor viewport needs IDs for asynchronous picking. Runtime and
        // preview views keep the optional R32I target disabled by default.
        snapshot.EnableObjectID = true;
        RenderThread* renderThread = Application::TryGet()->GetRenderThread();
        if (renderThread && renderThread->IsRunning())
            renderThread->SubmitFrame();
        else
            SceneRenderer::RenderFromSnapshot(snapshot);
    }

    void EditorLayer::HandleSceneState(Timestep ts)
    {
        switch (m_SceneState)
        {
        case SceneState::Edit: {
            s_EditorCamera.SetViewportSize((float)m_RenderTarget->GetProperties().Width, (float)m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);

            SceneManager::TryGet()->GetActiveScene()->OnUpdateEditor(ts);
            m_SceneRenderer->UpdateProceduralMeshes();
            RenderSnapshot& snapshot = AcquireSnapshot();
            m_SceneRenderer->ExtractSnapshot(snapshot, s_EditorCamera, s_EditorCamera.GetViewMatrix(), m_ShowGrid);
            snapshot.OverridePolygonMode = m_WireframeMode ? PolygonMode::Wireframe : PolygonMode::Solid;
            snapshot.Grid = { m_GridFineSize, m_GridCoarseSize, m_GridLineWidth, m_GridOpacity, m_ShowGridAxes };
            SubmitSnapshot(snapshot);
            break;
        }
        case SceneState::Play: {
            Application* application = Application::TryGet();
            Time& time = application->GetTime();
            const SimulationFrame frame = time.AdvanceSimulation(*application->GetTimeSettings());
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            scene->SynchronizePhysicsTransforms(1.0f, Timestep(0.0f));
            time.ExecuteSimulationFrame(
              frame,
              [&](Timestep fixedDelta) {
                  ScriptRuntime::OnFixedUpdate(scene, fixedDelta);
                  scene->OnFixedUpdate(fixedDelta);
              },
              [&](Timestep frameDelta) {
                  scene->OnUpdateRuntime(frameDelta);
                  ScriptRuntime::OnUpdate(scene, frameDelta);
                  m_SceneRenderer->UpdateAnimations(frameDelta);
              },
              [&](float interpolationAlpha, Timestep extrapolationTime) {
                  scene->SynchronizePhysicsTransforms(interpolationAlpha, extrapolationTime);
              });
            m_SceneRenderer->UpdateProceduralMeshes();
            RenderSnapshot& snapshot = AcquireSnapshot();
            m_SceneRenderer->ExtractSnapshot(snapshot);
            SubmitSnapshot(snapshot);
            break;
        }
        case SceneState::Simulate: {
            s_EditorCamera.SetViewportSize((float)m_RenderTarget->GetProperties().Width, (float)m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);
            Application* application = Application::TryGet();
            Time& time = application->GetTime();
            const SimulationFrame frame = time.AdvanceSimulation(*application->GetTimeSettings());
            const Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
            scene->SynchronizePhysicsTransforms(1.0f, Timestep(0.0f));
            time.ExecuteSimulationFrame(
              frame, [&](Timestep fixedDelta) { scene->OnSimulationFixedUpdate(fixedDelta); },
              [&](Timestep frameDelta) { m_SceneRenderer->UpdateAnimations(frameDelta); },
              [&](float interpolationAlpha, Timestep extrapolationTime) {
                  scene->SynchronizePhysicsTransforms(interpolationAlpha, extrapolationTime);
              });
            m_SceneRenderer->UpdateProceduralMeshes();
            RenderSnapshot& snapshot = AcquireSnapshot();
            m_SceneRenderer->ExtractSnapshot(snapshot, s_EditorCamera, s_EditorCamera.GetViewMatrix(), m_ShowGrid);
            snapshot.OverridePolygonMode = m_WireframeMode ? PolygonMode::Wireframe : PolygonMode::Solid;
            snapshot.Grid = { m_GridFineSize, m_GridCoarseSize, m_GridLineWidth, m_GridOpacity, m_ShowGridAxes };
            SubmitSnapshot(snapshot);
            break;
        }
        case SceneState::PausePlay: {
            m_SceneRenderer->UpdateProceduralMeshes();
            RenderSnapshot& snapshot = AcquireSnapshot();
            m_SceneRenderer->ExtractSnapshot(snapshot);
            SubmitSnapshot(snapshot);
            break;
        }
        }
    }

    void EditorLayer::OnUpdate(Timestep ts)
    {
        // Let one complete ImGui frame reach the swapchain before renderer
        // defaults, runtime services, project scans, and editor discovery.
        if (m_DeferredStartupPending)
        {
            if (m_StartupFrameCount++ == 0)
                return;
            FinishDeferredStartup();
        }

        ExecuteProjectAssetRefresh();

        // Process completed async imports (GPU init on main thread)
        if (ProjectLibrary::IsStartedUp() && ProjectLibrary::Get().IsImporting())
            ProjectLibrary::Get().ProcessCompletedImports();

        if (m_Temp) // Delay scene reload
        {
            Ref<Scene> activeScene = SceneManager::TryGet()->GetActiveScene();
            if (activeScene)
                activeScene->SetImGuiLayout(Application::TryGet()->GetImGuiLayer()->SaveLayout());
            UndoRedo::Get().Clear();
            const UUID previousSceneId = SceneManager::TryGet()->GetActiveSceneId();
            SceneManager::TryGet()->SetActiveScene(m_Temp, m_TempSceneId);
            if (!previousSceneId.Empty() && previousSceneId != m_TempSceneId)
                SceneManager::TryGet()->UnloadScene(previousSceneId);
            Application::TryGet()->GetImGuiLayer()->LoadLayout(m_Temp->GetImGuiLayout());
            Editor::Get().GetProjectSettings()->LastOpenSceneId = m_TempSceneId;
            m_Temp = nullptr;
            m_TempSceneId = UUID::EMPTY;
            // ScriptRuntime::Init();
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + SceneManager::TryGet()->GetActiveScene()->GetName();
            Application::TryGet()->GetWindow().SetTitle(title);
        }

        bool rebuildAssemblies = false;
        if (m_SceneState == SceneState::Edit)
        {
            Lock lock(m_FileWatchMutex);
            rebuildAssemblies = m_AssemblyReloadDebouncer.TryBegin();
        }
        if (rebuildAssemblies)
        {
            RebuildAssemblies();
            Lock lock(m_FileWatchMutex);
            m_AssemblyReloadDebouncer.Complete();
        }

        Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        HandleRenderTargetResize();
        HandleSceneState(ts);

        // Wait for render thread to finish scene recording before any main-thread
        // rendering (RenderOverlay) or readback (PickEntity) that touches the same
        // command buffer or render target.
        RenderThread* rt = Application::TryGet()->GetRenderThread();
        if (rt && rt->IsRunning())
            rt->WaitForFrameDone();

        RenderOverlay();

        if (m_ViewportPanel->IsHovered())
        {
            if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !Input::IsKeyPressed(Key::LeftAlt) && !Input::IsKeyPressed(Key::RightAlt) &&
                !m_ViewportPanel->IsMouseOverGizmo() && !m_ViewportPanel->IsMouseOverHud())
            {
                const Entity pickedEntity = PickEntity();
                const bool ctrl = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
                const bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
                if (pickedEntity)
                    m_HierarchyPanel->SelectEntity(pickedEntity, ctrl    ? EntitySelectionMode::Toggle
                                                                 : shift ? EntitySelectionMode::Add
                                                                         : EntitySelectionMode::Replace);
                else if (!ctrl && !shift)
                    m_HierarchyPanel->SetSelectedEntity({});
            }
        }

        m_HierarchyPanel->Update();
        if (m_SceneState != SceneState::Play)
        {
            ManagedScripting* managed = Application::TryGet()->GetRuntime().GetManagedScripting();
            if (managed != nullptr)
            {
                for (const ManagedDiagnostic& diagnostic : managed->Update())
                {
                    if (diagnostic.Severity == ManagedDiagnosticSeverity::Error)
                        CW_ENGINE_ERROR("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                    else if (diagnostic.Severity == ManagedDiagnosticSeverity::Warning)
                        CW_ENGINE_WARN("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                    else
                        CW_ENGINE_INFO("Managed scripting [{}]: {}", diagnostic.Code, diagnostic.Message);
                }
            }
        }
    }

    void EditorLayer::RenderOverlay()
    {
        Ref<Scene> scene = SceneManager::TryGet()->GetActiveScene();
        if (m_SceneState == SceneState::Play)
        {
            Entity camera = scene->GetPrimaryCameraEntity();
            if (!camera)
                return;
            Renderer2D::Begin(camera.GetComponent<CameraComponent>().Camera, camera.GetWorldMatrix());
        }
        else
            Renderer2D::Begin(s_EditorCamera, s_EditorCamera.GetViewMatrix());

        if (m_ShowColliders)
            ColliderOverlay::Draw(*scene, m_ColliderColor);

        // Audio cone gizmo for the selected entity. Drawn only for the selection so the viewport
        // isn't flooded when there are many AudioSources in the scene.
        Entity selected = m_HierarchyPanel->GetSelectedEntity();
        if (selected && selected.HasComponent<AudioSourceComponent>())
        {
            const AudioSourceComponent& asc = selected.GetComponent<AudioSourceComponent>();
            // Skip the gizmo for omnidirectional sources — no useful cone to draw.
            if (asc.GetConeOuterAngle() < 360.0f)
            {
                const glm::mat4 world = selected.GetWorldMatrix();
                const glm::vec3 apex = glm::vec3(world[3]);
                const glm::vec3 forward = glm::normalize(-glm::vec3(world[2]));
                const glm::vec3 up = glm::normalize(glm::vec3(world[1]));
                const glm::vec3 right = glm::normalize(glm::cross(forward, up));

                // Apex-to-base distance matches min distance so the gizmo scales with the source's
                // audible near-field. Half-angle drives base radius.
                const float length = std::max(asc.GetMinDistance(), 0.1f);
                const glm::vec3 baseCenter = apex + forward * length;

                auto drawCone = [&](float fullAngleDegrees, const glm::vec4& color) {
                    const float halfAngle = glm::radians(fullAngleDegrees) * 0.5f;
                    const float baseRadius = length * std::tan(halfAngle);
                    constexpr int Segments = 24;
                    glm::vec3 prev;
                    for (int i = 0; i <= Segments; i++)
                    {
                        const float t = (float)i / Segments * glm::two_pi<float>();
                        const glm::vec3 offset = right * (std::cos(t) * baseRadius) + up * (std::sin(t) * baseRadius);
                        const glm::vec3 p = baseCenter + offset;
                        if (i > 0)
                            Renderer2D::DrawLine(prev, p, color);
                        // Spokes from apex to every 3rd circle point — keeps the gizmo readable.
                        if (i % 3 == 0 && i < Segments)
                            Renderer2D::DrawLine(apex, p, color);
                        prev = p;
                    }
                };

                drawCone(asc.GetConeInnerAngle(), { 0.2f, 0.9f, 0.3f, 1.0f });
                drawCone(asc.GetConeOuterAngle(), { 0.9f, 0.5f, 0.1f, 0.7f });
            }
        }

        Renderer2D::End();
    }

    void EditorLayer::SetupImGuiRender()
    {

        static bool dockspaceOpen = true;
        static bool opt_fullscreen_persistant = true;
        const bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        String title = "Crowny Editor";
        if (Editor::Get().IsProjectLoaded())
        {
            title += " - " + Editor::Get().GetProjectName();
            const auto& activeScene = SceneManager::TryGet()->GetActiveScene();
            if (activeScene)
                title += " - " + activeScene->GetName();
        }
        title += "###CrownyEditorDockspaceHost";
        ImGui::Begin(title.c_str(), &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("Crowny Editor");
            const bool layoutMissing = ImGui::DockBuilderGetNode(dockspace_id) == nullptr;
            if (m_ResetLayoutRequested || layoutMissing)
            {
                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

                ImGuiID centerId = dockspace_id;
                ImGuiID leftId = 0;
                ImGuiID rightId = 0;
                ImGuiID bottomId = 0;
                ImGuiID leftBottomId = 0;
                ImGuiID leftTopId = 0;
                ImGuiID rightBottomId = 0;
                ImGuiID rightTopId = 0;
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.10f, &leftId, &centerId);
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.25f, &rightId, &centerId);
                ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.33f, &bottomId, &centerId);
                ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Down, 0.31f, &leftBottomId, &leftTopId);
                ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.31f, &rightBottomId, &rightTopId);

                ImGui::DockBuilderDockWindow("Viewport", centerId);
                ImGui::DockBuilderDockWindow("Hierarchy", leftTopId);
                ImGui::DockBuilderDockWindow("Tree view", leftBottomId);
                ImGui::DockBuilderDockWindow("Inspector", rightTopId);
                ImGui::DockBuilderDockWindow("Physics 2D", rightTopId);
                ImGui::DockBuilderDockWindow("Physics2D Stats", rightTopId);
                ImGui::DockBuilderDockWindow("Settings", rightBottomId);
                ImGui::DockBuilderDockWindow("Time Settings", rightBottomId);
                ImGui::DockBuilderDockWindow("C# debug", rightBottomId);
                ImGui::DockBuilderDockWindow("Asset Browser", bottomId);
                ImGui::DockBuilderDockWindow("Console", bottomId);
                ImGui::DockBuilderDockWindow("Audio Mixer", bottomId);
#ifdef CW_WITH_NODES
                ImGui::DockBuilderDockWindow("Node Editor", bottomId);
#endif
                ImGui::DockBuilderFinish(dockspace_id);
                m_ResetLayoutRequested = false;
            }
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
    }

    bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0)
            return false;

        const bool ctrl = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
        const bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

        if (ImGui::GetIO().WantTextInput && !(ctrl && e.GetKeyCode() == Key::P))
            return false;

        switch (e.GetKeyCode())
        {
        case Key::N: {
            if (ctrl && shift)
            {
                CreateNewScene();
                return true;
            }
            break;
        }

        case Key::O: {
            if (ctrl && shift)
            {
                OpenScene();
                return true;
            }
            break;
        }

        case Key::S: {
            if (ctrl && !shift)
            {
                SaveActiveScene();
                return true;
            }

            if (ctrl && shift)
            {
                SaveActiveSceneAs();
                return true;
            }
            break;
        }
        case Key::Z: {
            if (ctrl && m_SceneState == SceneState::Edit)
            {
                if (shift)
                    UndoRedo::Get().Redo();
                else
                    UndoRedo::Get().Undo();
                return true;
            }
            break;
        }
        case Key::Y: {
            if (ctrl && m_SceneState == SceneState::Edit)
            {
                UndoRedo::Get().Redo();
                return true;
            }
            break;
        }
        case Key::B: {
            if (ctrl && shift)
            {
                const bool built = RebuildAssemblies();
                AddNotification(built ? "Game scripts rebuilt." : "Game script build failed.",
                                built ? NotificationKind::Success : NotificationKind::Error);
                return true;
            }
            if (ctrl)
            {
                BuildGame();
                return true;
            }
            break;
        }
        case Key::P: {
            if (ctrl)
            {
                m_ShowCommandPalette = true;
                m_FocusCommandPalette = true;
                return true;
            }
            break;
        }
        case Key::Comma: {
            if (ctrl)
            {
                m_ShowSettings = true;
                return true;
            }
            break;
        }
        case Key::F5: {
            if (!ctrl && !shift && m_SceneState != SceneState::Simulate)
            {
                TogglePlay();
                return true;
            }
            break;
        }
        }
        return false;
    }

    bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e) { return false; }

    void EditorLayer::OnEvent(Event& e)
    {
        s_EditorCamera.OnEvent(e);
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>(CW_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
        dispatcher.Dispatch<MouseButtonPressedEvent>(CW_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
    }

} // namespace Crowny
