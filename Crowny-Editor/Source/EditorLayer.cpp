#include "EditorLayer.h"

#include <imgui.h>

#include "Crowny/Renderer/BatchRenderer2D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Platform/OpenGL/OpenGLInfo.h"
#include "Crowny/Ecs/Components.h"

namespace Crowny
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_Camera(glm::ortho(0.0f, 1280.0f, 720.0f, 0.0f)) // camera not for here
	{
		m_ImGuiWindows["viewport"] = true;
		m_ImGuiWindows["hierarchy"] = true;
		m_ImGuiWindows["glinfo"] = false;
		m_ImGuiWindows["properties"] = true;
		Ref<Shader> s = Shader::Create("Shaders/PBRShader.glsl");
	}

	void EditorLayer::OnAttach()
	{
		FramebufferProperties fbProps;
		fbProps.Width = 1280; // human code please
		fbProps.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbProps);
		OpenGLInfo::RetrieveInformation();
		m_ComponentEditor.RegisterComponent<Components::Transform>("Transform");

		m_ComponentEditor.RegisterComponent<Components::Camera>("Camera");
		m_Entity = m_Registry.create();
		m_Registry.emplace<Components::Transform>(m_Entity, 500.f, 500.f);
		m_Registry.emplace<Components::Camera>(m_Entity, 500.f);
	}

	void EditorLayer::OnDetach()
	{
		
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
				if(ImGui::BeginMenu("Renderer Capabilites"))
				{
					if (ImGui::MenuItem("OpenGL")) {
						m_ImGuiWindows["glinfo"] = true;
					}
					ImGui::EndMenu();
				}
				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 0, 0 });

		if (m_ImGuiWindows["viewport"]) {
			ImGui::Begin("Viewport");
			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };
			uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			ImGui::End();
		}

		if (m_ImGuiWindows["glinfo"])
		{
			ImGui::Begin("OpenGL Information");
			ImGui::Columns(3, "OpenGL Information");
			ImGui::Separator();
			for (OpenGLDetail& det : OpenGLInfo::GetInformation()) 
			{
				ImGui::Text(det.Name.c_str()); ImGui::NextColumn();
				ImGui::Text(det.GLName.c_str()); ImGui::NextColumn();
				ImGui::Text(det.Value.c_str()); ImGui::NextColumn();
			}
			ImGui::Separator();
			ImGui::Columns(1);
			ImGui::End();
		}

		if (m_ImGuiWindows["hierarchy"])
		{
			ImGui::Begin("Hierarchy");
			ImGui::SetNextItemOpen(true, ImGuiCond_Once);
			if (ImGui::TreeNode("Scene"))
			{
				if (ImGui::TreeNode("Entity"))
				{
					for (int i = 0; i < 5; i++)
					{
						if (ImGui::TreeNode((void*)(intptr_t)i, "Child %d", i))
						{
							ImGui::Text("");
							ImGui::SameLine();
							ImGui::Selectable("test", false);
							if (ImGui::IsItemClicked()) CW_INFO("Clicked");
							ImGui::TreePop();
						}
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::End();
		}

		if (m_ImGuiWindows["properties"])
		{
			ImGui::Begin("Properties");
			
			m_ComponentEditor.Render(m_Registry, m_Entity);

			ImGui::End();
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		
	}

}