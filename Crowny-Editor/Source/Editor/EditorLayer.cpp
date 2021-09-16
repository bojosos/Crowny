#include "cwepch.h"

#include "EditorLayer.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Events/ImGuiEvent.h"
#include "Crowny/RenderAPI/RenderTexture.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Skybox.h"
#include "Crowny/Scene/SceneRenderer.h"
#include "Crowny/Scene/SceneSerializer.h"
#include "Crowny/Scene/ScriptRuntime.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptComponent.h"

#include "Editor/EditorAssets.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace Crowny
{

    float EditorLayer::s_DeltaTime = 0.0f;
    float EditorLayer::s_SmoothDeltaTime = 0.0f;
    float EditorLayer::s_RealtimeSinceStartup = 0.0f;
    float EditorLayer::s_Time = 0.0f;
    float EditorLayer::s_FixedDeltaTime = 0.0f;
    float EditorLayer::s_FrameCount = 0.0f;

    EditorCamera EditorLayer::s_EditorCamera = EditorCamera(30.0f, 1280.0f / 720.0f, 0.001f, 100000.0f);

    void LoadTexture(const std::string& filepath, Ref<Texture>& texture);

    EditorLayer::EditorLayer() : Layer("EditorLayer") {}

    void EditorLayer::OnAttach()
    {
        VirtualFileSystem::Get()->Mount("Icons", "Resources/Icons");
        SceneRenderer::Init();
        EditorAssets::Load();
        m_MenuBar = new ImGuiMenuBar();

        ImGuiMenu* fileMenu = new ImGuiMenu("File");
        fileMenu->AddItem(new ImGuiMenuItem("New", "Ctrl+N", [&](auto& event) { CreateNewScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Open", "Ctrl+O", [&](auto& event) { OpenScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Save", "Ctrl+S", [&](auto& event) { SaveActiveScene(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Save as", "Ctrl+Shift+S", [&](auto& event) { SaveActiveSceneAs(); }));
        fileMenu->AddItem(new ImGuiMenuItem("Exit", "Alt+F4", [&](auto& event) { Application::Get().Exit(); }));
        m_MenuBar->AddMenu(fileMenu);

        ImGuiMenu* viewMenu = new ImGuiMenu("View");

        m_HierarchyPanel = new ImGuiHierarchyPanel("Hierarchy");
        viewMenu->AddItem(new ImGuiMenuItem("Hierarchy", "", [&](auto& event) { m_HierarchyPanel->Show(); }));

        m_ViewportPanel = new ImGuiViewportPanel("Viewport");
        viewMenu->AddItem(new ImGuiMenuItem("Viewport", "", [&](auto& event) { m_ViewportPanel->Show(); }));
        m_ViewportPanel->SetEventCallback(CW_BIND_EVENT_FN(OnViewportEvent));

        m_InspectorPanel = new ImGuiInspectorPanel("Inspector");
        viewMenu->AddItem(new ImGuiMenuItem("Inspector", "", [&](auto& event) { m_InspectorPanel->Show(); }));

        m_ConsolePanel = new ImGuiConsolePanel("Console");
        viewMenu->AddItem(new ImGuiMenuItem("Console", "", [&](auto& entity) { m_ConsolePanel->Show(); }));

        m_AssetBrowser = new ImGuiAssetBrowserPanel("Asset browser");
        viewMenu->AddItem(new ImGuiMenuItem("Asset browser", "", [&](auto& event) { m_AssetBrowser->Show(); }));

        m_MenuBar->AddMenu(viewMenu);

        SceneManager::AddScene(CreateRef<Scene>("Editor scene"));
        ScriptRuntime::Init();
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile("/Shaders/pbribl.vert", VERTEX_SHADER));
        Ref<Shader> fragment = Shader::Create(compiler.Compile("/Shaders/pbribl.frag", FRAGMENT_SHADER));
        Ref<PBRMaterial> mat = CreateRef<PBRMaterial>(vertex, fragment);
        /// auto& manifest = AssetManager::Get().ImportManifest("Sandbox.yaml", "Sandbox");
        // AssetManifest("Sandbox").Serialize("Sandbox.yaml");

        Ref<Texture> albedo, metallic, roughness, normal;
        LoadTexture("/Textures/rustediron2_basecolor.png", albedo);
        LoadTexture("/Textures/rustediron2_metallic.png", metallic);
        LoadTexture("/Textures/rustediron2_roughness.png", roughness);
        LoadTexture("/Textures/rustediron2_normal.png", normal);

        Ref<Texture> ao = Texture::WHITE;

        mat->SetAlbedoMap(albedo);
        mat->SetNormalMap(normal);
        mat->SetMetalnessMap(metallic);
        mat->SetRoughnessMap(roughness);
        mat->SetAoMap(ao);

        ImGuiInspectorPanel::SetSelectedMaterial(mat);
        ForwardRenderer::Init(); // Why here?

        // Ref<Scene> scene;
        // SceneSerializer serializer(scene);
        // serializer.Deserialize("Resources/Scenes/Test.yaml");
        // SceneManager::SetActiveScene(scene);

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

    void EditorLayer::CreateNewScene()
    {
        Ref<Scene> tmp = CreateRef<Scene>();
        tmp->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
        SceneManager::SetActiveScene(tmp);
        ScriptComponent::s_EntityComponents.clear();
    }

    void EditorLayer::OpenScene()
    {
        std::vector<std::string> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::OpenFile, "", "", outPaths))
        {
            OpenScene(outPaths[0]);
        }
    }

    void EditorLayer::OpenScene(const std::string& filepath)
    {
        m_Temp = CreateRef<Scene>();
        SceneSerializer serializer(m_Temp);
        serializer.Deserialize(filepath);
        ScriptComponent::s_EntityComponents.clear();
    }

    void EditorLayer::SaveActiveSceneAs()
    {
        std::vector<std::string> outPaths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, "", "", outPaths))
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
    }

    void EditorLayer::OnUpdate(Timestep ts)
    {
        Ref<Scene> scene = SceneManager::GetActiveScene();
        m_ViewportSize = m_ViewportPanel->GetViewportSize();
        if (m_Temp)
        {
            SceneManager::SetActiveScene(m_Temp);
            m_Temp = nullptr;
        }
        s_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
        s_EditorCamera.OnUpdate(ts);
        SceneRenderer::SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);

        auto& rapi = RenderAPI::Get();
        rapi.SetRenderTarget(m_RenderTarget);
        rapi.SetViewport(0, 0, m_ViewportSize.x, m_ViewportSize.y);
        SceneRenderer::OnEditorUpdate(ts, s_EditorCamera);

        if (m_GameMode && !m_Paused)
        {
            ScriptRuntime::OnUpdate();
            s_FrameCount += 1;
            s_DeltaTime = ts;
            s_Time += ts;
            s_RealtimeSinceStartup += ts;
            s_SmoothDeltaTime = s_DeltaTime + s_Time / (s_FrameCount + 1);
        }
        /*
        glm::vec4 bounds = m_ViewportPanel->GetViewportBounds();
        ImVec2 mouseCoords = ImGui::GetMousePos();
        glm::vec2 coords = { mouseCoords.x - bounds.x, mouseCoords.y - bounds.y };
        coords.y = m_ViewportSize.y - coords.y;
        if (coords.x >= 0 && coords.x < m_ViewportSize.x && coords.y >= 0 && coords.y < m_ViewportSize.y)
        {
            m_HoveredEntity = Entity((entt::entity)IDBufferRenderer::ReadPixel(coords.x, coords.y), scene.get());
        }*/
        m_HierarchyPanel->Update();
    }

    void EditorLayer::OnImGuiRender()
    {
        ImGui::ShowDemoWindow();
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
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 43.5f);
        ImTextureID textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().PlayIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
        {
            if (!m_GameMode)
            {
                ScriptRuntime::OnStart();
                m_GameMode = true;
            }
        }
        ImGui::SameLine(0.0f, 8.0f);
        textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().PauseIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
        {
            if (m_GameMode)
            {
                m_Paused = !m_Paused;
            }
        }
        ImGui::SameLine(0.0f, 8.0f);
        textureID = ImGui_ImplVulkan_AddTexture(EditorAssets::Get().StopIcon);
        if (ImGui::ImageButton(textureID, ImVec2(25.0f, 25.0f), ImVec2(0, 0), ImVec2(1, 1), -1,
                               ImVec4(1.0f, 1.0f, 1.0f, 0.0f), ImVec4(0.1f, 0.105f, 0.11f, 1.0f)))
        {
            if (m_GameMode)
            {
                ScriptRuntime::OnShutdown();
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
        // m_GLInfoPanel->Render();
        m_ViewportPanel->SetEditorRenderTarget(m_RenderTarget);
        m_ViewportPanel->Render();
        m_ConsolePanel->Render();
        m_AssetBrowser->Render();

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
            if (ctrl)
                CreateNewScene();
            break;
        }

        case Key::O: {
            if (ctrl)
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
            ImGuiHierarchyPanel::SetSelectedEntity(m_HoveredEntity);
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
