#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"
#include "Panels/NodeEditor/NodeEditorAdapter.h"
#include "Panels/NodeEditor/ImguiNodeEditorAdapter.h"
#include "Panels/NodeEditor/ImNodeFlowAdapter.h"
#include "Panels/NodeEditor/NodeEditorActions.h"
#include "Panels/NodeEditor/NodeEditorContext.h"
#include "Panels/NodeEditor/NodeEditorPanel.h"
#include "UI/Properties.h"

#include <imgui.h>

namespace Crowny
{
    // Toggle this to false to use the old ImNodeFlow backend
    static constexpr bool s_UseImguiNodeEditor = true;

    NodeEditorPanel::NodeEditorPanel(const String& name)
      : ImGuiPanel(name), m_Context(CreateScope<NodeEditorContext>())
    {
        if (s_UseImguiNodeEditor)
            m_Adapter = CreateScope<ImguiNodeEditorAdapter>();
        else
            m_Adapter = CreateScope<ImNodeFlowAdapter>();
    }

    NodeEditorPanel::~NodeEditorPanel() = default;

    void NodeEditorPanel::Render()
    {
        if (!m_Shown)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        BeginPanel(ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        if (!m_BeginCalled)
        {
            EndPanel();
            return;
        }

        RenderToolbar();

        Ref<NodeGraph> graph = m_Context->GetGraph();

        if (graph)
        {
            // Sync adapter with graph
            m_Adapter->SyncFromGraph(graph);

            // Render the node canvas
            RenderNodeCanvas();

            // Sync changes back
            m_Adapter->SyncToGraph(graph);
            m_Context->SetSelectedNodeID(m_Adapter->GetSelectedNodeID());

            const uint32_t graphVersion = graph->GetVersion();
            if (m_NeedsEvaluation || m_LastEvaluatedVersion != graphVersion)
                EvaluateGraph();

            // Auto-save the graph
            auto now = clock::now();
            if (m_LastSaveVersion != graphVersion && now - m_LastSaveTime >= std::chrono::seconds(5))
            {
                // TODO: We can probably just copy the graph here and do it in another thread.
                const auto graphAsset = m_Context->GetAsset();
                const Path& graphPath = ProjectLibrary::Get().UuidToPath(graphAsset.GetUUID());
                NodeGraphSerializer serializer(graph);
                serializer.Serialize(graphPath);
                m_LastSaveTime = now;
                m_LastSaveVersion = graphVersion;
            }
        }
        else
        {
            ImGui::TextDisabled("No graph loaded. Create or open a Node Graph asset.");
        }

        EndPanel();

        // Render properties window
        if (m_ShowProperties && graph)
            RenderProperties();
    }

    void NodeEditorPanel::SetGraph(AssetHandle<NodeGraphAsset> graphAsset)
    {
        m_Context->SetGraph(graphAsset);
        m_NeedsEvaluation = true;
    }

    Ref<NodeGraph> NodeEditorPanel::GetGraph() const { return m_Context->GetGraph(); }

    void NodeEditorPanel::EvaluateGraph()
    {
        Ref<NodeGraph> graph = m_Context->GetGraph();
        if (!graph)
            return;

        m_LastResult = graph->EvaluateGeometry();
        m_LastEvaluatedVersion = graph->GetVersion();
        m_NeedsEvaluation = false;
    }

    void NodeEditorPanel::RenderToolbar()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Graph"))
            {
                Ref<NodeGraph> graph = m_Context->GetGraph();

                if (ImGui::MenuItem("New Geometry Graph"))
                {
                    auto newGraph = CreateRef<NodeGraph>();
                    newGraph->SetDomain(NodeGraph::Domain::Geometry);
                    newGraph->SetName("New Geometry Graph");

                    // Add a default output node
                    auto outputNode = NodeRegistry::Get().Create("GeometryOutputNode");
                    if (outputNode)
                    {
                        outputNode->SetEditorPosition(glm::vec2(400.0f, 200.0f));
                        newGraph->AddNode(outputNode);
                    }
                    CW_ENGINE_ASSERT(false, "Unfinished");
                    // SetGraph(newGraph);
                }

                if (ImGui::MenuItem("Evaluate", "Ctrl+E", false, graph != nullptr))
                    EvaluateGraph();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add Node"))
            {
                Ref<NodeGraph> graph = m_Context->GetGraph();
                if (graph)
                    m_Adapter->RenderAddNodeMenu(graph);
                else
                    ImGui::TextDisabled("No graph loaded");
                ImGui::EndMenu();
            }

            ImGui::Checkbox("Properties", &m_ShowProperties);

            // Show evaluation result info
            if (m_LastResult)
            {
                ImGui::Separator();
                ImGui::Text("Verts: %u  Indices: %u", m_LastResult->GetVertexCount(), m_LastResult->GetIndexCount());
            }

            ImGui::EndMenuBar();
        }
    }

    void NodeEditorPanel::RenderNodeCanvas()
    {
        m_Adapter->Render();

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
                CopySelectedNodes();
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
                PasteNodes();
        }

        // Handle right-click context menu on the canvas
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##NodeEditorContextMenu");

        RenderContextMenu();
    }

    void NodeEditorPanel::RenderContextMenu()
    {
        if (ImGui::BeginPopup("##NodeEditorContextMenu"))
        {
            Ref<NodeGraph> graph = m_Context->GetGraph();
            if (graph)
            {
                ImGui::TextUnformatted("Add Node");
                ImGui::Separator();
                m_Adapter->RenderAddNodeMenu(graph);
            }
            ImGui::EndPopup();
        }
    }

    void NodeEditorPanel::RenderProperties()
    {
        ImGui::Begin("Node Properties", &m_ShowProperties);

        Ref<NodeGraph> graph = m_Context->GetGraph();

        if (graph)
        {
            if (ImGui::CollapsingHeader("Graph Inputs", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("+ Add Input"))
                    graph->AddInput("New Input", PinDataType::Float);

                const auto& inputs = graph->GetInputs();
                UUID toRemove = UUID::EMPTY;
                for (const auto& input : inputs)
                {
                    ImGui::PushID(input.ID.ToString().c_str());

                    char buffer[256];
                    strncpy(buffer, input.Name.c_str(), sizeof(buffer));
                    if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
                        graph->RenameInput(input.ID, buffer);

                    ImGui::SameLine();
                    if (ImGui::Button("X"))
                        toRemove = input.ID;

                    ImGui::PopID();
                }

                if (!toRemove.Empty())
                    graph->RemoveInput(toRemove);
            }

            ImGui::Separator();

            const UUID selectedId = m_Context->GetSelectedNodeID();
            if (!selectedId.Empty())
            {
                Node* node = graph->GetNode(selectedId);
                if (node)
                {
                    ImGui::Text("Node: %s", node->GetDisplayName().c_str());
                    ImGui::Text("Type: %s", node->GetTypeName().c_str());
                    ImGui::Separator();

                    UI::BeginPropertyGrid();
                    for (const auto& pin : node->GetInputPins())
                    {
                        if (pin->IsConnected())
                        {
                            ImGui::TextDisabled("  %s (connected)", pin->GetName().c_str());
                            ImGui::NextColumn();
                            ImGui::NextColumn();
                            continue;
                        }

                        PinValue val = pin->GetDefaultValue();
                        switch (pin->GetDataType())
                        {
                        case PinDataType::Float: {
                            float v = std::holds_alternative<float>(val) ? std::get<float>(val) : 0.0f;
                            if (UI::Property(pin->GetName().c_str(), v, 0.01f)) // TODO: Undo redo doesn't work here...
                                pin->SetDefaultValue(v);
                            break;
                        }
                        case PinDataType::Int: {
                            int32_t v = std::holds_alternative<int32_t>(val) ? std::get<int32_t>(val) : 0;
                            if (UI::Property(pin->GetName().c_str(), v))
                                pin->SetDefaultValue(v);
                            break;
                        }
                        case PinDataType::Vec3: {
                            glm::vec3 v = std::holds_alternative<glm::vec3>(val) ? std::get<glm::vec3>(val) : glm::vec3(0.0f);
                            if (UI::Property(pin->GetName().c_str(), v, 0.01f))
                                pin->SetDefaultValue(v);
                            break;
                        }
                        case PinDataType::Bool: {
                            bool v = std::holds_alternative<bool>(val) ? std::get<bool>(val) : false;
                            if (UI::Property(pin->GetName().c_str(), v))
                                pin->SetDefaultValue(v);
                            break;
                        }
                        default:
                            ImGui::Text("  %s: (no editor)", pin->GetName().c_str());
                            break;
                        }
                    }
                    UI::EndPropertyGrid();

                    ImGui::Separator();
                    if (ImGui::Button("Evaluate Graph"))
                        EvaluateGraph();
                }
            }
            else
            {
                ImGui::TextDisabled("Select a node to edit its properties.");
            }
        }

        ImGui::End();
    }

    void NodeEditorPanel::CopySelectedNodes()
    {
        Ref<NodeGraph> currentGraph = m_Context->GetGraph();
        if (!currentGraph)
            return;

        Vector<UUID> selectedIds = m_Adapter->GetSelectedNodes();
        if (selectedIds.empty())
            return;

        // Create a temporary graph to hold the copied nodes
        Ref<NodeGraph> copyGraph = CreateRef<NodeGraph>();
        copyGraph->SetDomain(currentGraph->GetDomain());

        // Copy nodes
        UnorderedMap<UUID, UUID> nodeMap; // old ID -> old ID
        for (const UUID& id : selectedIds)
        {
            Node* node = currentGraph->GetNode(id);
            if (node)
            {
                Ref<Node> clonedNode = NodeRegistry::Get().Create(node->GetTypeName(), node->GetID());
                clonedNode->SetEditorPosition(node->GetEditorPosition());
                for (const auto& pin : node->GetInputPins())
                {
                    if (auto clonedPin = clonedNode->FindInputPin(pin->GetName()))
                    {
                        clonedPin->SetID(pin->GetID());
                        clonedPin->SetDefaultValue(pin->GetDefaultValue());
                    }
                }
                for (const auto& pin : node->GetOutputPins())
                {
                    if (auto clonedPin = clonedNode->FindOutputPin(pin->GetName()))
                        clonedPin->SetID(pin->GetID());
                }
                copyGraph->AddNode(clonedNode);
                nodeMap[id] = id;
            }
        }

        // Copy internal connections
        for (const auto& conn : currentGraph->GetConnections())
        {
            if (nodeMap.find(conn.OutputNodeID) != nodeMap.end() && nodeMap.find(conn.InputNodeID) != nodeMap.end())
            {
                copyGraph->ConnectByPinID(conn.OutputPinID, conn.InputPinID);
            }
        }

        // Serialize to clipboard
        NodeGraphSerializer serializer(copyGraph);
        const String yaml = serializer.SerializeToString();
        ImGui::SetClipboardText(yaml.c_str());
    }

    void NodeEditorPanel::PasteNodes()
    {
        Ref<NodeGraph> currentGraph = m_Context->GetGraph();
        if (!currentGraph)
            return;

        const char* clipboardText = ImGui::GetClipboardText();
        if (!clipboardText || strlen(clipboardText) == 0)
            return;

        if (strstr(clipboardText, "Crowny NodeGraphAsset") == nullptr)
            return;

        Ref<NodeGraph> pastedGraph;
        NodeGraphSerializer serializer(pastedGraph);
        serializer.DeserializeFromString(clipboardText);

        if (!pastedGraph)
            return;

        UnorderedMap<UUID, UUID> pinIdMap;

        // Clear selection
        // We'd ideally select the newly pasted nodes, but for now we'll just add them

        for (const auto& [oldNodeId, oldNode] : pastedGraph->GetNodes())
        {
            Ref<Node> newNode = NodeRegistry::Get().Create(oldNode->GetTypeName());
            if (!newNode)
                continue;

            newNode->SetEditorPosition(oldNode->GetEditorPosition() + glm::vec2(20.0f, 20.0f));

            for (const auto& oldPin : oldNode->GetInputPins())
            {
                if (auto newPin = newNode->FindInputPin(oldPin->GetName()))
                {
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
                    newPin->SetDefaultValue(oldPin->GetDefaultValue());
                }
            }
            for (const auto& oldPin : oldNode->GetOutputPins())
            {
                if (auto newPin = newNode->FindOutputPin(oldPin->GetName()))
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
            }

            auto action = CreateRef<NodeAddedAction>(currentGraph, newNode);
            UndoRedo::Get().RegisterAction(action);
            currentGraph->AddNode(newNode);
        }

        for (const auto& oldConn : pastedGraph->GetConnections())
        {
            if (pinIdMap.find(oldConn.OutputPinID) != pinIdMap.end() && pinIdMap.find(oldConn.InputPinID) != pinIdMap.end())
            {
                UUID newOutPin = pinIdMap[oldConn.OutputPinID];
                UUID newInPin = pinIdMap[oldConn.InputPinID];
                auto action = CreateRef<NodesConnectedAction>(currentGraph, newOutPin, newInPin);
                UndoRedo::Get().RegisterAction(action);
                currentGraph->ConnectByPinID(newOutPin, newInPin);
            }
        }
    }
} // namespace Crowny

#endif // CW_WITH_NODES
