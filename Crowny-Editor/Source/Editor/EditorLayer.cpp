#include "cwepch.h"

#include <mono/metadata/object.h> // TODO: Implement array class

#include "EditorLayer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Serialization/SceneSerializer.h"

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
#include "Crowny/Scripting/Bindings/Utils/ScriptJson.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptLayerMask.h"
#include "Crowny/Scripting/ScriptInfoManager.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/fmt/fmt.h>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Crowny
{

    float EditorLayer::s_DeltaTime = 0.0f;
    float EditorLayer::s_SmoothDeltaTime = 0.0f;
    float EditorLayer::s_RealtimeSinceStartup = 0.0f;
    float EditorLayer::s_Time = 0.0f;
    float EditorLayer::s_FixedDeltaTime = 0.0f;
    float EditorLayer::s_FrameCount = 0.0f;

    static const char* EDITOR_NEW_PROJECT_ID = "New Project";
    static const char* EDITOR_PROJECT_MANAGER_ID = "Project Manager";

    EditorCamera EditorLayer::s_EditorCamera = EditorCamera(30.0f, 1280.0f / 720.0f, 0.001f, 1000.0f);

    EditorLayer::EditorLayer() : Layer("EditorLayer") {}

    void EditorLayer::OnAttach()
    {
        // Well constructors get discarded and the static data is gone, so construct a few empty objects
        ScriptTime tempTime = ScriptTime();
        ScriptMath tempMath = ScriptMath();
        ScriptInput tempInput = ScriptInput();
        ScriptDebug tempDebug = ScriptDebug();
        ScriptNoise tempNoise = ScriptNoise();
        ScriptRandom tempRandom = ScriptRandom();
        ScriptLayerMask tempLayerMask = ScriptLayerMask();
        ScriptJson tempJson = ScriptJson();
        // ScriptCompression tempCompression = ScriptCompression();

        EditorAssets::Load();

        Editor::StartUp();

        m_MenuBar = new ImGuiMenuBar();

        ImGuiMenu* fileMenu = new ImGuiMenu("File");
        fileMenu->AddItem(new ImGuiMenuItem("New Project", "", [&](auto& event) { m_NewProject = true; }));
        fileMenu->AddItem(new ImGuiMenuItem("Open Project", "", [&](auto& event) { m_OpenProject = true; }));
        fileMenu->AddItem(new ImGuiMenuItem("Save Project", "", [&](auto& event) { Editor::Get().SaveProject(); }));

        fileMenu->AddItem(new ImGuiMenuItem("New Scene", "Ctrl+Shift+N", [&](auto& event) { CreateNewScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Open Scene", "Ctrl+Shift+O", [&](auto& event) { OpenScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Save Scene", "Ctrl+S", [&](auto& event) { SaveActiveScene(); }));
        fileMenu->AddItem(
          new ImGuiMenuItem("Save Scene as", "Ctrl+Shift+S", [&](auto& event) { SaveActiveSceneAs(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Exit", "Alt+F4", [&](auto& event) { Application::Get().Exit(); }));
        m_MenuBar->AddMenu(fileMenu);

        ImGuiMenu* viewMenu = new ImGuiMenu("View");

        // Has to be done before hierarchy and asset browser panels
        m_InspectorPanel = new InspectorPanel("Inspector");
        m_HierarchyPanel = new HierarchyPanel("Hierarchy", [&](Entity e) { m_InspectorPanel->SetSelectedEntity(e); });
        m_ViewportPanel = new ViewportPanel("Viewport");
        m_ViewportPanel->SetEventCallback(CW_BIND_EVENT_FN(OnViewportEvent));
        m_ConsolePanel = new ConsolePanel("Console");
        m_AssetBrowser = new AssetBrowserPanel("Asset browser",
                                               [&](const Path& path) { m_InspectorPanel->SetSelectedAssetPath(path); });

        m_ViewportPanel->RegisterInMenu(viewMenu);
        m_InspectorPanel->RegisterInMenu(viewMenu);
        m_HierarchyPanel->RegisterInMenu(viewMenu);
        m_ConsolePanel->RegisterInMenu(viewMenu);
        m_AssetBrowser->RegisterInMenu(viewMenu);

        ImGuiMenu* buildMenu = new ImGuiMenu("Build");
        buildMenu->AddItem(
          new ImGuiMenuItem("Rebuild game assembly", "Ctrl+Shift+B", CW_BIND_EVENT_FN(RebuildAssemblies)));
        buildMenu->AddItem(new ImGuiMenuItem("Build game", "Ctrl+B", CW_BIND_EVENT_FN(BuildGame)));

        m_MenuBar->AddMenu(buildMenu);
        m_MenuBar->AddMenu(viewMenu);

        Ref<EditorSettings> editorSettings = Editor::Get().GetEditorSettings();
        m_ShowDemoWindow = editorSettings->ShowImGuiDemoWindow;
        m_ShowColliders = editorSettings->ShowPhysicsColliders2D;
        m_AutoLoadLastProject = editorSettings->AutoLoadLastProject;
        m_ShowScriptDebugInfo = editorSettings->ShowScriptDebugInfo;
        m_ShowEntityDebugInfo = editorSettings->ShowEntityDebugInfo;

        m_ConsolePanel->SetMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Info,
                                               editorSettings->EnableConsoleInfoMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Warn,
                                               editorSettings->EnableConsoleWarningMessages);
        m_ConsolePanel->SetMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Error,
                                               editorSettings->EnableConsoleErrorMessages);

        m_ConsolePanel->SetCollapseEnabled(editorSettings->CollapseConsole);
        m_ConsolePanel->SetScrollToBottomEnabled(editorSettings->ScrollToBottom);

        if (m_AutoLoadLastProject && !editorSettings->LastOpenProject.empty())
        {
            Editor::Get().LoadProject(editorSettings->LastOpenProject);
            SetProjectSettings();
            m_AssetBrowser->Initialize();
        }

        VirtualFileSystem::Get()->Mount("Icons", "Resources/Icons");
        SceneRenderer::Init();

        // Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Pbribl.glsl");
        AssetHandle<Shader> shader = AssetManager::Get().Load<Shader>("Resources/Shaders/Pbribl.asset");
        Ref<PBRMaterial> mat = CreateRef<PBRMaterial>(shader);

        // Ref<Texture> albedo, metallic, roughness, normal;
        // albedo = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_basecolor.png");
        // metallic = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_metallic.png");
        // roughness = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_roughness.png");
        // normal = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_normal.png");

        Ref<Texture> ao = Texture::WHITE;

        // mat->SetAlbedoMap(albedo);
        // mat->SetNormalMap(normal);
        // mat->SetMetalnessMap(metallic);
        // mat->SetRoughnessMap(roughness);
        // mat->SetAoMap(ao);

        InspectorPanel::SetSelectedMaterial(mat);
        // ForwardRenderer::Init(); // Why here?

        TextureParameters colorParams;
        colorParams.Width = 1337;
        colorParams.Height = 509;
        colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;

        TextureParameters objectId;
        objectId.Width = 1337;
        objectId.Height = 509;
        objectId.Format = TextureFormat::R32I;
        objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);

        TextureParameters depthParams;
        depthParams.Width = 1337;
        depthParams.Height = 509;
        depthParams.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
        depthParams.Format = TextureFormat::DEPTH24STENCIL8;

        Ref<Texture> color1 = Texture::Create(colorParams);
        Ref<Texture> color2 = Texture::Create(objectId);
        Ref<Texture> depth = Texture::Create(depthParams);
        RenderTextureProperties rtProps;
        rtProps.ColorSurfaces[0] = { color1 };
        rtProps.ColorSurfaces[1] = { color2 };
        rtProps.DepthSurface = { depth };
        rtProps.Width = 1337;
        rtProps.Height = 509;

        m_RenderTarget = RenderTexture::Create(rtProps);
        if (m_Temp == nullptr) // No scene was auto-loaded
            m_Temp = CreateRef<Scene>("Scene");
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
            Ref<Scene> scene = CreateRef<Scene>();
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
        const Ref<Scene>& activeScene = SceneManager::GetActiveScene();
        if (fs::is_regular_file(activeScene->GetFilepath()))
            projSettings->LastOpenScenePath = activeScene->GetFilepath().string();
        projSettings->LastAssetBrowserSelectedEntry = m_AssetBrowser->GetCurrentEntryPath();

        projSettings->GizmoMode = m_ViewportPanel->GetGizmoMode();
        projSettings->GizmoLocalMode = m_ViewportPanel->GetGizmoLocalMode();

        if (HierarchyPanel::GetSelectedEntity())
            projSettings->LastSelectedEntityID = HierarchyPanel::GetSelectedEntity().GetUuid();
        projSettings->ExpandedEntities = m_HierarchyPanel->GetSerializableHierarchy();
    }

    void EditorLayer::BuildGame(Event& event) {}

    void EditorLayer::RebuildAssemblies(Event& event)
    {
        MonoClass* scriptCompiler = ScriptInfoManager::Get().GetBuiltinClasses().ScriptCompiler;
        uint32_t type = 0;
        bool debug = true;
        MonoArray* libDirs = mono_array_new(MonoManager::Get().GetDomain(), MonoUtils::GetStringClass(), 1);
        mono_array_setref(libDirs, 0, MonoUtils::ToMonoString("C:/dev/Crowny/Crowny-Sharp/"));
        MonoArray* refs = mono_array_new(MonoManager::Get().GetDomain(), MonoUtils::GetStringClass(), 1);
        mono_array_setref(refs, 0, MonoUtils::ToMonoString("CrownySharp.dll"));

        void* params[6] = { &type,
                            &debug,
                            MonoUtils::ToMonoString((Editor::Get().GetProjectPath() / INTERNAL_ASSEMBLY_PATH).string()),
                            MonoUtils::ToMonoString(ProjectLibrary::Get().GetAssetFolder().string()),
                            libDirs,
                            refs };
        scriptCompiler->GetMethod("Compile", 6)->Invoke(nullptr, params);
        Vector<AssemblyRefreshInfo> refreshInfos;
        Path engineAssemblyPath = "C:/dev/Crowny/Crowny-Sharp/CrownySharp.dll";
        Path gameAssemblyPath = Editor::Get().GetProjectPath() / INTERNAL_ASSEMBLY_PATH / "GameAssembly.dll";
        CW_ENGINE_INFO("{0}, {1}", gameAssemblyPath, Editor::Get().GetProjectPath());
        refreshInfos.emplace_back(CROWNY_ASSEMBLY, &engineAssemblyPath);
        refreshInfos.emplace_back(GAME_ASSEMBLY, &gameAssemblyPath);
        ScriptObjectManager::Get().RefreshAssemblies(refreshInfos);

        // ScriptInfoManager::Get().InitializeTypes();
        // ScriptInfoManager::Get().LoadAssemblyInfo(GAME_ASSEMBLY);
        // ScriptInfoManager::Get().LoadAssemblyInfo(CROWNY_ASSEMBLY);
        /*
        auto view = SceneManager::GetActiveScene()->GetAllEntitiesWith<MonoScriptComponent>();
        for (auto e : view)
        {
            Entity entity = { e, SceneManager::GetActiveScene().get() };
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
        dispatcher.Dispatch<ImGuiViewportSceneDraggedEvent>([this](ImGuiViewportSceneDraggedEvent& event) {
            OpenScene(event.GetSceneFilepath());
            return true;
        });
        return true;
    }

    void EditorLayer::CreateNewScene() { m_Temp = CreateRef<Scene>("Scene"); }

    void EditorLayer::OpenScene()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }, outPaths))
            OpenScene(outPaths[0].replace_extension(".cwscene"));
    }

    void EditorLayer::OpenScene(const Path& filepath)
    {
        m_Temp = CreateRef<Scene>();
        SceneSerializer serializer(m_Temp);
        serializer.Deserialize(filepath);
    }

    void EditorLayer::SaveActiveSceneAs()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, ProjectLibrary::Get().GetAssetFolder(),
                                       { Editor::GetSceneDialogFilter() }, outPaths))
        {
            SceneSerializer serializer(SceneManager::GetActiveScene());
            serializer.Serialize(outPaths[0].replace_extension(".cwscene"));
        }
    }

    void EditorLayer::SaveActiveScene()
    {
        const auto& scene = SceneManager::GetActiveScene();
        SceneSerializer serializer(scene);
        if (scene->GetFilepath().empty())
            SaveActiveSceneAs();
        else
            serializer.Serialize(scene->GetFilepath());
    }

    void EditorLayer::AddRecentEntry(const Path& path)
    {
        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        int recentIdx = settings->RecentProjects.size() - 1;
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
        SceneRenderer::Shutdown();
        InspectorPanel::SetSelectedMaterial(nullptr);
        Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
        settings->ShowImGuiDemoWindow = m_ShowDemoWindow;
        settings->ShowPhysicsColliders2D = m_ShowColliders;
        settings->AutoLoadLastProject = m_AutoLoadLastProject;
        settings->ShowEntityDebugInfo = m_ShowEntityDebugInfo;
        settings->ShowAssetInfo = m_ShowAssetInfo;
        settings->ShowScriptDebugInfo = m_ShowScriptDebugInfo;
        settings->EnableConsoleInfoMessages =
          m_ConsolePanel->IsMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Info);
        settings->EnableConsoleWarningMessages =
          m_ConsolePanel->IsMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Warn);
        settings->EnableConsoleErrorMessages =
          m_ConsolePanel->IsMessageLevelEnabled(ImGuiConsoleBuffer::Message::Level::Error);

        settings->CollapseConsole = m_ConsolePanel->IsCollapseEnabled();
        settings->ScrollToBottom = m_ConsolePanel->IsScrollToBottomEnabled();

        EditorAssets::Unload();
        Editor::Get().SaveProject();

        AddRecentEntry(Editor::Get().GetProjectPath());
        SaveProjectSettings();
        SaveActiveScene();
        Editor::Shutdown();

        delete m_InspectorPanel;
        delete m_HierarchyPanel;
        delete m_ViewportPanel;
        delete m_ConsolePanel;
        delete m_AssetBrowser;
    }

    void EditorLayer::OnUpdate(Timestep ts)
    {
        if (m_Temp) // Delay scene reload
        {
            SceneManager::SetActiveScene(m_Temp);
            Editor::Get().GetProjectSettings()->LastOpenScenePath = m_Temp->GetFilepath().string();
            m_Temp = nullptr;
            // ScriptRuntime::Init();
        }

        Ref<Scene> scene = SceneManager::GetActiveScene();
        auto& rapi = RenderAPI::Get();
        if (m_ViewportPanel->IsShown() && (m_ViewportSize.x != m_ViewportPanel->GetViewportSize().x ||
                                           m_ViewportSize.y != m_ViewportPanel->GetViewportSize().y)) // TODO: Move out
        {
            scene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
            TextureParameters colorParams;
            colorParams.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            colorParams.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;

            TextureParameters objectId;
            objectId.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            objectId.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            objectId.Format = TextureFormat::R32I;
            objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);

            TextureParameters depthParams;
            depthParams.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            depthParams.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            depthParams.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            depthParams.Format = TextureFormat::DEPTH24STENCIL8;

            Ref<Texture> color1 = Texture::Create(colorParams);
            Ref<Texture> color2 = Texture::Create(objectId);
            Ref<Texture> depth = Texture::Create(depthParams);
            RenderTextureProperties rtProps;
            rtProps.ColorSurfaces[0] = { color1 };
            rtProps.ColorSurfaces[1] = { color2 };
            rtProps.DepthSurface = { depth };
            rtProps.Width = (uint32_t)m_ViewportPanel->GetViewportSize().x;
            rtProps.Height = (uint32_t)m_ViewportPanel->GetViewportSize().y;
            m_RenderTarget = RenderTexture::Create(rtProps);
        }
        m_ViewportSize = m_ViewportPanel->GetViewportSize();
        SceneRenderer::SetViewportSize(m_RenderTarget->GetProperties().Width,
                                       m_RenderTarget->GetProperties().Height); // Comment:

        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        rapi.ClearRenderTarget(FBT_COLOR | FBT_DEPTH);

        switch (m_SceneState)
        {
        case SceneState::Edit: {
            s_EditorCamera.SetViewportSize(m_RenderTarget->GetProperties().Width,
                                           m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);

            SceneManager::GetActiveScene()->OnUpdateEditor(ts);
            SceneRenderer::OnEditorUpdate(ts, s_EditorCamera);
            break;
        }
        case SceneState::Play: {
            SceneManager().GetActiveScene()->OnUpdateRuntime(ts);
            ScriptRuntime::OnUpdate();
            SceneRenderer::OnRuntimeUpdate(ts);
            s_FrameCount += 1;
            s_DeltaTime = ts;
            s_Time += ts;
            s_RealtimeSinceStartup += ts;
            s_SmoothDeltaTime = s_DeltaTime + s_Time / (s_FrameCount + 1);
            break;
        }
        case SceneState::Simulate: {
            s_EditorCamera.SetViewportSize(m_RenderTarget->GetProperties().Width,
                                           m_RenderTarget->GetProperties().Height);
            s_EditorCamera.OnUpdate(ts);
            SceneManager().GetActiveScene()->OnSimulationUpdate(ts);
            SceneRenderer::OnEditorUpdate(ts, s_EditorCamera);
            break;
        }
        }
        RenderOverlay();

        const glm::vec4& bounds = m_ViewportPanel->GetViewportBounds();
        ImVec2 mouseCoords = ImGui::GetMousePos();
        glm::vec2 coords = { mouseCoords.x - bounds.x, mouseCoords.y - bounds.y };
        coords.y = m_ViewportSize.y - coords.y;
        if (m_ViewportPanel->IsHovered())
        {
            RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
            Ref<PixelData> outPixelData =
              PixelData::Create(rt->GetColorTexture(1)->GetWidth(), rt->GetColorTexture(1)->GetHeight(),
                                rt->GetColorTexture(1)->GetFormat());
            rt->GetColorTexture(1)->ReadData(*outPixelData);
            if (outPixelData->GetSize() > coords.x * coords.y)
            {
                glm::vec4 col = outPixelData->GetColorAt(coords.x, coords.y);
                if (col.x == 0.0f)
                    m_HoveredEntity = Entity(entt::null, scene.get());
                else
                {
                    m_HoveredEntity = Entity((entt::entity)(col.x - 1), scene.get());
                    if (Input::IsMouseButtonDown(Mouse::ButtonLeft) && !Input::IsKeyPressed(Key::LeftAlt) &&
                        !Input::IsKeyPressed(Key::RightAlt) && !m_ViewportPanel->IsMouseOverGizmo())
                        m_HierarchyPanel->SetSelectedEntity(m_HoveredEntity);
                }
            }
        }
        m_HierarchyPanel->Update();
        ScriptObjectManager::Get().Update();
    }

    void EditorLayer::RenderOverlay()
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();
        if (m_SceneState == SceneState::Play)
        {
            Entity camera = scene->GetPrimaryCameraEntity();
            if (!camera)
                return;
            Renderer2D::Begin(camera.GetComponent<CameraComponent>().Camera,
                              camera.GetComponent<TransformComponent>().GetTransform());
        }
        else
            Renderer2D::Begin(s_EditorCamera, s_EditorCamera.GetViewMatrix());

        if (m_ShowColliders)
        {
            float zOffset = s_EditorCamera.GetPosition().z > 0 ? 0.001f : -0.001f;
            {
                auto view = scene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
                for (auto entity : view)
                {
                    auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);
                    glm::vec3 translation = tc.Position + glm::vec3(bc2d.GetOffset(), zOffset);
                    glm::vec3 scale = tc.Scale * glm::vec3(bc2d.GetSize() * 2.0f, 1.0f);
                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) *
                                          glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)) *
                                          glm::scale(glm::mat4(1.0f), scale);
                    Renderer2D::DrawRect(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 0.01f);
                }
            }

            {
                auto view = scene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
                for (auto entity : view)
                {
                    auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);
                    glm::vec3 translation = tc.Position + glm::vec3(cc2d.GetOffset(), zOffset);
                    glm::vec3 scale = tc.Scale * glm::vec3(cc2d.GetRadius() * 2.0f);
                    glm::mat4 transform =
                      glm::translate(glm::mat4(1.0f), translation) * glm::scale(glm::mat4(1.0f), scale);
                    Renderer2D::DrawCircle(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 0.01f);
                }
            }
        }
        Renderer2D::End();
    }

    void EditorLayer::OnImGuiRender()
    {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
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
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Crowny Editor", &dockspaceOpen, window_flags);
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

        m_MenuBar->Render();

        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);
        if (!Editor::Get().IsProjectLoaded() && !ImGui::IsPopupOpen(EDITOR_PROJECT_MANAGER_ID) || m_OpenProject)
        {
            ImGui::OpenPopup(EDITOR_PROJECT_MANAGER_ID);
            m_OpenProject = false;
        }

        if (UIUtils::BeginPopup(EDITOR_PROJECT_MANAGER_ID))
        {
            if (ImGui::Button("Open"))
            {
                Vector<Path> outPaths;
                if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, Editor::Get().GetDefaultProjectPath(), {},
                                               outPaths))
                {
                    if (outPaths.size() > 0)
                    {
                        if (Editor::Get().IsProjectLoaded())
                            SaveProjectSettings();
                        Editor::Get().LoadProject(outPaths[0]);
                        Editor::Get().GetEditorSettings()->LastOpenProject = outPaths[0];
                        SetProjectSettings();
                        m_AssetBrowser->Initialize();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("New"))
            {
                ImGui::CloseCurrentPopup();
                m_NewProject = true;
                // return;
            }

            ImGui::Text("Recent Projects");

            ImGui::BeginTable("##projectTable", 4);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort,
                                    0.15f);
            ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort,
                                    0.35f);
            ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthStretch, 0.3f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.1f);
            ImGui::TableHeadersRow();
            Ref<EditorSettings> settings = Editor::Get().GetEditorSettings();
            for (uint32_t i = 0; i < settings->RecentProjects.size(); i++)
            {
                const RecentProject& project = settings->RecentProjects[i];
                if (project.ProjectPath.empty())
                    continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char res[30];
                tm* timeinfo;
                timeinfo = localtime(&project.Timestamp);
                strftime(res, 30, "%c", timeinfo);
                const Path& projectPath = project.ProjectPath;
                if (ImGui::Selectable(projectPath.filename().string().c_str(), false,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap))
                {
                    if (Editor::Get().IsProjectLoaded())
                        SaveProjectSettings();
                    Editor::Get().LoadProject(project.ProjectPath);
                    Editor::Get().GetEditorSettings()->LastOpenProject = project.ProjectPath;
                    SetProjectSettings();
                    m_AssetBrowser->Initialize();
                }
                ImGui::TableNextColumn();
                ImGui::Text(projectPath.string().c_str());
                ImGui::TableNextColumn();
                ImGui::Text(res);
                ImGui::TableNextColumn();
                if (ImGui::Button("-", ImVec2(20.0f, 20.0f)))
                {
                    for (uint32_t j = i; j < settings->RecentProjects.size() - 1; j++)
                        settings->RecentProjects[j] = settings->RecentProjects[j + 1];
                    settings->RecentProjects[settings->RecentProjects.size() - 1].ProjectPath.clear();
                    settings->RecentProjects[settings->RecentProjects.size() - 1].Timestamp = 0;
                }
                ImGui::SameLine();
                if (ImGui::ImageButton(ImGui_ImplVulkan_AddTexture(EditorAssets::Get().FolderIcon),
                                       ImVec2(20.0f, 20.0f), { 0, 1 }, { 1, 0 }, 0.0f))
                    PlatformUtils::ShowInExplorer(project.ProjectPath);
            }
            ImGui::EndTable();
            UIUtils::EndPopup();
            if (!Editor::Get().IsProjectLoaded())
                return;
        }
        if (m_NewProject)
        {
            ImGui::OpenPopup(EDITOR_NEW_PROJECT_ID);
            m_NewProject = false;
        }
        if (UIUtils::BeginPopup(EDITOR_NEW_PROJECT_ID))
        {
            ImGui::Text("Path: ");
            ImGui::SameLine();
            if (m_NewProjectPath.empty())
            {
                m_NewProjectPath = Editor::Get().GetDefaultProjectPath().string();
                m_NewProjectName = "New Project";
            }
            ImGui::InputText("##newProjectPath", &m_NewProjectPath);
            ImGui::Text("ProjectName: ");
            ImGui::InputText("##newProjectName", &m_NewProjectName);
            if (!fs::exists(m_NewProjectPath))
                ImGui::Text("* Path does not exist");
            else if (fs::exists(Path(m_NewProjectPath) / m_NewProjectName))
                ImGui::Text("* A folder with the name of the project already exists there.");

            if (ImGui::Button("Create"))
            {
                Editor::Get().CreateProject(m_NewProjectPath, m_NewProjectName);
                Path newProjectPath = Path(m_NewProjectPath) / m_NewProjectName;
                if (Editor::Get().IsProjectLoaded())
                    SaveProjectSettings();
                Editor::Get().LoadProject(newProjectPath);
                Editor::Get().GetEditorSettings()->LastOpenProject = newProjectPath;
                SetProjectSettings();
                m_NewProjectPath.clear();
                ImGui::CloseCurrentPopup();
                m_AssetBrowser->Initialize();
            }
            UIUtils::EndPopup();
            if (!Editor::Get()
                   .IsProjectLoaded()) // TODO: Consider changing this (fixing panels) as it would look better
                return;
        }

        UI_Header();
        UI_GizmoSettings();
        UI_Settings();
        UI_Physics2DSettings();

#ifdef CW_DEBUG
        UI_ScriptInfo();
        UI_AssetInfo();
        UI_EntityDebugInfo();
        Physics2D::Get().UIStats();
#endif

        m_HierarchyPanel->Render();
        m_InspectorPanel->Render();
        m_ViewportPanel->SetEditorRenderTarget(m_RenderTarget);
        m_ViewportPanel->Render();
        m_ConsolePanel->Render();
        m_AssetBrowser->Render();

        ImGui::End();
    }

    void EditorLayer::UI_EntityDebugInfo()
    {
        if (m_ShowEntityDebugInfo)
        {
            ImGui::Begin("Entity Debug Info", &m_ShowEntityDebugInfo);
            Scene* scene = SceneManager::GetActiveScene().get();
            auto view = scene->GetAllEntitiesWith<TagComponent>();
            for (auto e : view)
            {
                Entity entity = Entity(e, scene);
                const String label = entity.GetName() + ": " + entity.GetUuid().ToString();
                if (ImGui::TreeNode(label.c_str()))
                {
                    if (entity.GetParent())
                    {
                        const String parentLabel =
                          entity.GetParent().GetName() + ": " + entity.GetParent().GetUuid().ToString();
                        ImGui::Text(parentLabel.c_str());
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
            ImGui::Columns(2);
            ImGui::Text("Project Library");
            ImGui::NextColumn();
            ImGui::Text("Show empty metadata entries");
            ImGui::SameLine();
            ImGui::Checkbox("##showEmptyMetadata", &m_ShowEmptyMetadataAssetInfo);
            ImGui::NextColumn();

            std::function<void(const Ref<LibraryEntry>&)> traverse = [&](const Ref<LibraryEntry>& entry) {
                if (entry->Type == LibraryEntryType::Directory)
                {
                    for (auto& child : std::static_pointer_cast<DirectoryEntry>(entry)->Children)
                        traverse(child);
                }
                else
                {
                    FileEntry* file = static_cast<FileEntry*>(entry.get());

                    if (!m_ShowEmptyMetadataAssetInfo && file->Metadata == nullptr)
                        return;
                    ImGui::Text(file->Filepath.string().c_str());
                    ImGui::NextColumn();
                    if (file->Metadata != nullptr)
                        ImGui::Text(file->Metadata->Uuid.ToString().c_str());
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
            glyph = font->FindGlyph(c);
            if (!glyph)
                continue;

            DrawList->PrimReserve(6, 4);
            DrawList->PrimQuadUV(pos + ImVec2(glyph->Y0, -glyph->X0), pos + ImVec2(glyph->Y0, -glyph->X1),
                                 pos + ImVec2(glyph->Y1, -glyph->X1), pos + ImVec2(glyph->Y1, -glyph->X0),

                                 ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V0),
                                 ImVec2(glyph->U1, glyph->V1), ImVec2(glyph->U0, glyph->V1), text_color);
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
            glm::vec2 gravity = Physics2D::Get().GetGravity();
            if (UI::Property("Gravity", gravity))
                Physics2D::Get().SetGravity(gravity);

            uint32_t velocityIterations = Physics2D::Get().GetVelocityIterations();
            if (UI::Property("Velocity iterations", velocityIterations))
                Physics2D::Get().SetVelocityIterations(velocityIterations);

            uint32_t positionIterations = Physics2D::Get().GetPositionIterations();
            if (UI::Property("Position iterations", positionIterations))
                Physics2D::Get().SetPositionIterations(positionIterations);
            UI::EndPropertyGrid();
            if (ImGui::CollapsingHeader("Layer Names"))
            {
                UI::BeginPropertyGrid();
                uint32_t lastNonEmptyIdx = 0;
                for (uint32_t i = 31; i >= 0; i--)
                {
                    if (!Physics2D::Get().GetLayerName(i).empty())
                    {
                        lastNonEmptyIdx = i;
                        break;
                    }
                }
                for (uint32_t i = 0; i < 32; i++)
                {
                    if (i > lastNonEmptyIdx + 1 &&
                        !UI::IsItemDisabled()) // Give the user exactly one non-disabled layer field
                        ImGui::BeginDisabled(true);
                    String layerName = Physics2D::Get().GetLayerName(i);
                    if (UI::Property(fmt::format("Layer {0}", i).c_str(), layerName))
                        Physics2D::Get().SetLayerName(i, layerName);
                }
                if (UI::IsItemDisabled())
                    ImGui::EndDisabled();
                UI::EndPropertyGrid();
            }

            if (ImGui::CollapsingHeader("Collision Matrix"))
            {
                UI::PushID();
                uint32_t id = 0;
                uint32_t nonEmpty = 0;
                uint32_t maxTextLength = 0;
                for (uint32_t i = 0; i < 32; i++)
                {
                    maxTextLength = std::max(maxTextLength,
                                             (uint32_t)ImGui::CalcTextSize(Physics2D::Get().GetLayerName(i).c_str()).x);
                    nonEmpty += !Physics2D::Get().GetLayerName(i).empty();
                }
                nonEmpty--;
                UI::ShiftCursorY(maxTextLength);
                const ImVec2 text_pos(ImGui::GetCurrentWindow()->DC.CursorPos.x,
                                      ImGui::GetCurrentWindow()->DC.CursorPos.y - 2.0f);
                uint32_t ii = 0;
                for (uint32_t i = 0; i < 32; i++) // rows
                {
                    uint32_t categoryMask = Physics2D::Get().GetCategoryMask(i);
                    if (Physics2D::Get().GetLayerName(i).empty())
                        continue;
                    ii++;
                    UI::ShiftCursorX(10);
                    ImGui::Text(Physics2D::Get().GetLayerName(i).c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(maxTextLength + ImGui::GetStyle().WindowPadding.x + 2 + 10);
                    uint32_t jj = 0;
                    for (uint32_t j = 0; j < 32; j++) // cols
                    {
                        if (ii + jj > nonEmpty + 1 || Physics2D::Get().GetLayerName(j).empty())
                            continue;
                        jj++;
                        if (ii == 1)
                        {
                            AddTextVertical(ImGui::GetWindowDrawList(), Physics2D::Get().GetLayerName(j).c_str(),
                                            text_pos + ImVec2(ImGui::GetCursorPosX() - 6.0f, 0),
                                            IM_COL32(192, 192, 192, 255));
                        }
                        bool value = (categoryMask & (1 << j)) != 0;
                        ImGui::PushID(id++);
                        if (ImGui::Checkbox("##checkbox", &value))
                        {
                            if (value)
                                Physics2D::Get().SetCategoryMask(i, categoryMask | (1 << j));
                            else
                                Physics2D::Get().SetCategoryMask(i, categoryMask & (~(1 << j)));
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
                if (ImGui::TreeNode(klass->GetName().c_str()))
                {
                    if (ImGui::TreeNode("Methods"))
                    {
                        for (MonoMethod* method : klass->GetMethods())
                            ImGui::Text(method->GetName().c_str());
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Fields"))
                    {
                        for (MonoField* field : klass->GetFields())
                        {
                            ImGui::Text(field->GetFullDeclName().c_str());
                            ImGui::SameLine();
                            ImGui::Text(field->GetType()->GetFullName().c_str());
                            ImGui::SameLine();
                            ImGui::Text(field->GetName().c_str());
                        }
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Properties"))
                    {
                        for (MonoProperty* prop : klass->GetProperties())
                            ImGui::Text(prop->GetName().c_str());
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
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
        const float backgroundWidth =
          edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

        float toolbarX = (m_ViewportPanel->GetViewportBounds().x + m_ViewportPanel->GetViewportBounds().z) / 2.0f;
        ImGui::SetNextWindowPos(
          ImVec2(toolbarX - (backgroundWidth / 2.0f), m_ViewportPanel->GetViewportBounds().y + edgeOffset));
        ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##viewport_central_toolbar", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

        const float desiredHeight = 26.0f + 5.0f;
        ImRect background =
          UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) * 0.5f);
        ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

        ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        {
            UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));
            const ImColor c_ButtonTint = IM_COL32(192, 192, 192, 255);
            const ImColor c_SimulateButtonTint =
              m_SceneState == SceneState::Simulate ? ImColor(39, 185, 242, 255) : c_ButtonTint;

            auto drawButton = [buttonSize](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
                const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
                const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
                const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
                UI::DrawButtonImage(icon, tint, tint, tint, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

                return clicked;
            };

            Ref<Texture> buttonTex =
              m_SceneState == SceneState::Play ? EditorAssets::Get().StopIcon : EditorAssets::Get().PlayIcon;
            if (drawButton(buttonTex, c_ButtonTint))
            {
                if (m_SceneState == SceneState::Edit)
                {
                    SaveActiveScene(); // Save the scene in case the simulation crashes
                    SceneManager::GetActiveScene()->OnRuntimeStart();
                    ScriptRuntime::OnStart();
                    m_SceneState = SceneState::Play;
                    m_ViewportPanel->DisalbeGizmo();
                }
                else if (m_SceneState != SceneState::Simulate)
                {
                    SceneManager::GetActiveScene()->OnRuntimeStop();
                    ScriptRuntime::OnShutdown();
                    m_ViewportPanel->EnableGizmo();
                    m_SceneState = SceneState::Edit;
                    m_GameMode = false;
                    s_DeltaTime = 0.0f;
                    s_SmoothDeltaTime = 0.0f;
                    s_RealtimeSinceStartup = 0.0f;
                    s_Time = 0.0f;
                    s_FixedDeltaTime = 0.0f;
                    s_FrameCount = 0.0f;
                }
            }
            UI::SetTooltip(m_SceneState == SceneState::Edit ? "Play" : "Stop");

            if (drawButton(EditorAssets::Get().PlayIcon, c_SimulateButtonTint))
            {
                if (m_SceneState == SceneState::Edit)
                {
                    m_SceneState = SceneState::Simulate;
                    SceneManager::GetActiveScene()->OnSimulationStart();
                }
                else if (m_SceneState == SceneState::Simulate)
                {
                    m_SceneState = SceneState::Edit;
                    SceneManager::GetActiveScene()->OnSimulationEnd();
                }
            }
            UI::SetTooltip(m_SceneState == SceneState::Simulate ? "Stop" : "Simulate Physics");

            if (drawButton(EditorAssets::Get().PauseIcon, c_ButtonTint))
            {
                if (m_SceneState == SceneState::Play)
                {
                    SceneManager::GetActiveScene()->OnRuntimePause();
                    m_SceneState = SceneState::PausePlay;
                    m_ViewportPanel->EnableGizmo();
                }
                else if (m_SceneState == SceneState::PausePlay)
                {
                    m_SceneState = SceneState::Play;
                    m_ViewportPanel->DisalbeGizmo();
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
        const float numberOfButtons = 5.0f;
        const float backgroundWidth =
          edgeOffset * 6.0f + buttonSize * numberOfButtons + edgeOffset * (numberOfButtons - 1.0f) * 2.0f;

        float toolbarX = (m_ViewportPanel->GetViewportBounds().x + edgeOffset);
        ImGui::SetNextWindowPos(ImVec2(toolbarX, m_ViewportPanel->GetViewportBounds().y + edgeOffset));
        ImGui::SetNextWindowSize(ImVec2(backgroundWidth, windowHeight));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##viewport_central_toolbar2", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking);

        const float desiredHeight = 26.0f + 5.0f;
        ImRect background =
          UI::RectExpanded(ImGui::GetCurrentWindow()->Rect(), 0.0f, -(windowHeight - desiredHeight) * 0.5f);
        ImGui::GetWindowDrawList()->AddRectFilled(background.Min, background.Max, IM_COL32(15, 15, 15, 127), 4.0f);

        ImGui::BeginVertical("##viewport_central_toolbarV", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        ImGui::BeginHorizontal("##viewport_central_toolbarH", { backgroundWidth, ImGui::GetContentRegionAvail().y });
        ImGui::Spring();
        {
            UI::ScopedStyle enableSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(edgeOffset * 2.0f, 0));
            const ImColor c_ButtonTint = IM_COL32(192, 192, 192, 255);
            const ImColor c_SimulateButtonTint =
              m_SceneState == SceneState::Simulate ? ImColor(1.0f, 0.25f, 0.75f, 1.0f) : c_ButtonTint;

            auto drawButton = [buttonSize](const Ref<Texture>& icon, const ImColor& tint, float paddingY = 0.0f) {
                const float height = std::min((float)icon->GetHeight(), buttonSize) - paddingY * 2.0f;
                const float width = (float)icon->GetWidth() / (float)icon->GetHeight() * height;
                const bool clicked = ImGui::InvisibleButton(UI::GenerateID(), ImVec2(width, height));
                UI::DrawButtonImage(icon, tint, tint, tint, UI::RectOffset(UI::GetItemRect(), 0.0f, paddingY));

                return clicked;
            };

            const ImColor activeColor = ImColor(39, 185, 242, 255);
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
        }
        ImGui::Spring();
        ImGui::EndHorizontal();
        ImGui::Spring();
        ImGui::EndVertical();

        ImGui::End();

        UI::PopID();
    }

    void EditorLayer::UI_Settings()
    {
        ImGui::Begin("Settings");
        ImGui::Checkbox("Show colliders", &m_ShowColliders);
        ImGui::Checkbox("Show demo window", &m_ShowDemoWindow);
        ImGui::Checkbox("Auto load last project", &m_AutoLoadLastProject);
        ImGui::Checkbox("Show C# debug info", &m_ShowScriptDebugInfo);
        ImGui::Checkbox("Show asset info", &m_ShowAssetInfo);
        ImGui::Checkbox("Show entity debug info", &m_ShowEntityDebugInfo);
        ImGui::End();
    }

    bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
    {
        if (e.GetRepeatCount() > 0)
            return false;

        bool ctrl = Input::IsKeyPressed(Key::LeftControl);
        bool shift = Input::IsKeyPressed(Key::LeftShift);

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

    float Time::GetTime() { return EditorLayer::s_Time; }
    float Time::GetDeltaTime() { return EditorLayer::s_DeltaTime; }
    float Time::GetFrameCount() { return EditorLayer::s_FrameCount; }
    float Time::GetFixedDeltaTime() { return EditorLayer::s_FixedDeltaTime; }
    float Time::GetRealtimeSinceStartup() { return EditorLayer::s_RealtimeSinceStartup; }
    float Time::GetSmoothDeltaTime() { return EditorLayer::s_SmoothDeltaTime; }

} // namespace Crowny
