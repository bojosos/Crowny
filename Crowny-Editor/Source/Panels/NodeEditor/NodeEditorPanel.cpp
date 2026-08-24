#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraphEvaluator.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/NodeGraph/UnknownNode.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Export/MeshExporter.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"

#include "Panels/NodeEditor/ImNodeFlowAdapter.h"
#include "Panels/NodeEditor/ImguiNodeEditorAdapter.h"
#include "Panels/NodeEditor/NodeEditorActions.h"
#include "Panels/NodeEditor/NodeEditorAdapter.h"
#include "Panels/NodeEditor/NodeEditorContext.h"
#include "Panels/NodeEditor/NodeEditorPanel.h"

#include "UI/Properties.h"

#include <imgui.h>

#include <limits>

namespace Crowny
{
    // Toggle this to false to use the old ImNodeFlow backend
    static constexpr bool s_UseImguiNodeEditor = true;

    NodeEditorPanel::NodeEditorPanel(const String& name) : ImGuiPanel(name), m_Context(CreateScope<NodeEditorContext>())
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
        const bool panelVisible = BeginPanel(ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        if (!panelVisible)
        {
            EndPanel();
            return;
        }

        RenderToolbar();

        Ref<NodeGraph> graph = m_Context->GetGraph();

        if (graph)
        {
            m_Adapter->SyncFromGraph(graph);

            if (m_ShowProperties)
            {
                const float availableWidth = ImGui::GetContentRegionAvail().x;
                const bool stackProperties = availableWidth < 640.0f;
                if (stackProperties)
                {
                    const float canvasHeight = glm::max(1.0f, ImGui::GetContentRegionAvail().y * 0.58f);
                    if (ImGui::BeginChild("##NodeCanvasRegion", ImVec2(0.0f, canvasHeight), false,
                                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                        RenderNodeCanvas();
                    ImGui::EndChild();

                    if (ImGui::BeginChild("##NodePropertiesRegion", ImVec2(0.0f, 0.0f), true))
                        RenderProperties();
                    ImGui::EndChild();
                }
                else
                {
                    const float sidebarWidth = glm::clamp(availableWidth * 0.3f, 280.0f, 360.0f);
                    const float canvasWidth = glm::max(1.0f, availableWidth - sidebarWidth - ImGui::GetStyle().ItemSpacing.x);

                    if (ImGui::BeginChild("##NodeCanvasRegion", ImVec2(canvasWidth, 0.0f), false,
                                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                        RenderNodeCanvas();
                    ImGui::EndChild();

                    ImGui::SameLine();
                    if (ImGui::BeginChild("##NodePropertiesRegion", ImVec2(0.0f, 0.0f), true))
                        RenderProperties();
                    ImGui::EndChild();
                }
            }
            else
            {
                RenderNodeCanvas();
            }

            m_Adapter->SyncToGraph(graph);
            m_Context->SetSelectedNodeID(m_Adapter->GetSelectedNodeID());

            const uint32_t graphVersion = graph->GetVersion();
            if (m_NeedsEvaluation || m_LastEvaluatedVersion != graph->GetEvaluationVersion())
                EvaluateGraph();

            const auto now = clock::now();
            if (m_LastSaveVersion != graphVersion && now - m_LastSaveTime >= std::chrono::seconds(5))
                SaveGraph();

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    EvaluateGraph();
                if (ImGui::IsKeyPressed(ImGuiKey_S))
                    SaveGraph();
            }
        }
        else
        {
            const ImVec2 available = ImGui::GetContentRegionAvail();
            const char* title = "No graph open";
            const char* hint = "Open a node graph asset to start editing.";
            ImGui::SetCursorPos(ImVec2(glm::max(16.0f, (available.x - ImGui::CalcTextSize(title).x) * 0.5f), available.y * 0.42f));
            ImGui::TextUnformatted(title);
            ImGui::SetCursorPosX(glm::max(16.0f, (available.x - ImGui::CalcTextSize(hint).x) * 0.5f));
            ImGui::TextDisabled("%s", hint);
        }

        EndPanel();
    }

    void NodeEditorPanel::SetGraph(AssetHandle<NodeGraphAsset> graphAsset)
    {
        m_Context->SetGraph(graphAsset);
        m_NeedsEvaluation = true;
        m_LastResult = nullptr;
        m_LastEvaluatedVersion = std::numeric_limits<uint32_t>::max();
        Ref<NodeGraph> graph = m_Context->GetGraph();
        m_LastSaveVersion = graph ? graph->GetVersion() : std::numeric_limits<uint32_t>::max();
        m_LastSaveTime = clock::now();
        m_SaveError.clear();
    }

    Ref<NodeGraph> NodeEditorPanel::GetGraph() const { return m_Context->GetGraph(); }

    void NodeEditorPanel::EvaluateGraph()
    {
        Ref<NodeGraph> graph = m_Context->GetGraph();
        if (!graph)
            return;

        m_LastResult = graph->EvaluateGeometry();
        m_LastEvaluatedVersion = graph->GetEvaluationVersion();
        m_NeedsEvaluation = false;
    }

    void NodeEditorPanel::RenderToolbar()
    {
        if (ImGui::BeginMenuBar())
        {
            Ref<NodeGraph> graph = m_Context->GetGraph();
            ImGui::BeginDisabled(!graph);

            if (ImGui::Button("Evaluate"))
                EvaluateGraph();
            UI::SetTooltip("Evaluate graph (Ctrl+E)");

            if (ImGui::Button("Save"))
                SaveGraph();
            UI::SetTooltip("Save graph (Ctrl+S)");

            if (ImGui::BeginMenu("Add node"))
            {
                m_Adapter->RenderAddNodeMenu(graph);
                ImGui::EndMenu();
            }

            if (ImGui::Button("Export OBJ"))
                ExportGraph();

            ImGui::EndDisabled();
            ImGui::Checkbox("Properties", &m_ShowProperties);

            RenderStatus();

            ImGui::EndMenuBar();
        }
    }

    void NodeEditorPanel::RenderStatus()
    {
        const Ref<NodeGraph> graph = m_Context->GetGraph();
        if (!graph)
            return;

        ImGui::Separator();
        if (!m_SaveError.empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.3f, 1.0f), "Save failed");
            UI::SetTooltip(m_SaveError);
        }
        else if (m_LastSaveVersion == graph->GetVersion())
        {
            ImGui::TextColored(ImVec4(0.45f, 0.78f, 0.48f, 1.0f), "Saved");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.25f, 1.0f), "Unsaved");
        }

        ImGui::Separator();
        const String& evaluationError = graph->GetLastEvaluationError();
        if (!evaluationError.empty())
        {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.3f, 1.0f), "Evaluation error");
            UI::SetTooltip(evaluationError);
        }
        else if (m_NeedsEvaluation || m_LastEvaluatedVersion != graph->GetEvaluationVersion())
        {
            ImGui::TextDisabled("Needs evaluation");
        }
        else if (m_LastResult)
        {
            ImGui::Text("%u verts, %u indices", m_LastResult->GetVertexCount(), m_LastResult->GetIndexCount());
        }
        else
        {
            ImGui::TextDisabled("No geometry output");
        }
    }

    bool NodeEditorPanel::SaveGraph()
    {
        Ref<NodeGraph> graph = m_Context->GetGraph();
        const auto graphAsset = m_Context->GetAsset();
        m_LastSaveTime = clock::now();

        if (!graph || !graphAsset)
        {
            m_SaveError = "No graph asset is open.";
            return false;
        }

        const Path& graphPath = ProjectLibrary::Get().UuidToPath(graphAsset.GetUUID());
        if (graphPath.empty())
        {
            m_SaveError = "The graph has no project path.";
            return false;
        }

        NodeGraphSerializer serializer(graph);
        if (!serializer.Serialize(graphPath))
        {
            m_SaveError = "Could not write the graph asset.";
            return false;
        }

        m_SaveError.clear();
        m_LastSaveVersion = graph->GetVersion();
        return true;
    }

    void NodeEditorPanel::ExportGraph()
    {
        const Ref<NodeGraph> graph = m_Context->GetGraph();
        if (!graph)
            return;

        if (m_NeedsEvaluation || m_LastEvaluatedVersion != graph->GetEvaluationVersion())
            EvaluateGraph();
        if (!m_LastResult)
            return;

        Vector<Path> paths;
        if (FileSystem::OpenFileDialog(FileDialogType::SaveFile, paths, "Export Geometry", {}, { { "Wavefront OBJ", "*.obj" } }, "geometry.obj") &&
            !paths.empty())
        {
            MeshExporter exporter(m_LastResult);
            exporter.Export(paths.front());
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
                ImGui::TextUnformatted("Add node");
                ImGui::Separator();
                m_Adapter->RenderAddNodeMenu(graph);
            }
            ImGui::EndPopup();
        }
    }

    void NodeEditorPanel::RenderProperties()
    {
        const Ref<NodeGraph> graph = m_Context->GetGraph();
        if (!graph)
            return;

        ImGui::TextUnformatted("Properties");
        const float hideWidth = ImGui::CalcTextSize("Hide").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(glm::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - hideWidth));
        if (ImGui::SmallButton("Hide"))
            m_ShowProperties = false;
        ImGui::Separator();

        ImGui::TextUnformatted("Graph inputs");
        ImGui::SameLine();
        ImGui::TextDisabled("%zu", graph->GetInputs().size());
        ImGui::Spacing();

        const auto& inputs = graph->GetInputs();
        if (inputs.empty())
            ImGui::TextDisabled("No graph inputs.");

        UUID toRemove = UUID::EMPTY;
        for (const GraphInput& input : inputs)
        {
            ImGui::PushID(input.ID.ToString().c_str());

            String inputName = input.Name.c_str();
            const float removeWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(glm::max(80.0f, ImGui::GetContentRegionAvail().x - removeWidth - ImGui::GetStyle().ItemSpacing.x));
            if (ImGui::InputText("##Name", &inputName))
                graph->RenameInput(input.ID, inputName);
            UI::SetTooltip(PinDataTypeName(input.DataType));

            ImGui::SameLine();
            if (ImGui::Button("Remove"))
                ImGui::OpenPopup("Remove input?");

            if (ImGui::BeginPopupModal("Remove input?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Remove \"%s\"?", input.Name.c_str());
                ImGui::TextDisabled("Nodes that use this input will lose their value.");
                ImGui::Spacing();

                if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
                    ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (ImGui::Button("Remove input", ImVec2(110.0f, 0.0f)))
                {
                    toRemove = input.ID;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        if (!toRemove.Empty())
            graph->RemoveInput(toRemove);

        ImGui::Spacing();
        if (ImGui::Button("+ Add input", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
        {
            m_NewInputName = "New input";
            m_NewInputType = PinDataType::Float;
            ImGui::OpenPopup("Add graph input");
        }

        if (ImGui::BeginPopupModal("Add graph input", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::TextUnformatted("Name");
            ImGui::SetNextItemWidth(280.0f);
            ImGui::InputText("##InputName", &m_NewInputName);

            ImGui::TextUnformatted("Type");
            ImGui::SetNextItemWidth(280.0f);
            if (ImGui::BeginCombo("##InputType", PinDataTypeName(m_NewInputType)))
            {
                static constexpr PinDataType supportedTypes[] = { PinDataType::Float, PinDataType::Int,  PinDataType::Vec2,
                                                                  PinDataType::Vec3,  PinDataType::Vec4, PinDataType::Bool };
                for (const PinDataType type : supportedTypes)
                {
                    const bool selected = type == m_NewInputType;
                    if (ImGui::Selectable(PinDataTypeName(type), selected))
                        m_NewInputType = type;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            ImGui::BeginDisabled(m_NewInputName.empty());
            if (ImGui::Button("Add input", ImVec2(90.0f, 0.0f)))
            {
                graph->AddInput(m_NewInputName, m_NewInputType);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Selected node");
        ImGui::Spacing();

        const UUID selectedId = m_Context->GetSelectedNodeID();
        if (selectedId.Empty())
        {
            ImGui::TextDisabled("Select a node to edit its values.");
            return;
        }

        Node* node = graph->GetNode(selectedId);
        if (!node)
        {
            ImGui::TextDisabled("The selected node is no longer available.");
            return;
        }

        ImGui::TextUnformatted(node->GetDisplayName().c_str());
        ImGui::TextDisabled("%s", node->GetTypeName().c_str());
        ImGui::Spacing();

        if (node->GetInputPins().empty())
        {
            ImGui::TextDisabled("This node has no input values.");
            return;
        }

        UI::BeginPropertyGrid();
        for (const auto& pin : node->GetInputPins())
        {
            if (pin->IsConnected())
            {
                ImGui::TextUnformatted(pin->GetName().c_str());
                ImGui::NextColumn();
                ImGui::TextDisabled("Connected");
                ImGui::NextColumn();
                continue;
            }

            PinValue val = pin->GetDefaultValue();
            const UUID pinId = pin->GetID();
            const PinValue oldValue = val;
            UndoRedo::Get().BeginComponentScope([graph, pinId, oldValue]() -> Ref<UndoAction> {
                Pin* current = graph->GetPin(pinId);
                return CreateRef<PinDefaultValueAction>(graph, pinId, oldValue, current ? current->GetDefaultValue() : oldValue);
            });
            switch (pin->GetDataType())
            {
            case PinDataType::Float: {
                float v = std::holds_alternative<float>(val) ? std::get<float>(val) : 0.0f;
                if (UI::Property(pin->GetName().c_str(), v, 0.01f))
                    pin->SetDefaultValue(v);
                break;
            }
            case PinDataType::Vec2: {
                glm::vec2 v = std::holds_alternative<glm::vec2>(val) ? std::get<glm::vec2>(val) : glm::vec2(0.0f);
                if (UI::Property(pin->GetName().c_str(), v, 0.01f))
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
            case PinDataType::Vec4: {
                glm::vec4 v = std::holds_alternative<glm::vec4>(val) ? std::get<glm::vec4>(val) : glm::vec4(0.0f);
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
                ImGui::TextUnformatted(pin->GetName().c_str());
                ImGui::NextColumn();
                ImGui::TextDisabled("No editor");
                ImGui::NextColumn();
                break;
            }
            UndoRedo::Get().EndComponentScope();
        }
        UI::EndPropertyGrid();
    }

    void NodeEditorPanel::CopySelectedNodes()
    {
        Ref<NodeGraph> currentGraph = m_Context->GetGraph();
        if (!currentGraph)
            return;

        const Vector<UUID> selectedIds = m_Adapter->GetSelectedNodes();
        if (selectedIds.empty())
            return;

        // Create a temporary graph to hold the copied nodes
        Ref<NodeGraph> copyGraph = CreateRef<NodeGraph>();
        copyGraph->SetDomain(currentGraph->GetDomain());

        // Copy nodes
        UnorderedMap<UUID, UUID> nodeMap; // old ID -> old ID
        UnorderedMap<UUID, UUID> graphInputMap;
        for (const UUID& id : selectedIds)
        {
            Node* node = currentGraph->GetNode(id);
            if (node)
            {
                Ref<Node> clonedNode =
                  NodeRegistry::Get().HasType(node->GetTypeName()) ? NodeRegistry::Get().Create(node->GetTypeName(), node->GetID()) : nullptr;
                if (!clonedNode)
                    clonedNode = CreateRef<UnknownNode>(node->GetID(), node->GetTypeName());
                if (node->GetTypeName() == "GraphInputNode"_sid)
                {
                    const UUID sourceInputId = static_cast<GraphInputNode*>(node)->GetInputID();
                    UUID copiedInputId = UUID::EMPTY;
                    if (const auto it = graphInputMap.find(sourceInputId); it != graphInputMap.end())
                        copiedInputId = it->second;
                    else if (const GraphInput* input = currentGraph->GetInput(sourceInputId))
                    {
                        copiedInputId = copyGraph->AddInput(input->Name, input->DataType, input->DefaultValue);
                        graphInputMap[sourceInputId] = copiedInputId;
                    }
                    static_cast<GraphInputNode*>(clonedNode.get())->SetInputID(copiedInputId);
                }
                clonedNode->SetEditorPosition(node->GetEditorPosition());
                for (const auto& pin : node->GetInputPins())
                {
                    if (auto clonedPin = clonedNode->FindInputPin(pin->GetName()))
                    {
                        clonedPin->SetID(pin->GetID());
                        clonedPin->SetDefaultValue(pin->GetDefaultValue());
                    }
                    else if (auto* unknown = dynamic_cast<UnknownNode*>(clonedNode.get()))
                    {
                        unknown->AddSerializedPin(pin->GetID(), pin->GetName(), Pin::Direction::Input, pin->GetDataType(), pin->GetDefaultValue());
                    }
                }
                for (const auto& pin : node->GetOutputPins())
                {
                    if (auto clonedPin = clonedNode->FindOutputPin(pin->GetName()))
                        clonedPin->SetID(pin->GetID());
                    else if (auto* unknown = dynamic_cast<UnknownNode*>(clonedNode.get()))
                    {
                        unknown->AddSerializedPin(pin->GetID(), pin->GetName(), Pin::Direction::Output, pin->GetDataType(), pin->GetDefaultValue());
                    }
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
                copyGraph->ConnectByPinID(conn.OutputPinID, conn.InputPinID, conn.ID);
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
        if (!serializer.DeserializeFromString(clipboardText))
            return;

        if (!pastedGraph)
            return;

        UnorderedMap<UUID, UUID> pinIdMap;
        UnorderedMap<UUID, UUID> graphInputMap;
        for (const GraphInput& pastedInput : pastedGraph->GetInputs())
        {
            UUID targetId = UUID::EMPTY;
            for (const GraphInput& currentInput : currentGraph->GetInputs())
            {
                if (currentInput.Name == pastedInput.Name && currentInput.DataType == pastedInput.DataType)
                {
                    targetId = currentInput.ID;
                    break;
                }
            }
            if (targetId.Empty())
                targetId = currentGraph->AddInput(pastedInput.Name, pastedInput.DataType, pastedInput.DefaultValue);
            graphInputMap[pastedInput.ID] = targetId;
        }

        // Clear selection
        // We'd ideally select the newly pasted nodes, but for now we'll just add them

        for (const auto& [oldNodeId, oldNode] : pastedGraph->GetNodes())
        {
            Ref<Node> newNode = NodeRegistry::Get().HasType(oldNode->GetTypeName()) ? NodeRegistry::Get().Create(oldNode->GetTypeName()) : nullptr;
            if (!newNode)
                newNode = CreateRef<UnknownNode>(UuidGenerator::Generate(), oldNode->GetTypeName());

            newNode->SetEditorPosition(oldNode->GetEditorPosition() + glm::vec2(20.0f, 20.0f));

            if (oldNode->GetTypeName() == "GraphInputNode"_sid)
            {
                const UUID oldInputId = static_cast<GraphInputNode*>(oldNode.get())->GetInputID();
                const auto mappedInput = graphInputMap.find(oldInputId);
                static_cast<GraphInputNode*>(newNode.get())->SetInputID(mappedInput == graphInputMap.end() ? UUID::EMPTY : mappedInput->second);
            }

            for (const auto& oldPin : oldNode->GetInputPins())
            {
                if (auto newPin = newNode->FindInputPin(oldPin->GetName()))
                {
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
                    newPin->SetDefaultValue(oldPin->GetDefaultValue());
                }
                else if (auto* unknown = dynamic_cast<UnknownNode*>(newNode.get()))
                {
                    auto newPin = unknown->AddSerializedPin(UuidGenerator::Generate(), oldPin->GetName(), Pin::Direction::Input,
                                                            oldPin->GetDataType(), oldPin->GetDefaultValue());
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
                }
            }
            for (const auto& oldPin : oldNode->GetOutputPins())
            {
                if (auto newPin = newNode->FindOutputPin(oldPin->GetName()))
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
                else if (auto* unknown = dynamic_cast<UnknownNode*>(newNode.get()))
                {
                    auto newPin = unknown->AddSerializedPin(UuidGenerator::Generate(), oldPin->GetName(), Pin::Direction::Output,
                                                            oldPin->GetDataType(), oldPin->GetDefaultValue());
                    pinIdMap[oldPin->GetID()] = newPin->GetID();
                }
            }

            const auto action = CreateRef<NodeAddedAction>(currentGraph, newNode);
            action->Commit();
            UndoRedo::Get().RegisterAction(action);
        }

        for (const auto& oldConn : pastedGraph->GetConnections())
        {
            if (pinIdMap.find(oldConn.OutputPinID) != pinIdMap.end() && pinIdMap.find(oldConn.InputPinID) != pinIdMap.end())
            {
                const UUID newOutPin = pinIdMap[oldConn.OutputPinID];
                const UUID newInPin = pinIdMap[oldConn.InputPinID];
                const auto action = CreateRef<NodesConnectedAction>(currentGraph, newOutPin, newInPin);
                action->Commit();
                UndoRedo::Get().RegisterAction(action);
            }
        }
    }
} // namespace Crowny

#endif // CW_WITH_NODES
