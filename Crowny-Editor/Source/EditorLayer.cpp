#include "EditorLayer.h"

#include <imgui.h>

#include "Crowny/Renderer/BatchRenderer2D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Crowny/ImGui/OpenGLInformationWindow.h"
#include "Crowny/ImGui/ImGuiViewportWindow.h"
#include "Crowny/ImGui/ImGuiHierarchyWindow.h"
#include "Crowny/Ecs/Components.h"

#include "Crowny/ImGui/ImGuiTextureEditor.h"

namespace Crowny
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{
		m_ImGuiWindows.push_back(new OpenGLInformationWindow("OpenGL"));
		m_ImGuiWindows.push_back(new ImGuiHierarchyWindow("Hierarchy"));
		m_ImGuiWindows.push_back(new ImGuiViewportWindow("Viewport"));
		m_ImGuiWindows.push_back(new ImGuiTextureEditor("Texture Properties"));

		//test prb shader for errors
		Ref<Shader> s = Shader::Create("Shaders/PBRShader.glsl");
	}

	void EditorLayer::OnAttach()
	{
		FramebufferProperties fbProps;
		fbProps.Width = 1280; // human code please
		fbProps.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbProps);
		m_ComponentEditor.RegisterComponent<TransformComponent>("Transform");

		m_ComponentEditor.RegisterComponent<CameraComponent>("Camera");
		m_Entity = m_Registry.create();
		m_Registry.emplace<TransformComponent>(m_Entity);
		m_Registry.emplace<CameraComponent>(m_Entity);
	}

	void EditorLayer::OnDetach()
	{
		for (ImGuiWindow* win : m_ImGuiWindows) 
		{
			delete win;
		}
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		m_Camera.Update(ts);

		if (FramebufferProperties spec = m_Framebuffer->GetProperties(); m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&  (spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_Camera.OnResize(m_ViewportSize.x, m_ViewportSize.y);
		}

		m_Framebuffer->Bind();
		BatchRenderer2D::Begin(m_Camera);
		BatchRenderer2D::DrawString("This is a test", 150, 550, FontManager::Get("default"), Color::White);
		BatchRenderer2D::End();
		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnImGuiRender()
	{
		bool show = true;
		ImGui::ShowDemoWindow(&show);
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
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit")) Application::Get().Exit();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window"))
			{
				for(ImGuiWindow* win : m_ImGuiWindows)
					if (ImGui::MenuItem(win->GetName().c_str()))
						win->Show();
				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}

		for (ImGuiWindow* win : m_ImGuiWindows)
		{
			win->Render();
		}

		ImGui::Begin("Properties");
			
		m_ComponentEditor.Render(m_Registry, m_Entity);

		ImGui::End();

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		
	}

}