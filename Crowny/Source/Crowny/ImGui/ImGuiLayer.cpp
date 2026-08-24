#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/ImGui/ImGuiLayer.h"
#include "Crowny/Window/Window.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>

#include <ImGuizmo.h>
#include <imgui.h>

namespace Crowny
{
    namespace
    {
        ImFont* LoadBuiltInImGuiFont(ImFontAtlas& atlas, const Path& path, float size, const ImWchar* glyphRanges)
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            if (!stream)
                return nullptr;

            const Vector<uint8_t> bytes = stream->ReadAll();
            if (bytes.empty() || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
                return nullptr;

            void* fontData = IM_ALLOC(bytes.size());
            if (fontData == nullptr)
                return nullptr;
            std::memcpy(fontData, bytes.data(), bytes.size());
            return atlas.AddFontFromMemoryTTF(fontData, static_cast<int>(bytes.size()), size, nullptr, glyphRanges);
        }
    } // namespace

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

        // Load fonts before applying style so font metrics are stable.
        io.FontDefault = LoadBuiltInImGuiFont(*io.Fonts, "Resources/Fonts/Roboto/Roboto-Regular.ttf", 17.0f,
                                              io.Fonts->GetGlyphRangesCyrillic());
        LoadBuiltInImGuiFont(*io.Fonts, "Resources/Fonts/Roboto/Roboto-Bold.ttf", 17.0f, io.Fonts->GetGlyphRangesCyrillic());

        ApplyCrownyDarkTheme();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ColorButtonPosition = ImGuiDir_Left;
    }

    // Crowny editor skin — single-amber-accent dark theme.
    // Tokens, paddings, and color slots correspond 1:1 to the design handoff spec.
    void ImGuiLayer::ApplyCrownyDarkTheme()
    {
        ImGuiStyle& s = ImGui::GetStyle();

        // spacing & sizing
        s.WindowPadding     = ImVec2(10, 10);
        s.FramePadding      = ImVec2(6,  4);
        s.ItemSpacing       = ImVec2(6,  5);
        s.ItemInnerSpacing  = ImVec2(4,  4);
        s.IndentSpacing     = 14.0f;
        s.ScrollbarSize     = 10.0f;
        s.GrabMinSize       = 8.0f;

        // border widths
        s.WindowBorderSize        = 0.0f;
        s.ChildBorderSize         = 0.0f;
        s.FrameBorderSize         = 1.0f;
        s.TabBorderSize           = 0.0f;
        s.PopupBorderSize         = 1.0f;
        s.SeparatorTextBorderSize = 1.0f;

        // corner radii
        s.WindowRounding    = 0.0f;
        s.ChildRounding     = 0.0f;
        s.FrameRounding     = 3.0f;
        s.ScrollbarRounding = 2.0f;
        s.GrabRounding      = 2.0f;
        s.TabRounding       = 3.0f;
        s.PopupRounding     = 3.0f;

        // alignment
        s.WindowTitleAlign    = ImVec2(0.00f, 0.50f);
        s.ButtonTextAlign     = ImVec2(0.50f, 0.50f);
        s.SelectableTextAlign = ImVec2(0.00f, 0.50f);

        ImVec4* const c = s.Colors;
        c[ImGuiCol_Text]                  = ImVec4(0.910f, 0.867f, 0.816f, 1.00f);
        c[ImGuiCol_TextDisabled]          = ImVec4(0.290f, 0.251f, 0.227f, 1.00f);
        c[ImGuiCol_WindowBg]              = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_ChildBg]               = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_PopupBg]               = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_Border]                = ImVec4(0.173f, 0.149f, 0.133f, 1.00f);
        c[ImGuiCol_BorderShadow]          = ImVec4(0.00f,  0.00f,  0.00f,  0.00f);
        c[ImGuiCol_FrameBg]               = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_FrameBgHovered]        = ImVec4(0.180f, 0.157f, 0.125f, 1.00f);
        c[ImGuiCol_FrameBgActive]         = ImVec4(0.227f, 0.196f, 0.157f, 1.00f);
        c[ImGuiCol_TitleBg]               = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_TitleBgActive]         = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_MenuBarBg]             = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_ScrollbarBg]           = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.180f, 0.157f, 0.125f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.227f, 0.196f, 0.157f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_CheckMark]             = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_SliderGrab]            = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_SliderGrabActive]      = ImVec4(0.843f, 0.541f, 0.227f, 1.00f);
        c[ImGuiCol_Button]                = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_ButtonHovered]         = ImVec4(0.843f, 0.541f, 0.227f, 1.00f);
        c[ImGuiCol_ButtonActive]          = ImVec4(0.690f, 0.420f, 0.157f, 1.00f);
        c[ImGuiCol_Header]                = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_HeaderHovered]         = ImVec4(0.180f, 0.157f, 0.125f, 1.00f);
        c[ImGuiCol_HeaderActive]          = ImVec4(0.227f, 0.196f, 0.157f, 1.00f);
        c[ImGuiCol_Separator]             = ImVec4(0.173f, 0.149f, 0.133f, 1.00f);
        c[ImGuiCol_SeparatorHovered]      = ImVec4(0.769f, 0.482f, 0.188f, 0.78f);
        c[ImGuiCol_SeparatorActive]       = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_ResizeGrip]            = ImVec4(0.00f,  0.00f,  0.00f,  0.00f);
        c[ImGuiCol_ResizeGripHovered]     = ImVec4(0.769f, 0.482f, 0.188f, 0.50f);
        c[ImGuiCol_ResizeGripActive]      = ImVec4(0.769f, 0.482f, 0.188f, 0.80f);
        c[ImGuiCol_Tab]                   = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_TabHovered]            = ImVec4(0.180f, 0.157f, 0.125f, 1.00f);
        c[ImGuiCol_TabSelected]           = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        // Engine has no SubmitWindowTab hook to draw a manual underline, so use
        // ImGui's built-in overline slot for the active-tab accent indicator.
        c[ImGuiCol_TabSelectedOverline]   = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_TabDimmed]             = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_TabDimmedSelected]     = ImVec4(0.102f, 0.090f, 0.078f, 1.00f);
        c[ImGuiCol_DockingPreview]        = ImVec4(0.769f, 0.482f, 0.188f, 0.40f);
        c[ImGuiCol_DockingEmptyBg]        = ImVec4(0.078f, 0.067f, 0.058f, 1.00f);
        c[ImGuiCol_PlotLines]             = ImVec4(0.541f, 0.490f, 0.447f, 1.00f);
        c[ImGuiCol_PlotLinesHovered]      = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_PlotHistogram]         = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_TableHeaderBg]         = ImVec4(0.133f, 0.118f, 0.102f, 1.00f);
        c[ImGuiCol_TableBorderLight]      = ImVec4(0.173f, 0.149f, 0.133f, 1.00f);
        c[ImGuiCol_TextSelectedBg]        = ImVec4(0.769f, 0.482f, 0.188f, 0.35f);
        c[ImGuiCol_DragDropTarget]        = ImVec4(0.769f, 0.482f, 0.188f, 0.90f);
        c[ImGuiCol_NavCursor]             = ImVec4(0.769f, 0.482f, 0.188f, 1.00f);
        c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f,  0.00f,  0.00f,  0.55f);
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
            const ImGuiIO& io = ImGui::GetIO();
            e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
            // Allow Ctrl+key shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+S, …) to reach the
            // EditorLayer even when an ImGui panel (e.g. the node editor) has keyboard
            // focus. Without this, keyboard events are consumed here and undo/redo
            // only works while hovering the viewport.
            if (!io.KeyCtrl)
                e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
        }
    }

    void ImGuiLayer::Begin()
    {
        ImGuiIO& io = ImGui::GetIO();
        Application& app = (*Application::TryGet());
        io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void ImGuiLayer::End() { ImGui::Render(); }

    String ImGuiLayer::SaveLayout()
    {
        ImGuiIO& io = ImGui::GetIO();
        const char* const oldIni = io.IniFilename;
        io.IniFilename = nullptr;
        size_t size;
        const char* const data = ImGui::SaveIniSettingsToMemory(&size);
        String layout(data, size);
        io.IniFilename = oldIni;
        return layout;
    }

    void ImGuiLayer::LoadLayout(const String& layout)
    {
        if (layout.empty())
            return;
        ImGui::LoadIniSettingsFromMemory(layout.c_str(), layout.size());
    }

} // namespace Crowny
