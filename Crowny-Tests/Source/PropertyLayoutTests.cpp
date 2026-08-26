#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
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

    struct NamedOption
    {
        String Name;
    };

    void DrawDropdownProperties(const Vector<String>& stringOptions, const Vector<NamedOption>& namedOptions, int& stringSelection,
                                int& literalSelection, int& namedSelection)
    {
        ImGui::NewFrame();
        ImGui::Begin("Dropdown allocation test");
        UI::BeginPropertyGrid();
        UI::PropertyDropdown("A deliberately long string option label", stringOptions, stringSelection);
        UI::PropertyDropdown("A deliberately long literal option label",
                             { "First long literal option", "Second long literal option", "Third long literal option",
                               "Fourth long literal option", "Fifth long literal option", "Sixth long literal option",
                               "Seventh long literal option", "Eighth long literal option", "Ninth long literal option",
                               "Tenth long literal option" },
                             literalSelection);
        UI::PropertyDropdown("A deliberately long selected option label", namedOptions, namedSelection,
                             [](const NamedOption& option) -> const String& { return option.Name; });
        UI::EndPropertyGrid();
        ImGui::End();
        ImGui::EndFrame();
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

TEST_CASE("Property dropdowns allocate nothing after ImGui warm-up", "[Editor][Properties][ImGui][Memory][Frame]")
{
    UndoRedoScope undoRedo;
    ImGuiContextScope imgui;

    const Vector<String> stringOptions{ "First deliberately long string option", "Second deliberately long string option" };
    const Vector<NamedOption> namedOptions{ { "First deliberately long named option" }, { "Second deliberately long named option" } };
    int stringSelection = 1;
    int literalSelection = 9;
    int namedSelection = 1;

    for (uint32_t frame = 0; frame < 8; frame++)
        DrawDropdownProperties(stringOptions, namedOptions, stringSelection, literalSelection, namedSelection);

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
        DrawDropdownProperties(stringOptions, namedOptions, stringSelection, literalSelection, namedSelection);
    const Memory::ThreadAllocationSnapshot delta =
      Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(stringSelection == 1);
    CHECK(literalSelection == 9);
    CHECK(namedSelection == 1);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
