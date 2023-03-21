#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/ImGui/ImGuiLayer.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>

#include <ImGuizmo.h>
#include <imgui.h>

namespace Crowny
{

    ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

    void ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.MouseDoubleClickTime = 0.15f;
        io.MouseDoubleClickMaxDist = 6.0f;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        io.FontDefault = io.Fonts->AddFontFromFileTTF("Resources/Fonts/Roboto/Roboto-Regular.ttf", 17.0f, nullptr,
                                                      io.Fonts->GetGlyphRangesCyrillic());
        io.Fonts->AddFontFromFileTTF("Resources/Fonts/Roboto/Roboto-Bold.ttf", 17.0f, nullptr,
                                     io.Fonts->GetGlyphRangesCyrillic());

        io.KeyMap[ImGuiKey_Tab] = Key::Tab; // Keyboard mapping. ImGui will use those indices to peek into the
                                          // io.KeyDown[] array that we will update during the application lifetime.
        io.KeyMap[ImGuiKey_LeftArrow] = Key::Left;
        io.KeyMap[ImGuiKey_RightArrow] = Key::Right;
        io.KeyMap[ImGuiKey_UpArrow] = Key::Up;
        io.KeyMap[ImGuiKey_DownArrow] = Key::Down;
        io.KeyMap[ImGuiKey_PageUp] = Key::PageUp;
        io.KeyMap[ImGuiKey_PageDown] = Key::PageDown;
        io.KeyMap[ImGuiKey_Home] = Key::Home;
        io.KeyMap[ImGuiKey_End] = Key::End;
        io.KeyMap[ImGuiKey_Delete] = Key::Delete;
        io.KeyMap[ImGuiKey_Backspace] = Key::Backspace;
        io.KeyMap[ImGuiKey_Enter] = Key::Enter;
        io.KeyMap[ImGuiKey_Escape] = Key::Escape;
        io.KeyMap[ImGuiKey_A] = Key::A;
        io.KeyMap[ImGuiKey_C] = Key::C;
        io.KeyMap[ImGuiKey_V] = Key::V;
        io.KeyMap[ImGuiKey_X] = Key::X;
        io.KeyMap[ImGuiKey_Y] = Key::Y;
        io.KeyMap[ImGuiKey_Z] = Key::Z;

        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ColorButtonPosition = ImGuiDir_Left;

        style.FrameRounding = 2.5f;
        style.FrameBorderSize = 1.0f;
        style.IndentSpacing = 11.0f;

        auto& colors = ImGui::GetStyle().Colors;

        // Headers
        colors[ImGuiCol_Header] = ImGui::ColorConvertU32ToFloat4(IM_COL32(47, 47, 47, 255));
        colors[ImGuiCol_HeaderHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(47, 47, 47, 255));
        colors[ImGuiCol_HeaderActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(47, 47, 47, 255));

        // Buttons
        colors[ImGuiCol_Button] = ImGui::ColorConvertU32ToFloat4(IM_COL32(56, 56, 56, 200));
        colors[ImGuiCol_ButtonHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(70, 70, 70, 255));
        colors[ImGuiCol_ButtonActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(56, 56, 56, 150));

        // Frame BG
        colors[ImGuiCol_FrameBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(15, 15, 15, 255));
        colors[ImGuiCol_FrameBgHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(15, 15, 15, 255));
        colors[ImGuiCol_FrameBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(15, 15, 15, 255));

        // Tabs
        colors[ImGuiCol_Tab] = ImGui::ColorConvertU32ToFloat4(IM_COL32(21, 21, 21, 255));
        colors[ImGuiCol_TabHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(255, 225, 135, 30));
        colors[ImGuiCol_TabActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(255, 225, 135, 60));
        colors[ImGuiCol_TabUnfocused] = ImGui::ColorConvertU32ToFloat4(IM_COL32(21, 21, 21, 255));
        colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabHovered];

        // Title
        colors[ImGuiCol_TitleBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(21, 21, 21, 255));
        colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(21, 21, 21, 255));
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

        // Resize Grip
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.0f);

        // Check Mark
        colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(IM_COL32(200, 200, 200, 255));

        // Slider
        colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 0.7f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.66f, 0.66f, 0.66f, 1.0f);

        // Text
        colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(IM_COL32(192, 192, 192, 255));

        // Checkbox
        colors[ImGuiCol_CheckMark] = ImGui::ColorConvertU32ToFloat4(IM_COL32(192, 192, 192, 255));

        // Separator
        colors[ImGuiCol_Separator] = ImGui::ColorConvertU32ToFloat4(IM_COL32(26, 26, 26, 255));
        colors[ImGuiCol_SeparatorActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(39, 185, 242, 255));
        colors[ImGuiCol_SeparatorHovered] = ImGui::ColorConvertU32ToFloat4(IM_COL32(39, 185, 242, 150));

        // Window Background
        colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(21, 21, 21, 255));
        colors[ImGuiCol_ChildBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(36, 36, 36, 255));
        colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(63, 70, 77, 255));
        colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(IM_COL32(26, 26, 26, 255));

        // Tables
        colors[ImGuiCol_TableHeaderBg] = ImGui::ColorConvertU32ToFloat4(IM_COL32(47, 47, 47, 255));
        colors[ImGuiCol_TableBorderLight] = ImGui::ColorConvertU32ToFloat4(IM_COL32(26, 26, 26, 255));

        // Menubar
        colors[ImGuiCol_MenuBarBg] = ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f };
        // colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, style.Colors[ImGuiCol_WindowBg].w);

        //========================================================
        /// Style
        style.FrameRounding = 2.5f;
        style.FrameBorderSize = 1.0f;
        style.IndentSpacing = 11.0f;
    }

    void ImGuiLayer::OnDetach()
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::OnEvent(Event& e)
    {
        if (m_BlockEvents)
        {
            ImGuiIO& io = ImGui::GetIO();
            e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
            e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
        }
    }

    void ImGuiLayer::Begin()
    {
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::End()
    {
        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::Get();
        io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

        ImGui::Render();
    }

} // namespace Crowny
