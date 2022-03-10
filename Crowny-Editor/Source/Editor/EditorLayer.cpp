#include "cwepch.h"

#include "EditorLayer.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/SceneSerializer.h"
#include "Crowny/Scene/ScriptRuntime.h"

#include "Editor/Editor.h"
#include "Editor/EditorAssets.h"
#include "Editor/ProjectLibrary.h"

#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Math/ScriptMath.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"
#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/ScriptObjectManager.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    EditorCamera EditorLayer::s_EditorCamera = EditorCamera(30.0f, 1280.0f / 720.0f, 0.001f, 100000.0f);

    EditorLayer::EditorLayer() : Layer("EditorLayer") {}

    void EditorLayer::OnAttach()
    {
        // Well constructors get discarded and the static data is gone, so construct a few empty objects
        ScriptTime();
        ScriptInput();
        ScriptDebug();
        ScriptRandom();
        ScriptNoise();
        ScriptMath();
        ProjectLibrary::StartUp();
        Editor::StartUp();
        VirtualFileSystem::Get()->Mount("Icons", "Resources/Icons");
        SceneRenderer::Init();
        EditorAssets::Load();
        m_MenuBar = new ImGuiMenuBar();

        ImGuiMenu* fileMenu = new ImGuiMenu("File");
        fileMenu->AddItem(new ImGuiMenuItem("New Project", "", [&](auto& event) { NewProject(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Open Project", "", [&](auto& event) { OpenProject(); }));
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
        m_AssetBrowser = new AssetBrowserPanel("Asset browser", [&](const Path& path) { m_InspectorPanel->SetSelectedAssetPath(path); });
        
        m_ViewportPanel->RegisterInMenu(viewMenu);
        m_InspectorPanel->RegisterInMenu(viewMenu);
        m_HierarchyPanel->RegisterInMenu(viewMenu);
        m_ConsolePanel->RegisterInMenu(viewMenu);
        m_AssetBrowser->RegisterInMenu(viewMenu);

        m_MenuBar->AddMenu(viewMenu);

        Editor::Get().LoadProject("C:/dev/New Project");
        // ProjectLibrary::Get().Refresh(ProjectLibrary::Get().GetAssetFolder());
        m_AssetBrowser->Initialize();

        SceneManager::AddScene(CreateRef<Scene>("Editor scene"));
        ScriptRuntime::Init();

        Ref<Shader> shader = Importer::Get().Import<Shader>("Resources/Shaders/Pbribl.glsl");
        Ref<PBRMaterial> mat = CreateRef<PBRMaterial>(shader);

        Ref<Texture> albedo, metallic, roughness, normal;
        albedo = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_basecolor.png");
        metallic = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_metallic.png");
        roughness = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_roughness.png");
        normal = Importer::Get().Import<Texture>("Resources/Textures/rustediron2_normal.png");

        Ref<Texture> ao = Texture::WHITE;

        mat->SetAlbedoMap(albedo);
        mat->SetNormalMap(normal);
        mat->SetMetalnessMap(metallic);
        mat->SetRoughnessMap(roughness);
        mat->SetAoMap(ao);

        InspectorPanel::SetSelectedMaterial(mat);
        ForwardRenderer::Init(); // Why here?

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

    void EditorLayer::NewProject()
    {
        ImGui::CloseCurrentPopup();
        ImGui::OpenPopup("New Project");
    }

    void EditorLayer::OpenProject()
    {
        ImGui::CloseCurrentPopup();
        ImGui::OpenPopup("Project Manager");
    }

    void EditorLayer::CreateNewScene()
    {
        Ref<Scene> tmp = CreateRef<Scene>();
        tmp->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
        SceneManager::SetActiveScene(tmp);
    }

    void EditorLayer::OpenScene()
    {
        Vector<Path> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, ProjectLibrary::Get().GetAssetFolder(), "", outPaths))
        {
            OpenScene(outPaths[0]);
        }
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
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, ProjectLibrary::Get().GetAssetFolder(), "", outPaths))
        {
            SceneSerializer serializer(SceneManager::GetActiveScene());
            serializer.Serialize(outPaths[0]);
        }
    }

    void EditorLayer::SaveActiveScene()
    {
        const auto& scene = SceneManager::GetActiveScene();
        SceneSerializer serializer(scene);
        serializer.Serialize(scene->GetFilepath());
    }

    void EditorLayer::OnDetach()
    {
        delete m_InspectorPanel;
        delete m_HierarchyPanel;
        delete m_ViewportPanel;
        delete m_ConsolePanel;
        delete m_AssetBrowser;

        EditorAssets::Unload();
        Editor::Get().SaveProject();
    }

    void EditorLayer::OnUpdate(Timestep ts)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();
        auto& rapi = RenderAPI::Get();
        if (m_ViewportPanel->IsShown() && (m_ViewportSize.x != m_ViewportPanel->GetViewportSize().x ||
            m_ViewportSize.y != m_ViewportPanel->GetViewportSize().y)) // TODO: Move out
        {
            SceneManager().GetActiveScene()->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
            TextureParameters colorParams;
            colorParams.Width = m_ViewportPanel->GetViewportSize().x;
            colorParams.Height = m_ViewportPanel->GetViewportSize().y;
            colorParams.Usage = TextureUsage::TEXTURE_RENDERTARGET;

            TextureParameters objectId;
            objectId.Width = m_ViewportPanel->GetViewportSize().x;
            objectId.Height = m_ViewportPanel->GetViewportSize().y;
            objectId.Format = TextureFormat::R32I;
            objectId.Usage = TextureUsage(TextureUsage::TEXTURE_RENDERTARGET | TextureUsage::TEXTURE_DYNAMIC);

            TextureParameters depthParams;
            depthParams.Width = m_ViewportPanel->GetViewportSize().x;
            depthParams.Height = m_ViewportPanel->GetViewportSize().y;
            depthParams.Usage = TextureUsage::TEXTURE_DEPTHSTENCIL;
            depthParams.Format = TextureFormat::DEPTH24STENCIL8;

            Ref<Texture> color1 = Texture::Create(colorParams);
            Ref<Texture> color2 = Texture::Create(objectId);
            Ref<Texture> depth = Texture::Create(depthParams);
            RenderTextureProperties rtProps;
            rtProps.ColorSurfaces[0] = { color1 };
            rtProps.ColorSurfaces[1] = { color2 };
            rtProps.DepthSurface = { depth };
            rtProps.Width = m_ViewportPanel->GetViewportSize().x;
            rtProps.Height = m_ViewportPanel->GetViewportSize().y;
            m_RenderTarget = RenderTexture::Create(rtProps);
        }
        m_ViewportSize = m_ViewportPanel->GetViewportSize();
        if (m_Temp) // Delay scene reload
        {
            SceneManager::SetActiveScene(m_Temp);
            m_Temp = nullptr;
        }
        SceneRenderer::SetViewportSize(m_RenderTarget->GetProperties().Width, m_RenderTarget->GetProperties().Height);

        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0, 0, m_RenderTarget->GetProperties().Width, m_RenderTarget->GetProperties().Height);

        switch (m_SceneState)
        {
        case SceneState::Edit: {
            s_EditorCamera.SetViewportSize(m_RenderTarget->GetProperties().Width, m_RenderTarget->GetProperties().Height);
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
        }
        RenderOverlay();

        glm::vec4 bounds = m_ViewportPanel->GetViewportBounds();
        ImVec2 mouseCoords = ImGui::GetMousePos();
        glm::vec2 coords = { mouseCoords.x - bounds.x, mouseCoords.y - bounds.y };
        coords.y = m_ViewportSize.y - coords.y;
        if (coords.x >= 0 && coords.x < m_ViewportSize.x && coords.y >= 0 && coords.y < m_ViewportSize.y)
        {
            // RenderTexture* rt = static_cast<RenderTexture*>(m_RenderTarget.get());
            // PixelData outPixelData;
            // rt->GetColorTexture(1)->ReadData(outPixelData);
            // glm::vec4 col = outPixelData.GetColorAt(coords.x, coords.y);
            // CW_ENGINE_INFO("Hover: {0}, {1}, {2}, {3}, {4}", col.x, col.y, col.z, col.w);
            // m_HoveredEntity = Entity((entt::entity)
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
            Renderer2D::Begin(camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetTransform());
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
                    glm::vec3 translation = tc.Position + glm::vec3(bc2d.Offset, zOffset);
                    glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);
                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) * glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(glm::mat4(1.0f), scale);
                    Renderer2D::DrawRect(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 0.01);
                }
            }

            {
                auto view = scene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
                for (auto entity : view)
                {
                    auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);
                    glm::vec3 translation = tc.Position + glm::vec3(cc2d.Offset, zOffset);
                    glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);
                    glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) * glm::scale(glm::mat4(1.0f), scale);
                    Renderer2D::DrawCircle(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 0.01);
                }
            }
        }
        Renderer2D::End();
    }

    void EditorLayer::OnImGuiRender()
    {
        if (m_ShowDemoWindow)
            ImGui::ShowDemoWindow(&m_ShowDemoWindow);
        if (!Editor::Get().IsProjectLoaded() && !ImGui::IsPopupOpen("New Project"))
        {
            if (!ImGui::IsPopupOpen("Project Manager"))
                ImGui::OpenPopup("Project Manager");
            if (ImGui::BeginPopupModal("Project Manager"))
            {
                ImGui::Text("Recent Projects");
                for (uint32_t i = 0; i < 5; i++)
                    ImGui::Selectable(("Project_" + std::to_string(i)).c_str());
                bool shouldEnd = true;
                if (ImGui::Button("Open"))
                {
                    shouldEnd = false;
                    ImGui::EndPopup();
                    Vector<Path> outPaths;
                    if (FileSystem::OpenFileDialog(FileDialogType::OpenFolder, "/home/life/Desktop/dev", String(),
                                                   outPaths))
                    {
                        if (outPaths.size() > 0)
                        {
                            Editor::Get().LoadProject(outPaths[0]);
                            m_AssetBrowser->Initialize();
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("New"))
                {
                    shouldEnd = false;
                    ImGui::EndPopup();
                    NewProject();
                }

                if (shouldEnd)
                    ImGui::EndPopup();
            }
            return;
        }
        if (ImGui::BeginPopupModal("New Project"))
        {
            ImGui::Text("Path: ");
            ImGui::SameLine();
            if (m_NewProjectPath.empty())
            {
                m_NewProjectPath = "/home/life/Desktop/dev/";
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
                Editor::Get().LoadProject(Path(m_NewProjectPath) / m_NewProjectName);
                m_NewProjectPath.clear();
                ImGui::CloseCurrentPopup();
                m_AssetBrowser->Initialize();
            }
            ImGui::EndPopup();
            return;
        }

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

        m_MenuBar->Render();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() * 0.5f - 43.5f);
        ImTextureID textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().PlayIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
        {
            if (m_SceneState == SceneState::Edit)
            {
                SceneManager::GetActiveScene()->OnRuntimeStart();
                ScriptRuntime::OnStart();
                m_SceneState = SceneState::Play;
                m_ViewportPanel->DisalbeGizmo();
            }
        }
        ImGui::SameLine(0.0f, 8.0f);
        textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().PauseIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
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
        ImGui::SameLine(0.0f, 8.0f);
        textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().StopIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
        {
            if (m_SceneState == SceneState::Play)
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
        ImGui::Separator();

        // DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("Crowny Editor");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        m_HierarchyPanel->Render();
        m_InspectorPanel->Render();
        m_ViewportPanel->SetEditorRenderTarget(m_RenderTarget);
        m_ViewportPanel->Render();
        m_ConsolePanel->Render();
        m_AssetBrowser->Render();

        ImGui::Begin("Settings");
        ImGui::Checkbox("Show colliders", &m_ShowColliders);
        ImGui::Checkbox("Show demo window", &m_ShowDemoWindow);
        ImGui::End();

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

    bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
    {
        if (m_ViewportPanel->IsHovered() && m_ViewportPanel->IsFocused())
        {
            HierarchyPanel::SetSelectedEntity(m_HoveredEntity);
            return true;
        }

        return false;
    }

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
