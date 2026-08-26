#include <catch2/catch_test_macros.hpp>

#include "Editor/UndoRedo.h"
#include "UI/Properties.h"

#include <imgui_internal.h>

using namespace Crowny;

namespace Crowny::PrefabOverrideContext
{
    bool IsOverridden(const char*) { return false; }
} // namespace Crowny::PrefabOverrideContext

namespace
{
    class ImGuiContextScope
    {
    public:
        ImGuiContextScope() : m_Previous(ImGui::GetCurrentContext()), m_Context(ImGui::CreateContext())
        {
            ImGui::SetCurrentContext(m_Context);
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(640.0f, 480.0f);
            io.DeltaTime = 1.0f / 60.0f;
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        }

        ~ImGuiContextScope()
        {
            ImGui::DestroyContext(m_Context);
            ImGui::SetCurrentContext(m_Previous);
        }

    private:
        ImGuiContext* m_Previous;
        ImGuiContext* m_Context;
    };

    class UndoRedoScope
    {
    public:
        UndoRedoScope() : m_Owned(!UndoRedo::IsStartedUp())
        {
            if (m_Owned)
                UndoRedo::StartUp();
        }

        ~UndoRedoScope()
        {
            if (m_Owned)
                UndoRedo::Shutdown();
        }

    private:
        bool m_Owned;
    };

    struct StackObservations
    {
        bool RowStyleBalanced = false;
        bool GridStyleBalanced = false;
        bool ItemWidthBalanced = false;
        bool IdBalanced = false;
        bool ColumnsFinalized = false;
    };

    StackObservations DrawMultilineProperty(bool disabled)
    {
        ImGui::NewFrame();
        ImGui::Begin(disabled ? "Disabled multiline property" : "Enabled multiline property");

        ImGuiContext& context = *GImGui;
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        const int initialStyleDepth = context.StyleVarStack.Size;
        const int initialItemWidthDepth = window->DC.ItemWidthStack.Size;
        const int initialIdDepth = window->IDStack.Size;

        UI::BeginPropertyGrid();
        if (disabled)
            ImGui::BeginDisabled(true);
        const int rowStyleDepth = context.StyleVarStack.Size;
        const int rowItemWidthDepth = window->DC.ItemWidthStack.Size;

        String value = "Line one\nLine two";
        UI::PropertyMultiline("Text", value);

        StackObservations observations;
        observations.RowStyleBalanced = context.StyleVarStack.Size == rowStyleDepth;
        observations.ItemWidthBalanced = window->DC.ItemWidthStack.Size == rowItemWidthDepth;
        if (disabled)
            ImGui::EndDisabled();
        UI::EndPropertyGrid();

        observations.GridStyleBalanced = context.StyleVarStack.Size == initialStyleDepth;
        observations.ItemWidthBalanced &= window->DC.ItemWidthStack.Size == initialItemWidthDepth;
        observations.IdBalanced = window->IDStack.Size == initialIdDepth;
        observations.ColumnsFinalized = window->DC.CurrentColumns == nullptr;
        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        ImGui::End();
        ImGui::EndFrame();
        return observations;
    }
} // namespace

TEST_CASE("Multiline properties balance ImGui row and property-grid stacks", "[Editor][Properties][ImGui]")
{
    UndoRedoScope undoRedo;
    ImGuiContextScope imgui;

    for (bool disabled : { false, true })
    {
        const StackObservations observations = DrawMultilineProperty(disabled);
        CHECK(observations.RowStyleBalanced);
        CHECK(observations.GridStyleBalanced);
        CHECK(observations.ItemWidthBalanced);
        CHECK(observations.IdBalanced);
        CHECK(observations.ColumnsFinalized);
    }
}
