#include "cwepch.h"

#include "Editor/EditorLayer.h"

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
#include "Crowny/Scripting/Mono/MonoArray.h"
#include "Crowny/Scripting/Mono/MonoAssembly.h"
#include "Crowny/Scripting/Mono/MonoMethod.h"
#include "Crowny/Scripting/Mono/MonoProperty.h"
#include "Crowny/Serialization/SceneSerializer.h"

#include "Editor/PrefabUtils.h"

#include "Panels/AssetBrowserPanel.h"
#include "Panels/ComponentEditor.h"
#include "Panels/AudioMixerPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#ifdef CW_WITH_NODES
#include "Panels/NodeEditor/NodeEditorPanel.h"
#endif

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"
#include "UI/Properties.h"
#include "UI/UIUtils.h"

#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptMath.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"
#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptCompression.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptJSON.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptLayerMask.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

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

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
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
                if (settings->RecentScenes.empty())
                {
                    ImGui::BeginDisabled();
                    ImGui::MenuItem("No recent scenes");
                    ImGui::EndDisabled();
                }
                for (const Path& path : settings->RecentScenes)
                {
                    if (ImGui::MenuItem(path.filename().string().c_str()))
                    {
                        m_Layer->OpenScene(path);
                    }
                }
                ImGui::EndMenu();
            }
        }

    private:
        EditorLayer* m_Layer;
    };

    EditorLayer::EditorLayer() : Layer("EditorLayer"), m_SceneRenderer(nullptr) {}

    void EditorLayer::OnAttach()
    {
        RegisterBuiltinNodeTypes();

        EditorAssets::Load();

        Editor::StartUp([this](const Path& path, FileWatch::Change changeType) {
            if (changeType != FileWatch::FileModified && changeType != FileWatch::FileAdded && changeType != FileWatch::FileNewRenamed)
                return;
            ProjectLibrary::Get().Refresh(path);
            if (path.extension() == ".cs")
            {
                Lock lock(m_FileWatchMutex);
                m_AssemblyReloadPending = true;
                m_LastCsChangeTime = std::chrono::steady_clock::now();
            }
        });

        m_MenuBar = new ImGuiMenuBar();

        ImGuiMenu* fileMenu = new ImGuiMenu("File");
        fileMenu->AddItem(new ImGuiMenuItem("New Project", "", [&](auto& event) { m_NewProject = true; }));
        fileMenu->AddItem(new ImGuiMenuItem("Open Project", "", [&](auto& event) { m_OpenProject = true; }));
        fileMenu->AddItem(new ImGuiMenuItem("Save Project", "", [&](auto& event) { Editor::Get().SaveProject(); }));

        fileMenu->AddItem(new ImGuiMenuItem("New Scene", "Ctrl+Shift+N", [&](auto& event) { CreateNewScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Open Scene", "Ctrl+Shift+O", [&](auto& event) { OpenScene(); }));
        fileMenu->AddMenu(new RecentScenesMenu("Open Recent", this));
        fileMenu->AddItem(new ImGuiMenuItem("Save Scene", "Ctrl+S", [&](auto& event) { SaveActiveScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Save Scene as", "Ctrl+Shift+S", [&](auto& event) { SaveActiveSceneAs(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Exit", "Alt+F4", [&](auto& event) { gApplication->Exit(); }));
        m_MenuBar->AddMenu(fileMenu);

        ImGuiMenu* viewMenu = new ImGuiMenu("View");

        // Has to be done before hierarchy and asset browser panels
        m_InspectorPanel = new InspectorPanel("Inspector");
        m_HierarchyPanel = new HierarchyPanel("Hierarchy", [&](Entity e) { m_InspectorPanel->SetSelectedEntity(e); });
        m_ViewportPanel = new ViewportPanel("Viewport");
        m_ViewportPanel->SetEventCallback(CW_BIND_EVENT_FN(OnViewportEvent));
        m_ConsolePanel = new ConsolePanel("Console");
        m_AssetBrowser = new AssetBrowserPanel("Asset browser", [&](const Path& path) { m_InspectorPanel->SetSelectedAssetPath(path); });
        m_AudioMixerPanel = new AudioMixerPanel("Audio Mixer");
        m_AudioMixerPanel->Hide();
#ifdef CW_WITH_NODES
        m_NodeEditorPanel = new NodeEditorPanel("Node Editor");
        m_NodeEditorPanel->Hide();

        m_InspectorPanel->SetOpenNodeEditorCallback([this](AssetHandle<NodeGraphAsset> graphAsset) {
            m_NodeEditorPanel->SetGraph(graphAsset);
            m_NodeEditorPanel->Show();
        });
#endif

        m_ViewportPanel->RegisterInMenu(viewMenu);
        m_InspectorPanel->RegisterInMenu(viewMenu);
        m_HierarchyPanel->RegisterInMenu(viewMenu);
        m_ConsolePanel->RegisterInMenu(viewMenu);
        m_AssetBrowser->RegisterInMenu(viewMenu);
        m_AudioMixerPanel->RegisterInMenu(viewMenu);
#ifdef CW_WITH_NODES
        m_NodeEditorPanel->RegisterInMenu(viewMenu);
#endif

        ImGuiMenu* buildMenu = new ImGuiMenu("Build");
        buildMenu->AddItem(new ImGuiMenuItem("Rebuild game assembly", "Ctrl+Shift+B", CW_BIND_EVENT_FN(RebuildAssemblies)));
        buildMenu->AddItem(new ImGuiMenuItem("Build game", "Ctrl+B", CW_BIND_EVENT_FN(BuildGame)));

        m_MenuBar->AddMenu(buildMenu);
        m_MenuBar->AddMenu(viewMenu);

        CreateRenderTarget();

        UndoRedo::StartUp();

        CodeEditorManager::StartUp();
        // This sets the active code editor so it should happen after the initialization of the manager.
        ApplyEditorSettings();

        BuildManager::StartUp();
        Path engineAssemblyPath = "C:/dev/Crowny/Crowny-Sharp/CrownySharp.dll";
        CodeEditorManager::Get().SyncSolution(GAME_ASSEMBLY, { ScriptProjectReference{ CROWNY_ASSEMBLY, engineAssemblyPath } });

        if (m_Temp == nullptr) // No scene was auto-loaded
            m_Temp = CreateRef<Scene>("Scene");

        m_SceneRenderer = new SceneRenderer(nullptr, m_RenderTarget);
        m_SceneRenderer->Init();
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

    void EditorLayer::SetProjectSettings()
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();
        s_EditorCamera.SetPosition(projSettings->EditorCameraPosition);
        s_EditorCamera.SetFocalPoint(projSettings->EditorCameraFocalPoint);
        s_EditorCamera.SetPitch(projSettings->EditorCameraRotation.x);
        s_EditorCamera.SetYaw(projSettings->EditorCameraRotation.y);
        s_EditorCamera.SetDistance(projSettings->EditorCameraDistance);
        if (fs::is_regular_file(projSettings->LastOpenScenePath))
        {
            Ref<Scene> scene = CreateRef<Scene>(projSettings->LastOpenScenePath, false);
            SceneSerializer serializer(scene);
            serializer.Deserialize(projSettings->LastOpenScenePath);
            m_Temp = scene;
        }
        m_ViewportPanel->SetGizmoMode(projSettings->GizmoMode);
        m_ViewportPanel->SetGizmoLocalMode(projSettings->GizmoLocalMode);

        if (m_Temp != nullptr)
            m_HierarchyPanel->SetSelectedEntity(m_Temp->GetEntityFromUuid(projSettings->LastSelectedEntityID));

        m_HierarchyPanel->SetHierarchy(projSettings->ExpandedEntities);
    }

    void EditorLayer::SaveProjectSettings()
    {
        Ref<ProjectSettings> projSettings = Editor::Get().GetProjectSettings();

        projSettings->EditorCameraPosition = s_EditorCamera.GetPosition();
        projSettings->EditorCameraFocalPoint = s_EditorCamera.GetFocalPoint();
        projSettings->EditorCameraRotation = { s_EditorCamera.GetPitch(), s_EditorCamera.GetYaw() };
        projSettings->EditorCameraDistance = s_EditorCamera.GetDistance();
        const Ref<Scene>& activeScene = gSceneManager->GetActiveScene();
        if (fs::is_regular_file(activeScene->GetFilepath()))
            projSettings->LastOpenScenePath = activeScene->GetFilepath().string();
        projSettings->LastAssetBrowserSelectedEntry = m_AssetBrowser->GetCurrentEntryPath();

        projSettings->GizmoMode = m_ViewportPanel->GetGizmoMode();
        projSettings->GizmoLocalMode = m_ViewportPanel->GetGizmoLocalMode();

        if (HierarchyPanel::GetSelectedEntity())
            projSettings->LastSelectedEntityID = HierarchyPanel::GetSelectedEntity().GetUuid();
        projSettings->ExpandedEntities = m_HierarchyPanel->GetSerializableHierarchy();
    }

    void EditorLayer::ApplyEditorSettings()
    {
        Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        m_ShowDemoWindow = editorSettings->ShowImGuiDemoWindow;
        m_ShowColliders = editorSettings->ShowPhysicsColliders2D;
        m_AutoLoadLastProject = editorSettings->AutoLoadLastProject;
        m_ShowScriptDebugInfo = editorSettings->ShowScriptDebugInfo;
        m_ShowEntityDebugInfo = editorSettings->ShowEntityDebugInfo;

        m_WireframeMode = editorSettings->WireframeMode;
        m_ShowGrid = editorSettings->ShowGrid;
        m_ShowGridAxes = editorSettings->ShowGridAxes;
        m_GridFineSize = editorSettings->GridFineSize;
        m_GridCoarseSize = editorSettings->GridCoarseSize;
        m_GridLineWidth = editorSettings->GridLineWidth;
        m_GridOpacity = editorSettings->GridOpacity;
        m_ColliderColor = editorSettings->ColliderColor;

        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Info, editorSettings->EnableConsoleInfoMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Warn, editorSettings->EnableConsoleWarningMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ConsoleBuffer::Message::Level::Error, editorSettings->EnableConsoleErrorMessages);

        m_ConsolePanel->SetCollapseEnabled(editorSettings->CollapseConsole);
        m_ConsolePanel->SetScrollToBottomEnabled(editorSettings->ScrollToBottom);

        if (m_AutoLoadLastProject && !editorSettings->LastOpenProject.empty())
        {
            Editor::Get().LoadProject(editorSettings->LastOpenProject);
            SetProjectSettings();
            m_AssetBrowser->Initialize();
        }
        if (editorSettings->CodeEditorPath.extension() == ".exe" && fs::exists(editorSettings->CodeEditorPath))
            CodeEditorManager::Get().SetActive(editorSettings->CodeEditorPath);
        else
        {
            // Use the first available code editor.
            const Vector<CodeEditorInstallation>& installations = CodeEditorManager::Get().GetAvailableEditors();
            CodeEditorManager::Get().SetActive(installations[0].ExecutablePath);
        }
    }

    void EditorLayer::BuildGame(Event& event) {}

    void EditorLayer::RebuildAssemblies()
    {
        MonoClass* scriptCompiler = ScriptInfoManager::Get().GetBuiltinClasses().ScriptCompiler;
        uint32_t type = 0;
        bool debug = true;
        const Path engineAssemblyPath = gApplication->GetApplicationDesc().WorkingDirectory / gApplication->GetApplicationDesc().EngineAssemblyPath;

        ScriptArray libDirs = ScriptArray::Create<String>(1);
        libDirs.Set<String>(0, engineAssemblyPath.parent_path().string());
        ScriptArray refs = ScriptArray::Create<String>(1);
        refs.Set<String>(0, engineAssemblyPath.filename().string());

        void* params[6] = { &type, &debug,
                            MonoUtils::ToMonoString((Editor::Get().GetProjectPath() / INTERNAL_ASSEMBLY_PATH).string()),
                            MonoUtils::ToMonoString(ProjectLibrary::Get().GetAssetFolder().string()), libDirs.GetInternal(), refs.GetInternal() };
        scriptCompiler->GetMethod("Compile", 6)->Invoke(nullptr, params);
        Vector<AssemblyRefreshInfo> refreshInfos;
        Path gameAssemblyPath = Editor::Get().GetProjectPath() / INTERNAL_ASSEMBLY_PATH / "GameAssembly.dll";
        CW_ENGINE_INFO("{0}, {1}", gameAssemblyPath, Editor::Get().GetProjectPath());
        refreshInfos.emplace_back(CROWNY_ASSEMBLY, &engineAssemblyPath);
        refreshInfos.emplace_back(GAME_ASSEMBLY, &gameAssemblyPath);
        ScriptObjectManager::Get().RefreshAssemblies(refreshInfos);

        // ScriptInfoManager::Get().InitializeTypes();
        // ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
        // ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
        /*
        auto view = gSceneManager->GetActiveScene()->GetAllEntitiesWith<MonoScriptComponent>();
        for (auto e : view)
        {
            Entity entity = { e, gSceneManager->GetActiveScene().get() };
            auto& msc = entity.GetComponent<MonoScriptComponent>();
            for (auto& script : msc.Scripts)
            {
                script.SetClassName(script.GetTypeName());
                // FIXME script.OnInitialize(entity);
            }
        }*/
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
                OpenScene(fileEntry->Filepath);
            else if (assetType == AssetType::Material)
            {
                Entity entity = PickEntity(fileDragEvent.GetRelativePosition());
                if (entity)
                {
                    const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                    entity.GetComponent<MeshRendererComponent>().SetMaterial(0, static_asset_cast<Material>(assetHandle));
                }
            }
            else if (assetType == AssetType::Mesh)
            {
                Ref<Scene> activeScene = gSceneManager->GetActiveScene();
                Entity entity = activeScene->CreateEntity(fileEntry->Filepath.filename().string());
                MeshRendererComponent& meshRenderer = entity.AddComponent<MeshRendererComponent>();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                meshRenderer.MeshHandle = static_asset_cast<Mesh>(assetHandle);
            }
            else if (assetType == AssetType::AudioClip)
            {
                Ref<Scene> activeScene = gSceneManager->GetActiveScene();
                Entity entity = activeScene->CreateEntity(fileEntry->Filepath.filename().string());
                AudioSourceComponent& audioSourceComponent = entity.AddComponent<AudioSourceComponent>();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                audioSourceComponent.SetClip(static_asset_cast<AudioClip>(assetHandle));
            }
            else if (assetType == AssetType::Prefab)
            {
                Ref<Scene> activeScene = gSceneManager->GetActiveScene();
                const AssetHandle<Asset> assetHandle = ProjectLibrary::Get().Load(fileEntry);
                AssetHandle<Prefab> prefab = static_asset_cast<Prefab>(assetHandle);
                Entity root = activeScene->GetRootEntity();
                PrefabUtils::InstantiatePrefab(prefab, root);
            }
            return true;
        });
        return true;
    }

    void EditorLayer::CreateNewScene()
    {
        m_Temp = CreateRef<Scene>("Scene");
        m_Temp->SetEditorScene(true);
        const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + m_Temp->GetName();
        gApplication->GetWindow().SetTitle(title);
    }

    void EditorLayer::OpenScene()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, outPaths, "Open Scene", ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }))
            OpenScene(outPaths[0].replace_extension(".cwscene"));
    }

    void EditorLayer::OpenScene(const Path& filepath)
    {
        m_Temp = CreateRef<Scene>(filepath.string(), false);
        m_Temp->SetEditorScene(true);
        SceneSerializer serializer(m_Temp);
        serializer.Deserialize(filepath);
        AddRecentScene(filepath);
    }

    void EditorLayer::SaveActiveSceneAs()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, outPaths, "Save scene", ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }))
        {
            const Path path = outPaths[0].replace_extension(".cwscene");
            const auto& scene = gSceneManager->GetActiveScene();
            scene->SetImGuiLayout(gApplication->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            serializer.Serialize(path);
            AddRecentScene(path);
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + gSceneManager->GetActiveScene()->GetName();
            gApplication->GetWindow().SetTitle(title);
        }
    }

    void EditorLayer::SaveActiveScene()
    {
        const auto& scene = gSceneManager->GetActiveScene();
        if (scene->GetFilepath().empty())
            SaveActiveSceneAs();
        else
        {
            scene->SetImGuiLayout(gApplication->GetImGuiLayer()->SaveLayout());
            SceneSerializer serializer(scene);
            serializer.Serialize(scene->GetFilepath());
            AddRecentScene(scene->GetFilepath());
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + scene->GetName();
            gApplication->GetWindow().SetTitle(title);
        }
    }

    void EditorLayer::AddRecentScene(const Path& path)
    {
        Ref<ProjectSettings> settings = Editor::Get().GetProjectSettings();
        auto& recentScenes = settings->RecentScenes;

        auto it = std::find(recentScenes.begin(), recentScenes.end(), path);
        if (it != recentScenes.end())
            recentScenes.erase(it);

        recentScenes.insert(recentScenes.begin(), path);

        if (recentScenes.size() > 5)
            recentScenes.resize(5);
    }

    void EditorLayer::AddRecentEntry(const Path& path)
    {
        // TODO: Don't write code like this...
        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        uint32_t recentIdx = settings->RecentProjects.size() > 0 ? (uint32_t)(settings->RecentProjects.size() - 1) : 0;
        for (uint32_t i = 0; i < settings->RecentProjects.size(); i++)
        {
            if (settings->RecentProjects[i].ProjectPath == Editor::Get().GetProjectPath())
                recentIdx = i;
        }
        for (uint32_t i = recentIdx; i >= 1; i--)
            settings->RecentProjects[i] = settings->RecentProjects[i - 1];

        settings->RecentProjects[0].Timestamp = std::time(nullptr);
        settings->RecentProjects[0].ProjectPath = path;
    }

    void EditorLayer::OnDetach()
    {
        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        settings->ShowImGuiDemoWindow = m_ShowDemoWindow;
        settings->ShowPhysicsColliders2D = m_ShowColliders;
        settings->AutoLoadLastProject = m_AutoLoadLastProject;
        settings->ShowEntityDebugInfo = m_ShowEntityDebugInfo;
        settings->ShowAssetInfo = m_ShowAssetInfo;
        settings->ShowScriptDebugInfo = m_ShowScriptDebugInfo;

        settings->WireframeMode = m_WireframeMode;
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
        settings->CodeEditorPath = CodeEditorManager::Get().GetActiveEditorPath();

        EditorAssets::Unload();
        Editor::Get().SaveProject();

        AddRecentEntry(Editor::Get().GetProjectPath());
        SaveProjectSettings();
        SaveActiveScene();
        Editor::Shutdown();

        delete m_SceneRenderer;
        delete m_InspectorPanel;
        delete m_HierarchyPanel;
        delete m_ViewportPanel;
        delete m_ConsolePanel;
        delete m_AssetBrowser;
        delete m_AudioMixerPanel;
    }

    void EditorLayer::ExecuteProjectAssetRefresh()
    {
        Vector<Path> queueCopy;
        {
            Lock lock(m_FileWatchMutex);
            m_FileWatchQueue.swap(queueCopy);
        }

        // This is detecting the .meta? yikes
        for (const Path& path : queueCopy)
            ProjectLibrary::Get().Refresh(path);
    }

    Entity EditorLayer::PickEntity()
    {
        const Ref<Scene> scene = gSceneManager->GetActiveScene();
        const glm::vec4& bounds = m_ViewportPanel->GetViewportBounds();
        const ImVec2 mouseCoords = ImGui::GetMousePos();
        glm::vec2 coords = { mouseCoords.x - bounds.x, mouseCoords.y - bounds.y };
        coords.y = m_ViewportSize.y - coords.y - 1;
        return PickEntity(coords);
    }

    Entity EditorLayer::PickEntity(const glm::vec2& coords)
    {
        RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
        // TODO: This is bad: allocates every frame a full image.
        Ref<PixelData> outPixelData =
          PixelData::Create(rt->GetColorTexture(1)->GetWidth(), rt->GetColorTexture(1)->GetHeight(), rt->GetColorTexture(1)->GetFormat());
        rt->GetColorTexture(1)->ReadData(*outPixelData);
        if (outPixelData->GetWidth() > coords.x && outPixelData->GetHeight() > coords.y)
        {
            const glm::vec4 col = outPixelData->GetColorAt((uint32_t)coords.x, (uint32_t)coords.y);
            const Ref<Scene> scene = gSceneManager->GetActiveScene();
            if (col.x == 0.0f)
                return Entity(entt::null, scene.get());
            else
                return Entity((entt::entity)(col.x - 1), scene.get());
        }
        return Entity(entt::null, nullptr);
    }

    void EditorLayer::HandleRenderTargetResize()
    {
        Ref<Scene> scene = gSceneManager->GetActiveScene();
        auto& rapi = *gRenderAPI;
        if (m_ViewportPanel->IsShown() &&
            (m_ViewportSize.x != m_ViewportPanel->GetViewportSize().x || m_ViewportSize.y != m_ViewportPanel->GetViewportSize().y)) // TODO: Move out
        {
            scene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
            TextureDesc colorParams;
            colorParams.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            colorParams.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;
            colorParams.DebugName = "EditorLayer/ViewportColor";

            TextureDesc objectId;
            objectId.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            objectId.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            objectId.Format = TextureFormat::R32I;
            objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);
            objectId.DebugName = "EditorLayer/ViewportObjectId";

            TextureDesc depthParams;
            depthParams.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            depthParams.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
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
            rtProps.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            rtProps.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            m_RenderTarget = RenderTexture::Create(rtProps);
        }
        m_ViewportSize = m_ViewportPanel->GetViewportSize();
        m_SceneRenderer->SetRenderTarget(m_RenderTarget);

        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);
    }

    void EditorLayer::SubmitSnapshot(RenderSnapshot&& snapshot)
    {
        RenderThread* renderThread = gApplication->GetRenderThread();
        if (renderThread && renderThread->IsRunning())
        {
            renderThread->SubmitFrame(std::move(snapshot));
        }
        else
        {
            // Fallback: single-threaded rendering
            SceneRenderer::RenderFromSnapshot(snapshot);
        }
    }

    void EditorLayer::HandleSceneState(Timestep ts)
    {
        switch (m_SceneState)
        {
        case SceneState::Edit: {
            s_EditorCamera.SetViewportSize((float)m_RenderTarget->GetProperties().Width, (float)m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);

            gSceneManager->GetActiveScene()->OnUpdateEditor(ts);
            m_SceneRenderer->UpdateProceduralMeshes();
            auto snapshot = m_SceneRenderer->ExtractSnapshot(s_EditorCamera, s_EditorCamera.GetViewMatrix(), m_ShowGrid);
            snapshot.OverridePolygonMode = m_WireframeMode ? PolygonMode::Wireframe : PolygonMode::Solid;
            snapshot.Grid = { m_GridFineSize, m_GridCoarseSize, m_GridLineWidth, m_GridOpacity, m_ShowGridAxes };
            SubmitSnapshot(std::move(snapshot));
            break;
        }
        case SceneState::Play: {
            gSceneManager->GetActiveScene()->OnUpdateRuntime(ts);
            ScriptRuntime::OnUpdate();
            m_SceneRenderer->UpdateProceduralMeshes();
            auto snapshot = m_SceneRenderer->ExtractSnapshot();
            SubmitSnapshot(std::move(snapshot));
            Time::Update(ts, gApplication->GetTimeSettings()->FixedTimestep);
            break;
        }
        case SceneState::Simulate: {
            s_EditorCamera.SetViewportSize((float)m_RenderTarget->GetProperties().Width, (float)m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);
            gSceneManager->GetActiveScene()->OnSimulationUpdate(ts);
            m_SceneRenderer->UpdateProceduralMeshes();
            auto snapshot = m_SceneRenderer->ExtractSnapshot(s_EditorCamera, s_EditorCamera.GetViewMatrix(), m_ShowGrid);
            snapshot.OverridePolygonMode = m_WireframeMode ? PolygonMode::Wireframe : PolygonMode::Solid;
            snapshot.Grid = { m_GridFineSize, m_GridCoarseSize, m_GridLineWidth, m_GridOpacity, m_ShowGridAxes };
            SubmitSnapshot(std::move(snapshot));
            break;
        }
        }
    }

    void EditorLayer::OnUpdate(Timestep ts)
    {
        // Process completed async imports (GPU init on main thread)
        if (ProjectLibrary::IsStartedUp() && ProjectLibrary::Get().IsImporting())
            ProjectLibrary::Get().ProcessCompletedImports();

        if (m_Temp) // Delay scene reload
        {
            Ref<Scene> activeScene = gSceneManager->GetActiveScene();
            if (activeScene)
                activeScene->SetImGuiLayout(gApplication->GetImGuiLayer()->SaveLayout());
            m_SceneRenderer->SetScene(m_Temp);
            gSceneManager->SetActiveScene(m_Temp);
            gApplication->GetImGuiLayer()->LoadLayout(m_Temp->GetImGuiLayout());
            Editor::Get().GetProjectSettings()->LastOpenScenePath = m_Temp->GetFilepath().string();
            m_Temp = nullptr;
            // ScriptRuntime::Init();
            const String title = "Crowny Editor - " + Editor::Get().GetProjectName() + " - " + gSceneManager->GetActiveScene()->GetName();
            gApplication->GetWindow().SetTitle(title);
        }

        if (m_AssemblyReloadPending && m_SceneState == SceneState::Edit)
        {
            float elapsed;
            {
                Lock lock(m_FileWatchMutex);
                elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_LastCsChangeTime).count();
            }
            if (elapsed >= 1.0f)
            {
                // Clear flag before rebuild. If .cs files change during rebuild,
                // a new debounce cycle starts (won't cause infinite loop).
                m_AssemblyReloadPending = false;
                RebuildAssemblies();
            }
        }

        Ref<Scene> scene = gSceneManager->GetActiveScene();
        HandleRenderTargetResize();
        HandleSceneState(ts);

        // Wait for render thread to finish scene recording before any main-thread
        // rendering (RenderOverlay) or readback (PickEntity) that touches the same
        // command buffer or render target.
        RenderThread* rt = gApplication->GetRenderThread();
        if (rt && rt->IsRunning())
            rt->WaitForFrameDone();

        RenderOverlay();

        if (m_ViewportPanel->IsHovered())
        {
            if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !Input::IsKeyPressed(Key::LeftAlt) && !Input::IsKeyPressed(Key::RightAlt) &&
                !m_ViewportPanel->IsMouseOverGizmo())
            {
                const Entity pickedEntity = PickEntity();
                m_HierarchyPanel->SetSelectedEntity(pickedEntity);
            }
        }

        m_HierarchyPanel->Update();
        ScriptObjectManager::Get().Update();
    }

    void EditorLayer::RenderOverlay()
    {
        Ref<Scene> scene = gSceneManager->GetActiveScene();
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
        {
            {
                auto view = scene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
                for (auto e : view)
                {
                    const auto& bc2d = view.get<BoxCollider2DComponent>(e);
                    Entity entity(e, scene.get());
                    const glm::mat4 world = entity.GetWorldMatrix();
                    const glm::mat4 colliderTransform = world
                        * glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.GetOffset(), 0.0f))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(bc2d.GetSize() * 2.0f, 1.0f));
                    Renderer2D::DrawRect(colliderTransform, m_ColliderColor, 0.01f);
                }
            }

            {
                auto view = scene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
                for (auto e : view)
                {
                    const auto& cc2d = view.get<CircleCollider2DComponent>(e);
                    Entity entity(e, scene.get());
                    const glm::mat4 world = entity.GetWorldMatrix();
                    const glm::mat4 colliderTransform = world
                        * glm::translate(glm::mat4(1.0f), glm::vec3(cc2d.GetOffset(), 0.0f))
                        * glm::scale(glm::mat4(1.0f), glm::vec3(cc2d.GetRadius() * 2.0f));
                    Renderer2D::DrawCircle(colliderTransform, m_ColliderColor, 0.05f);
                }
            }
        }

        // Audio cone gizmo for the selected entity. Drawn only for the selection so the viewport
        // isn't flooded when there are many AudioSources in the scene.
        Entity selected = HierarchyPanel::GetSelectedEntity();
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
            const auto& activeScene = gSceneManager->GetActiveScene();
            if (activeScene)
                title += " - " + activeScene->GetName();
        }
        ImGui::Begin(title.c_str(), &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("Crowny Editor");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
    }

    void EditorLayer::OnImGuiRender()
    {
        SetupImGuiRender();

        // When no project is loaded, show the project hub and skip the editor UI
        if (!Editor::Get().IsProjectLoaded())
        {
            UI_ProjectManager();
            ImGui::End();
            return;
        }

        m_MenuBar->Render();
        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);

        // UI_ProjectManager handles menu-triggered open/new even when a project is loaded
        UI_ProjectManager();
        UI_Header();
        UI_GizmoSettings();
        UI_ViewportSettings();
        UI_Settings();
        UI_Physics2DSettings();
        UI_TimeSettings();

#ifdef CW_DEBUG
        UI_ScriptInfo();
        UI_AssetInfo();
        UI_EntityDebugInfo();
        gPhysics2D->UIStats();
#endif

        m_HierarchyPanel->Render();
        m_InspectorPanel->Render();
        m_ViewportPanel->SetEditorRenderTarget(m_RenderTarget);
        m_ViewportPanel->Render();
        m_ConsolePanel->Render();
        m_AssetBrowser->Render();
        if (m_AudioMixerPanel->IsShown())
            m_AudioMixerPanel->Render();
#ifdef CW_WITH_NODES
        m_NodeEditorPanel->Render();
#endif

        ImGui::End(); // End dockspace

        // Status bar at the very bottom — rendered OUTSIDE the dockspace
        if (ProjectLibrary::IsStartedUp() && ProjectLibrary::Get().IsImporting())
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float statusBarHeight = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - statusBarHeight));
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, statusBarHeight));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("##StatusBar", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoNav);

            const auto& progress = ProjectLibrary::Get().GetImportProgress();
            char text[128];
            snprintf(text, sizeof(text), "Importing assets... %u / %u", progress.CompletedFiles.load(), progress.TotalFiles.load());
            ImGui::Text("%s", text);
            ImGui::SameLine();
            ImGui::ProgressBar(progress.GetFraction(), ImVec2(200, ImGui::GetFrameHeight() - 4));

            ImGui::End();
            ImGui::PopStyleVar(3);
        }
    }

    void EditorLayer::UI_ProjectManager()
    {
        // Handle menu-item flags when a project is already loaded:
        // Use native dialogs directly rather than showing the hub.
        if (Editor::Get().IsProjectLoaded())
        {
            if (m_OpenProject)
            {
                m_OpenProject = false;
                Vector<Path> outPaths;
                if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Open Project", Editor::Get().GetDefaultProjectPath()))
                {
                    if (outPaths.size() > 0)
                    {
                        SaveProjectSettings();
                        m_AssetBrowser->Unload();
                        Editor::Get().LoadProject(outPaths[0]);
                        Editor::Get().GetEditorSettings()->LastOpenProject = outPaths[0];
                        SetProjectSettings();
                        m_AssetBrowser->Initialize();
                    }
                }
            }
            if (m_NewProject)
            {
                m_NewProject = false;
                SaveProjectSettings();
                m_AssetBrowser->Unload();
                Editor::Get().UnloadProject();
                m_HubPage = HubPage::NewProject;
                m_NewProjectPath = Editor::Get().GetDefaultProjectPath().string();
                m_NewProjectName = "New Project";
                // Will render hub on the next frame since project is now unloaded
            }
            return;
        }

        // Below: no project is loaded -- render the fullscreen hub

        // Fullscreen hub window covering the entire viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(viewport->Size);
        ImGuiWindowFlags hubFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##ProjectHub", nullptr, hubFlags);
        ImGui::PopStyleVar();

        const float sidebarWidth = 200.0f;
        const ImVec2 windowSize = ImGui::GetContentRegionAvail();

        // ---- Left sidebar ----
        {
            ImGui::BeginChild("##HubSidebar", ImVec2(sidebarWidth, windowSize.y), true);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));

            // Title
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);
            ImGui::SetCursorPosX(16.0f);
            ImGui::TextUnformatted("CROWNY");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Navigation buttons
            const float buttonWidth = sidebarWidth - ImGui::GetStyle().WindowPadding.x * 2.0f;
            bool isRecent = (m_HubPage == HubPage::RecentProjects);
            if (isRecent)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("Projects", ImVec2(buttonWidth, 0)))
                m_HubPage = HubPage::RecentProjects;
            if (isRecent)
                ImGui::PopStyleColor();

            bool isNew = (m_HubPage == HubPage::NewProject);
            if (isNew)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("New Project", ImVec2(buttonWidth, 0)))
            {
                m_HubPage = HubPage::NewProject;
                if (m_NewProjectPath.empty())
                {
                    m_NewProjectPath = Editor::Get().GetDefaultProjectPath().string();
                    m_NewProjectName = "New Project";
                }
            }
            if (isNew)
                ImGui::PopStyleColor();

            ImGui::Separator();

            // Version info pushed to bottom
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y);
            ImGui::SetCursorPosX(16.0f);
            ImGui::TextDisabled("v0.1.0-dev");

            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        ImGui::SameLine();

        // ---- Right content area ----
        {
            ImGui::BeginChild("##HubContent", ImVec2(0, windowSize.y), false);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
            const float contentPadding = 16.0f;
            ImGui::SetCursorPos(ImVec2(contentPadding, contentPadding));

            if (m_HubPage == HubPage::RecentProjects)
            {
                // ---- Recent Projects page ----
                ImGui::TextUnformatted("Recent Projects");
                ImGui::Spacing();

                // Search bar
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - contentPadding - 120.0f);
                ImGui::InputTextWithHint("##projectSearch", "Search projects...", &m_RecentSearchFilter);
                ImGui::SameLine();
                if (ImGui::Button("Open Folder", ImVec2(110.0f, 0)))
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Open Project", Editor::Get().GetDefaultProjectPath()))
                    {
                        if (outPaths.size() > 0)
                        {
                            Editor::Get().LoadProject(outPaths[0]);
                            Editor::Get().GetEditorSettings()->LastOpenProject = outPaths[0];
                            SetProjectSettings();
                            m_AssetBrowser->Initialize();
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Scrollable project list
                ImGui::BeginChild("##RecentList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 8.0f), false);

                Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
                for (uint32_t i = 0; i < settings->RecentProjects.size(); i++)
                {
                    const RecentProject& project = settings->RecentProjects[i];
                    if (project.ProjectPath.empty())
                        continue;

                    // Apply search filter
                    String projectName = project.ProjectPath.filename().string();
                    if (!m_RecentSearchFilter.empty())
                    {
                        String lowerName = projectName;
                        String lowerFilter = m_RecentSearchFilter;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                        std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                        if (lowerName.find(lowerFilter) == String::npos)
                            continue;
                    }

                    // Format timestamp safely
                    char timeStr[30] = "Unknown";
                    if (project.Timestamp != 0)
                    {
                        tm timeinfo;
#ifdef CW_PLATFORM_WIN32
                        localtime_s(&timeinfo, &project.Timestamp);
#else
                        localtime_r(&project.Timestamp, &timeinfo);
#endif
                        strftime(timeStr, sizeof(timeStr), "%c", &timeinfo);
                    }

                    ImGui::PushID(static_cast<int>(i));

                    const bool isSelected = (m_SelectedRecentIdx == static_cast<int>(i));
                    const float itemHeight = ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().ItemSpacing.y + 8.0f;

                    if (ImGui::Selectable("##recentEntry", isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, itemHeight)))
                    {
                        m_SelectedRecentIdx = static_cast<int>(i);
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            Editor::Get().LoadProject(project.ProjectPath);
                            Editor::Get().GetEditorSettings()->LastOpenProject = project.ProjectPath;
                            SetProjectSettings();
                            m_AssetBrowser->Initialize();
                        }
                    }

                    // Draw project info on top of the selectable
                    const ImVec2 itemMin = ImGui::GetItemRectMin();
                    ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 8.0f, itemMin.y + 4.0f));
                    ImGui::TextUnformatted(projectName.c_str());
                    ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 8.0f, itemMin.y + 4.0f + ImGui::GetTextLineHeightWithSpacing()));
                    ImGui::TextDisabled("%s  |  %s", project.ProjectPath.string().c_str(), timeStr);

                    ImGui::PopID();
                }

                ImGui::EndChild();

                // Bottom action bar
                ImGui::Separator();
                ImGui::Spacing();

                const bool hasSelection = m_SelectedRecentIdx >= 0 && m_SelectedRecentIdx < static_cast<int>(settings->RecentProjects.size()) &&
                                    !settings->RecentProjects[m_SelectedRecentIdx].ProjectPath.empty();

                if (!hasSelection)
                    ImGui::BeginDisabled();

                if (ImGui::Button("Open", ImVec2(80.0f, 0)))
                {
                    const RecentProject& sel = settings->RecentProjects[m_SelectedRecentIdx];
                    Editor::Get().LoadProject(sel.ProjectPath);
                    Editor::Get().GetEditorSettings()->LastOpenProject = sel.ProjectPath;
                    SetProjectSettings();
                    m_AssetBrowser->Initialize();
                }
                ImGui::SameLine();
                if (ImGui::Button("-", ImVec2(30.0f, 0)))
                {
                    for (uint32_t j = m_SelectedRecentIdx; j < settings->RecentProjects.size() - 1; j++)
                        settings->RecentProjects[j] = settings->RecentProjects[j + 1];
                    settings->RecentProjects[settings->RecentProjects.size() - 1].ProjectPath.clear();
                    settings->RecentProjects[settings->RecentProjects.size() - 1].Timestamp = 0;
                    m_SelectedRecentIdx = -1;
                }
                UI::SetTooltip("Remove from recents");
                ImGui::SameLine();
                if (ImGui::ImageButton("##ShowInExplorer", ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon), ImVec2(18.0f, 18.0f),
                                       { 0, 1 }, { 1, 0 }))
                    PlatformUtils::ShowInExplorer(settings->RecentProjects[m_SelectedRecentIdx].ProjectPath);
                UI::SetTooltip("Show in explorer");

                if (!hasSelection)
                    ImGui::EndDisabled();
            }
            else if (m_HubPage == HubPage::NewProject)
            {
                // ---- New Project page ----
                ImGui::TextUnformatted("Create New Project");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextUnformatted("Project Name");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - contentPadding);
                ImGui::InputText("##newProjectName", &m_NewProjectName);

                ImGui::Spacing();
                ImGui::TextUnformatted("Location");
                float browseButtonWidth = 40.0f;
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - contentPadding - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x);
                ImGui::InputText("##newProjectPath", &m_NewProjectPath);
                ImGui::SameLine();
                if (ImGui::Button("...", ImVec2(browseButtonWidth, 0)))
                {
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, outPaths, "Select Location", Path(m_NewProjectPath)))
                    {
                        if (outPaths.size() > 0)
                            m_NewProjectPath = outPaths[0].string();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();

                // Real-time validation
                const bool pathExists = fs::exists(m_NewProjectPath);
                const bool projectExists = pathExists && fs::exists(Path(m_NewProjectPath) / m_NewProjectName);
                const bool nameEmpty = m_NewProjectName.empty();
                const bool canCreate = pathExists && !projectExists && !nameEmpty;

                if (nameEmpty)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Project name cannot be empty");
                    ImGui::PopStyleColor();
                }
                else if (!pathExists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Path does not exist");
                    ImGui::PopStyleColor();
                }
                else if (projectExists)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("A project with this name already exists");
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    ImGui::TextUnformatted("Ready to create");
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();

                if (!canCreate)
                    ImGui::BeginDisabled();

                if (ImGui::Button("Create", ImVec2(120.0f, 0)))
                {
                    Editor::Get().CreateProject(m_NewProjectPath, m_NewProjectName);
                    Path newProjectPath = Path(m_NewProjectPath) / m_NewProjectName;
                    Editor::Get().LoadProject(newProjectPath);
                    Editor::Get().GetEditorSettings()->LastOpenProject = newProjectPath;
                    SetProjectSettings();
                    m_NewProjectPath.clear();
                    m_NewProjectName.clear();
                    m_AssetBrowser->Initialize();
                }

                if (!canCreate)
                    ImGui::EndDisabled();
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        ImGui::End();
    }

    void EditorLayer::UI_EntityDebugInfo()
    {
        if (m_ShowEntityDebugInfo)
        {
            ImGui::Begin("Entity Debug Info", &m_ShowEntityDebugInfo);
            const Ref<Scene> scene = gSceneManager->GetActiveScene();
            auto view = scene->GetAllEntitiesWith<TagComponent>();
            for (auto e : view)
            {
                Entity entity = Entity(e, scene.get());
                const String label = entity.GetName() + ": " + entity.GetUuid().ToString();
                if (ImGui::TreeNode(label.c_str()))
                {
                    if (entity.GetParent())
                    {
                        const String parentLabel = entity.GetParent().GetName() + ": " + entity.GetParent().GetUuid().ToString();
                        ImGui::Text("%s", parentLabel.c_str());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::End();
        }
    }

    void EditorLayer::UI_AssetInfo()
    {
        if (m_ShowAssetInfo)
        {
            ImGui::Begin("Asset Info", &m_ShowAssetInfo);
            ImGui::Columns(3);
            ImGui::Text("Project Library");
            ImGui::NextColumn();
            ImGui::Text("Show empty metadata entries");
            ImGui::SameLine();
            ImGui::Checkbox("##showEmptyMetadata", &m_ShowEmptyMetadataAssetInfo);
            ImGui::NextColumn();
            ImGui::NextColumn();

            static const Map<AssetType, const char*> assetTypes = {
                { AssetType::None, "None" },
                { AssetType::AudioClip, "Audio Clip" },
                { AssetType::Material, "Material" },
                { AssetType::Mesh, "Mesh" },
                { AssetType::MeshSource, "Mesh Source" },
                { AssetType::PhysicsMaterial, "Physics Material" },
                { AssetType::PhysicsMaterial2D, "Physics Material 2D" },
                { AssetType::PhysicsMesh, "Physics Mesh" },
                { AssetType::PlainText, "Plain Text" },
                { AssetType::ScriptCode, "Script Code" },
                { AssetType::Shader, "Shader" },
                { AssetType::Texture, "Texture" },
                { AssetType::Font, "Font" },
                { AssetType::Scene, "Scene" },
            };

            std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
                if (entry->Type == LibraryEntryType::Directory)
                {
                    for (auto& child : StaticRefCast<DirectoryEntry>(entry)->Children)
                        traverse(child);
                }
                else
                {
                    FileEntry* file = static_cast<FileEntry*>(entry.get());

                    if (!m_ShowEmptyMetadataAssetInfo && file->Metadata == nullptr)
                        return;
                    ImGui::Text("%s", file->Filepath.string().c_str());
                    ImGui::NextColumn();
                    if (file->Metadata != nullptr)
                        ImGui::Text("%s", file->Metadata->Uuid.ToString().c_str());
                    ImGui::NextColumn();
                    if (file->Metadata != nullptr && assetTypes.count(file->Metadata->Type))
                        ImGui::Text("%s", assetTypes.at(file->Metadata->Type));
                    ImGui::NextColumn();
                }
            };
            const Ref<DirectoryEntry>& root = ProjectLibrary::Get().GetRoot();
            traverse(root);
            ImGui::End();
        }
    }

    inline void AddTextVertical(ImDrawList* DrawList, const char* text, ImVec2 pos, ImU32 text_color)
    {
        pos.x = IM_ROUND(pos.x);
        pos.y = IM_ROUND(pos.y);
        ImFont* font = GImGui->Font;
        const ImFontGlyph* glyph;
        char c;
        ImGuiContext& g = *GImGui;
        ImVec2 text_size = ImGui::CalcTextSize(text);
        while ((c = *text++))
        {
            glyph = GImGui->FontBaked->FindGlyph(c);
            if (!glyph)
                continue;

            DrawList->PrimReserve(6, 4);
            DrawList->PrimQuadUV(pos + ImVec2(glyph->Y0, -glyph->X0), pos + ImVec2(glyph->Y0, -glyph->X1), pos + ImVec2(glyph->Y1, -glyph->X1),
                                 pos + ImVec2(glyph->Y1, -glyph->X0),

                                 ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V0), ImVec2(glyph->U1, glyph->V1),
                                 ImVec2(glyph->U0, glyph->V1), text_color);
            pos.y -= glyph->AdvanceX;
        }
    }

    static bool s_OpenCollisionMatrix = true;

    void EditorLayer::UI_Physics2DSettings()
    {
        ImGui::Begin("Physics 2D", &s_OpenCollisionMatrix);
        {
            UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2{ 2.0f, 2.0f });

            UI::BeginPropertyGrid();
            glm::vec2 gravity = gPhysics2D->GetGravity();
            if (UI::Property("Gravity", gravity))
                gPhysics2D->SetGravity(gravity);

            uint32_t velocityIterations = gPhysics2D->GetVelocityIterations();
            if (UI::Property("Velocity iterations", velocityIterations))
                gPhysics2D->SetVelocityIterations(velocityIterations);

            uint32_t positionIterations = gPhysics2D->GetPositionIterations();
            if (UI::Property("Position iterations", positionIterations))
                gPhysics2D->SetPositionIterations(positionIterations);
            UI::EndPropertyGrid();
            if (ImGui::CollapsingHeader("Layer Names"))
            {
                UI::BeginPropertyGrid();
                uint32_t lastNonEmptyIdx = 0;
                for (int32_t i = 31; i >= 0; i--)
                {
                    if (!gPhysics2D->GetLayerName(i).empty())
                    {
                        lastNonEmptyIdx = i;
                        break;
                    }
                }
                for (uint32_t i = 0; i < 32; i++)
                {
                    if (i > lastNonEmptyIdx + 1 && !UI::IsItemDisabled()) // Give the user exactly one non-disabled layer field
                        ImGui::BeginDisabled(true);
                    String layerName = gPhysics2D->GetLayerName(i);
                    if (UI::Property(fmt::format("Layer {0}", i).c_str(), layerName))
                        gPhysics2D->SetLayerName(i, layerName);
                }
                if (UI::IsItemDisabled())
                    ImGui::EndDisabled();
                UI::EndPropertyGrid();
            }

            // How not to pass code review 101
            if (ImGui::CollapsingHeader("Collision Matrix"))
            {
                UI::PushID();
                uint32_t id = 0;
                uint32_t nonEmpty = 0;
                float maxTextLength = 0;
                for (uint32_t i = 0; i < 32; i++)
                {
                    maxTextLength = std::max(maxTextLength, ImGui::CalcTextSize(gPhysics2D->GetLayerName(i).c_str()).x);
                    nonEmpty += !gPhysics2D->GetLayerName(i).empty();
                }
                nonEmpty--;
                UI::ShiftCursorY(maxTextLength);
                const ImVec2 text_pos(ImGui::GetCurrentWindow()->DC.CursorPos.x, ImGui::GetCurrentWindow()->DC.CursorPos.y - 2.0f);
                uint32_t ii = 0;
                for (uint32_t i = 0; i < 32; i++) // rows
                {
                    const uint32_t categoryMask = gPhysics2D->GetCategoryMask(i);
                    if (gPhysics2D->GetLayerName(i).empty())
                        continue;
                    ii++;
                    UI::ShiftCursorX(10);
                    ImGui::Text("%s", gPhysics2D->GetLayerName(i).c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(maxTextLength + ImGui::GetStyle().WindowPadding.x + 2 + 10);
                    uint32_t jj = 0;
                    for (uint32_t j = 0; j < 32; j++) // cols
                    {
                        if (ii + jj > nonEmpty + 1 || gPhysics2D->GetLayerName(j).empty())
                            continue;
                        jj++;
                        if (ii == 1)
                        {
                            AddTextVertical(ImGui::GetWindowDrawList(), gPhysics2D->GetLayerName(j).c_str(),
                                            text_pos + ImVec2(ImGui::GetCursorPosX() - 6.0f, 0), IM_COL32(192, 192, 192, 255));
                        }
                        bool value = (categoryMask & (1 << j)) != 0;
                        ImGui::PushID(id++);
                        if (ImGui::Checkbox("##checkbox", &value))
                        {
                            if (value)
                                gPhysics2D->SetCategoryMask(i, categoryMask | (1 << j));
                            else
                                gPhysics2D->SetCategoryMask(i, categoryMask & (~(1 << j)));
                        }
                        if (ii + jj <= nonEmpty + 1)
                            ImGui::SameLine();
                        ImGui::PopID();
                    }
                }
                UI::PopID();
            }
        }
        ImGui::End();
    }

    void EditorLayer::UI_TimeSettings()
    {
        ImGui::Begin("Time Settings", &m_ShowTimeSettings);
        const Ref<TimeSettings>& timeSettings = gApplication->GetTimeSettings();
        UI::Property("Time Scale", timeSettings->TimeScale);
        UI::Property("Fixed Timestep", timeSettings->FixedTimestep);
        UI::Property("Max Timestep", timeSettings->MaxTimestep);
        ImGui::End();
    }

    static void DrawClass(MonoClass* klass)
    {
        if (ImGui::TreeNode(klass->GetName().c_str()))
        {
            if (ImGui::TreeNode("Methods"))
            {
                for (MonoMethod* method : klass->GetMethods())
                {
                    if (ImGui::TreeNode(method->GetName().c_str()))
                    {
                        for (MonoClass* attribute : method->GetAttributes())
                            DrawClass(attribute);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Fields"))
            {
                for (MonoField* field : klass->GetFields())
                {
                    bool expanded = ImGui::TreeNode(field->GetFullDeclName().c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s", field->GetType()->GetFullName().c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s", field->GetName().c_str());
                    if (expanded)
                    {
                        for (MonoClass* attribute : field->GetAttributes())
                            DrawClass(attribute);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Properties"))
            {
                for (MonoProperty* prop : klass->GetProperties())
                {
                    if (ImGui::TreeNode(prop->GetName().c_str()))
                    {
                        for (MonoClass* attribute : prop->GetAttributes())
                            DrawClass(attribute);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Attributes"))
            {
                for (MonoClass* attribute : klass->GetAttributes())
                    DrawClass(attribute);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }

    void EditorLayer::UI_ScriptInfo()
    {
        if (m_ShowScriptDebugInfo)
        {
            ImGui::Begin("C# debug", &m_ShowScriptDebugInfo);
            static AssetHandle<AudioClip> audioHandle;
            UIUtils::AssetReference<AudioClip>("Clip", audioHandle);
            static AssetHandle<Shader> shaderHandle;
            UIUtils::AssetReference<Shader>("Shader", shaderHandle);
            MonoAssembly* gameAssembly = MonoManager::Get().GetAssembly(GAME_ASSEMBLY);
            for (MonoClass* klass : gameAssembly->GetClasses())
            {
                DrawClass(klass);
            }
            ImGui::End();
        }
    }

    void EditorLayer::UI_Header()
    {
        UI::PushID();

        UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
        UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
        UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        const float buttonSize = 18.0f + 5.0f;
        const float edgeOffset = 4.0f;
        const float windowHeight = 32.0f;
        const float numberOfButtons = 3.0f;
        const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

        const float toolbarX = (m_ViewportPanel->GetViewportBounds().x + m_ViewportPanel->GetViewportBounds().z) / 2.0f;
        ImGui::SetNextWindowPos(ImVec2(toolbarX - (backgroundWidth / 2.0f), m_ViewportPanel->GetViewportBounds().y + edgeOffset));
        ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##viewport_central_toolbar", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

        const float desiredHeight = 26.0f + 5.0f;
        const ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) * 0.5f);
        // Floating HUD background: bg_elevated #221E1A at 70% alpha (matches design spec).
        ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(34, 30, 26, 178), 3.0f);

        ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        {
            UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));
            const ImColor c_ButtonTint = IM_COL32(138, 125, 114, 255); // text_secondary
            const ImColor c_SimulateButtonTint = m_SceneState == SceneState::Simulate ? ImColor(UI::Colors::Accent) : c_ButtonTint;

            auto drawButton = [buttonSize](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
                const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
                const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
                const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
                UI::DrawButtonImage(icon, tint, tint, tint, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

                return clicked;
            };

            const Ref<Texture> buttonTex = m_SceneState == SceneState::Play ? EditorAssets::Get().StopIcon : EditorAssets::Get().PlayIcon;
            if (drawButton(buttonTex, c_ButtonTint))
            {
                if (m_SceneState == SceneState::Edit)
                {
                    SaveActiveScene(); // Save to disk in case the simulation crashes

                    // Backup the editor scene and create a runtime copy
                    m_EditorSceneBackup = gSceneManager->GetActiveScene();
                    Ref<Scene> runtimeScene = CreateRef<Scene>(*m_EditorSceneBackup);
                    runtimeScene->SetEditorScene(false);
                    gSceneManager->SetActiveScene(runtimeScene);
                    m_SceneRenderer->SetScene(runtimeScene);

                    runtimeScene->OnRuntimeStart();
                    ScriptRuntime::OnStart();
                    m_SceneState = SceneState::Play;
                    m_ViewportPanel->DisableGizmo();
                }
                else if (m_SceneState != SceneState::Simulate)
                {
                    gSceneManager->GetActiveScene()->OnRuntimeStop();
                    ScriptRuntime::OnShutdown();

                    // Restore the editor scene
                    gSceneManager->SetActiveScene(m_EditorSceneBackup);
                    m_SceneRenderer->SetScene(m_EditorSceneBackup);
                    m_EditorSceneBackup = nullptr;

                    m_ViewportPanel->EnableGizmo();
                    m_SceneState = SceneState::Edit;
                    m_GameMode = false;
                    Time::Reset();
                }
            }
            UI::SetTooltip(m_SceneState == SceneState::Edit ? "Play" : "Stop");

            if (drawButton(EditorAssets::Get().PlayIcon, c_SimulateButtonTint))
            {
                if (m_SceneState == SceneState::Edit)
                {
                    // Backup the editor scene and create a simulation copy
                    m_EditorSceneBackup = gSceneManager->GetActiveScene();
                    Ref<Scene> simScene = CreateRef<Scene>(*m_EditorSceneBackup);
                    simScene->SetEditorScene(false);
                    gSceneManager->SetActiveScene(simScene);
                    m_SceneRenderer->SetScene(simScene);

                    m_SceneState = SceneState::Simulate;
                    simScene->OnSimulationStart();
                }
                else if (m_SceneState == SceneState::Simulate)
                {
                    gSceneManager->GetActiveScene()->OnSimulationEnd();

                    // Restore the editor scene
                    gSceneManager->SetActiveScene(m_EditorSceneBackup);
                    m_SceneRenderer->SetScene(m_EditorSceneBackup);
                    m_EditorSceneBackup = nullptr;

                    m_SceneState = SceneState::Edit;
                }
            }
            UI::SetTooltip(m_SceneState == SceneState::Simulate ? "Stop" : "Simulate Physics");

            if (drawButton(EditorAssets::Get().PauseIcon, c_ButtonTint))
            {
                if (m_SceneState == SceneState::Play)
                {
                    gSceneManager->GetActiveScene()->OnRuntimePause();
                    m_SceneState = SceneState::PausePlay;
                    m_ViewportPanel->EnableGizmo();
                }
                else if (m_SceneState == SceneState::PausePlay)
                {
                    gSceneManager->GetActiveScene()->OnRuntimeResume();
                    m_SceneState = SceneState::Play;
                    m_ViewportPanel->DisableGizmo();
                }
            }
            UI::SetTooltip(m_SceneState == SceneState::PausePlay ? "Resume" : "Pause");
        }
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();

        ImGui::End();

        UI::PopID();
    }

    void EditorLayer::UI_GizmoSettings()
    {
        UI::PushID();
        UI::ScopedStyle disableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        UI::ScopedStyle disableWindowBorder(ImGuiStyleVar_WindowBorderSize, 0.0f);
        UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 4.0f);
        UI::ScopedStyle disablePadding(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        const float buttonSize = 18.0f + 5.0f;
        const float edgeOffset = 4.0f;
        const float windowHeight = 32.0f;
        const float numberOfButtons = 6.0f;
        const float backgroundWidth = edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

        const float toolbarX = (m_ViewportPanel->GetViewportBounds().x + edgeOffset);
        ImGui::SetNextWindowPos(ImVec2(toolbarX, m_ViewportPanel->GetViewportBounds().y + edgeOffset));
        ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##viewport_central_toolbar2", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

        const float desiredHeight = 26.0f + 5.0f;
        const ImRect background = UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) * 0.5f);
        // Floating HUD background: bg_elevated #221E1A at 70% alpha (matches design spec).
        ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(34, 30, 26, 178), 3.0f);

        ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        {
            UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));
            const ImColor c_ButtonTint = IM_COL32(138, 125, 114, 255); // text_secondary
            const ImColor c_SimulateButtonTint = m_SceneState == SceneState::Simulate ? ImColor(UI::Colors::Accent) : c_ButtonTint;

            auto drawButton = [buttonSize](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
                const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
                const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
                const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
                UI::DrawButtonImage(icon, tint, tint, tint, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

                return clicked;
            };

            const ImColor activeColor = ImColor(UI::Colors::Accent);
            ImColor tint = m_ViewportPanel->GetGizmoMode() == GizmoEditMode::None ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().ArrowPointerIcon, tint))
                m_ViewportPanel->SetGizmoMode(GizmoEditMode::None);
            UI::SetTooltip("Normal edit mode");
            tint = m_ViewportPanel->GetGizmoMode() == GizmoEditMode::Translate ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().ArrowsIcon, tint))
                m_ViewportPanel->SetGizmoMode(GizmoEditMode::Translate);
            UI::SetTooltip("Translate mode");
            tint = m_ViewportPanel->GetGizmoMode() == GizmoEditMode::Rotate ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().RotateIcon, tint))
                m_ViewportPanel->SetGizmoMode(GizmoEditMode::Rotate);
            UI::SetTooltip("Rotate mode");
            tint = m_ViewportPanel->GetGizmoMode() == GizmoEditMode::Scale ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().MaximizeIcon, tint))
                m_ViewportPanel->SetGizmoMode(GizmoEditMode::Scale);
            UI::SetTooltip("Scale mode");

            tint = m_ViewportPanel->GetGizmoLocalMode() ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().GlobeIcon, tint))
            {
                if (m_ViewportPanel->GetGizmoLocalMode())
                    m_ViewportPanel->SetGizmoLocalMode(false);
                else
                    m_ViewportPanel->SetGizmoLocalMode(true);
            }
            UI::SetTooltip("Toggle global gizmo editing");

            tint = m_ShowViewportSettings ? activeColor : c_ButtonTint;
            if (drawButton(EditorAssets::Get().SettingsIcon, tint))
                m_ShowViewportSettings = !m_ShowViewportSettings;
            UI::SetTooltip("Viewport settings");
        }
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();

        ImGui::End();

        UI::PopID();
    }

    void EditorLayer::UI_ViewportSettings()
    {
        if (!m_ShowViewportSettings)
            return;

        UI::PushID();
        UI::ScopedStyle windowRounding(ImGuiStyleVar_WindowRounding, 6.0f);

        const float settingsWidth = 240.0f;
        const float edgeOffset = 4.0f;
        const float toolbarHeight = 32.0f;
        const float settingsX = m_ViewportPanel->GetViewportBounds().x + edgeOffset;
        const float settingsY = m_ViewportPanel->GetViewportBounds().y + edgeOffset + toolbarHeight + 4.0f;

        ImGui::SetNextWindowPos(ImVec2(settingsX, settingsY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(settingsWidth, 0.0f)); // auto height
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::Begin("##viewport_settings_popup", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoNav);

        ImGui::Spacing();

        // ── Rendering ──────────────────────────────────────────────────
        ImGui::TextDisabled("Rendering");
        ImGui::Separator();

        bool solid = !m_WireframeMode;
        if (ImGui::RadioButton("Solid", solid))
            m_WireframeMode = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Wireframe", m_WireframeMode))
            m_WireframeMode = true;

        ImGui::Spacing();

        // ── Grid ───────────────────────────────────────────────────────
        ImGui::TextDisabled("Grid");
        ImGui::Separator();

        ImGui::Checkbox("Show Grid", &m_ShowGrid);
        if (m_ShowGrid)
        {
            ImGui::Checkbox("Show Axes", &m_ShowGridAxes);

            ImGui::PushItemWidth(settingsWidth * 0.55f);
            ImGui::DragFloat("Fine Cell Size", &m_GridFineSize, 0.1f, 0.1f, 100.0f, "%.2f m");
            ImGui::DragFloat("Coarse Cell Size", &m_GridCoarseSize, 1.0f, 1.0f, 1000.0f, "%.1f m");
            ImGui::DragFloat("Line Width", &m_GridLineWidth, 0.001f, 0.005f, 0.49f, "%.3f");
            ImGui::DragFloat("Opacity", &m_GridOpacity, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
        }

        ImGui::Spacing();

        // ── Physics Gizmos ─────────────────────────────────────────────
        ImGui::TextDisabled("Physics Gizmos");
        ImGui::Separator();

        ImGui::Checkbox("Show Colliders", &m_ShowColliders);
        if (m_ShowColliders)
        {
            ImGui::PushItemWidth(settingsWidth * 0.55f);
            ImGui::ColorEdit4("Collider Color", glm::value_ptr(m_ColliderColor), ImGuiColorEditFlags_NoInputs);
            ImGui::PopItemWidth();
        }

        ImGui::Spacing();
        ImGui::End();

        UI::PopID();
    }

    extern float metalness;
    extern float roughness;
    extern glm::vec4 albedo;

    void EditorLayer::UI_Settings()
    {
        ImGui::Begin("Settings");
        ImGui::Checkbox("Show colliders", &m_ShowColliders);
        ImGui::Checkbox("Show demo window", &m_ShowDemoWindow);
        ImGui::Checkbox("Auto load last project", &m_AutoLoadLastProject);
        ImGui::Checkbox("Show C# debug info", &m_ShowScriptDebugInfo);
        ImGui::Checkbox("Show asset info", &m_ShowAssetInfo);
        ImGui::Checkbox("Show entity debug info", &m_ShowEntityDebugInfo);

        const Vector<CodeEditorInstallation>& editors = CodeEditorManager::Get().GetAvailableEditors();
        std::function<const String&(const CodeEditorInstallation&)> selector = [](const CodeEditorInstallation& install) -> const String& {
            return install.Name;
        };
        if (UI::PropertyDropdown("Visual Studio Version", editors, m_VisualStudioVersionId, selector))
        {
            const CodeEditorInstallation& selectedEditor = editors[m_VisualStudioVersionId];
            CodeEditorManager::Get().SetActive(selectedEditor.ExecutablePath);
        }

        UI::PropertyColor("Albedo", albedo);
        UI::Property("Metalness", metalness);
        UI::Property("Roughness", roughness);

        ImGui::End();
    }

    bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0)
            return false;

        const bool ctrl = Input::IsKeyPressed(Key::LeftControl);
        const bool shift = Input::IsKeyPressed(Key::LeftShift);

        switch (e.GetKeyCode())
        {
        case Key::N: {
            if (ctrl && shift)
                CreateNewScene();
            break;
        }

        case Key::O: {
            if (ctrl && shift)
                OpenScene();
            break;
        }

        case Key::R: {
            if (ctrl)
            {
                // Ref<PBRMaterial> mat = CreateRef<PBRMaterial>(Shader::Create("/Shaders/PBRShader.glsl"));
                // mat->SetAlbedoMap(Texture2D::Create("/Textures/rustediron2_basecolor.png"));
                // mat->SetMetalnessMap(Texture2D::Create("/Textures/rustediron2_metallic.png"));
                // mat->SetNormalMap(Texture2D::Create("/Textures/rustediron2_normal.png"));
                // mat->SetRoughnessMap(Texture2D::Create("/Textures/rustediron2_roughness.png"));
                // ImGuiMaterialPanel::SetSelectedMaterial(mat);
            }
            break;
        }

        case Key::S: {
            if (ctrl && !shift)
                SaveActiveScene();

            if (ctrl && shift)
                SaveActiveSceneAs();
            break;
        }
        case Key::Z: {
            if (ctrl)
                UndoRedo::Get().Undo();
            break;
        }
        case Key::Y: {
            if (ctrl)
                UndoRedo::Get().Redo();
            break;
        }
        }
        return true;
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
