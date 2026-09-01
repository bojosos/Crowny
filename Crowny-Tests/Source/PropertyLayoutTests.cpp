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
        float MultilineHeight = 0.0f;
        float ExpectedMultilineHeight = 0.0f;
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
        UI::PropertyMultiline("Text", value, 3);

        StackObservations observations;
        observations.MultilineHeight = ImGui::GetItemRectSize().y;
        observations.ExpectedMultilineHeight = ImGui::GetTextLineHeightWithSpacing() * 3.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
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
                                int& literalSelection, int& namedSelection, int& borrowedSelection, int& mixedBorrowedSelection,
                                uint32_t& borrowedLabelRequests)
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
        const auto borrowedLabelAt = [&](size_t index) {
            borrowedLabelRequests++;
            return index == 0 ? "(None)" : namedOptions[index - 1u].Name.c_str();
        };
        UI::PropertyDropdown("A deliberately long borrowed option label", namedOptions.size() + 1u, borrowedSelection, borrowedLabelAt);
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        UI::PropertyDropdown("A deliberately long mixed borrowed option label", namedOptions.size() + 1u, mixedBorrowedSelection,
                             borrowedLabelAt);
        ImGui::PopItemFlag();
        UI::EndPropertyGrid();
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::End();
        ImGui::EndFrame();
    }

    struct WindowStackObservations
    {
        bool Visible = false;
        bool NestedStylePushed = false;
        bool StyleBalanced = false;
        bool WindowBalanced = false;
    };

    WindowStackObservations DrawScopedWindowWithNestedStyle(bool collapsed)
    {
        ImGui::NewFrame();
        if (collapsed)
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);

        ImGuiContext& context = *GImGui;
        const int initialStyleDepth = context.StyleVarStack.Size;
        const int initialWindowDepth = context.CurrentWindowStack.Size;

        WindowStackObservations observations;
        {
            UI::ScopedWindow window("Settings stack test");
            observations.Visible = window.IsVisible();
            if (observations.Visible)
            {
                UI::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
                observations.NestedStylePushed = context.StyleVarStack.Size == initialStyleDepth + 1;
                ImGui::TextUnformatted("Settings");
            }
        }

        observations.StyleBalanced = context.StyleVarStack.Size == initialStyleDepth;
        observations.WindowBalanced = context.CurrentWindowStack.Size == initialWindowDepth;
        ImGui::EndFrame();
        return observations;
    }
} // namespace

TEST_CASE("Scoped windows end after nested style guards unwind", "[Editor][UI][ImGui][Settings]")
{
    ImGuiContextScope imgui;

    const WindowStackObservations visible = DrawScopedWindowWithNestedStyle(false);
    CHECK(visible.Visible);
    CHECK(visible.NestedStylePushed);
    CHECK(visible.StyleBalanced);
    CHECK(visible.WindowBalanced);

    const WindowStackObservations collapsed = DrawScopedWindowWithNestedStyle(true);
    CHECK_FALSE(collapsed.Visible);
    CHECK_FALSE(collapsed.NestedStylePushed);
    CHECK(collapsed.StyleBalanced);
    CHECK(collapsed.WindowBalanced);
}

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
        CHECK(observations.MultilineHeight == Catch::Approx(observations.ExpectedMultilineHeight));
    }
}

TEST_CASE("Labeled ImGui scopes isolate asset reference child IDs", "[Editor][Properties][ImGui][AssetReference]")
{
    ImGuiContextScope imgui;

    ImGui::NewFrame();
    ImGui::Begin("Asset reference ID test");
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const int initialIdDepth = window->IDStack.Size;
    const auto captureIds = [](const char* propertyLabel) {
        UI::ScopedID propertyScope(propertyLabel);
        const UI::PopupLabelId popupId = UI::PopupLabelId::Create("AssetSearch", 0u);
        return Pair<ImGuiID, ImGuiID>(ImGui::GetID(popupId.CStr()), ImGui::GetID("Null##1"));
    };

    const Pair<ImGuiID, ImGuiID> meshIds = captureIds("Mesh");
    const Pair<ImGuiID, ImGuiID> materialIds = captureIds("Material");

    CHECK(meshIds.first != materialIds.first);
    CHECK(meshIds.second != materialIds.second);
    CHECK(window->IDStack.Size == initialIdDepth);

    ImGui::End();
    ImGui::EndFrame();
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
    int borrowedSelection = 2;
    int mixedBorrowedSelection = 1;
    uint32_t borrowedLabelRequests = 0;

    for (uint32_t frame = 0; frame < 8; frame++)
        DrawDropdownProperties(stringOptions, namedOptions, stringSelection, literalSelection, namedSelection, borrowedSelection,
                               mixedBorrowedSelection, borrowedLabelRequests);

    borrowedLabelRequests = 0;
    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
        DrawDropdownProperties(stringOptions, namedOptions, stringSelection, literalSelection, namedSelection, borrowedSelection,
                               mixedBorrowedSelection, borrowedLabelRequests);
    const Memory::ThreadAllocationSnapshot delta =
      Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

    CHECK(stringSelection == 1);
    CHECK(literalSelection == 9);
    CHECK(namedSelection == 1);
    CHECK(borrowedSelection == 2);
    CHECK(mixedBorrowedSelection == 1);
    CHECK(borrowedLabelRequests == 120u);
    CHECK(delta.AllocationCount == 0u);
    CHECK(delta.RequestedBytes == 0u);
}
