#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/ImGui/ImGuiOpenGLLayer.h"
#include "Crowny/Input/Input.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.cpp>
#include <imgui.h>

namespace Crowny
{

    ImGuiOpenGLLayer::ImGuiOpenGLLayer() : ImGuiLayer() {}

    void ImGuiOpenGLLayer::OnAttach()
    {
        ImGuiLayer::OnAttach();
        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void ImGuiOpenGLLayer::OnDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGuiLayer::OnDetach();
    }

    void ImGuiOpenGLLayer::Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGuiLayer::Begin();
    }

    void ImGuiOpenGLLayer::End()
    {
        ImGuiLayer::End();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

} // namespace Crowny
