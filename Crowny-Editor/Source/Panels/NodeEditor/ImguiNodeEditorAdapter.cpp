#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Panels/NodeEditor/ImguiNodeEditorAdapter.h"

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Editor/EditorLayer.h"
#include "Panels/NodeEditor/NodeEditorActions.h"

#include <imgui.h>
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace Crowny
{
    struct ImguiNodeEditorAdapter::Impl
    {
        ed::EditorContext* Context = nullptr;

        uintptr_t NextId = 1;
        UnorderedMap<UUID, uintptr_t> UuidToId;
        UnorderedMap<uintptr_t, UUID> IdToUuid;

        uintptr_t GetOrCreateId(const UUID& uuid)
        {
            if (auto it = UuidToId.find(uuid); it != UuidToId.end())
                return it->second;
            uintptr_t id = NextId++;
            UuidToId[uuid] = id;
            IdToUuid[id] = uuid;
            return id;
        }

        UUID GetUuid(uintptr_t id) const
        {
            if (auto it = IdToUuid.find(id); it != IdToUuid.end())
                return it->second;
            return UUID::EMPTY;
        }

        void ClearIds()
        {
            UuidToId.clear();
            IdToUuid.clear();
            NextId = 1;
        }
    };

    ImguiNodeEditorAdapter::ImguiNodeEditorAdapter() : m_Impl(CreateScope<Impl>())
    {
        ed::Config config;
        config.SettingsFile = "imgui_node_editor.json";
        config.EnableSmoothZoom = true;
        config.SmoothZoomPower = 1.1f;
        m_Impl->Context = ed::CreateEditor(&config);

        ed::SetCurrentEditor(m_Impl->Context);
        ed::GetStyle().ScrollDuration = 0.05f; // Short duration for snappiness
        ed::SetCurrentEditor(nullptr);
    }

    ImguiNodeEditorAdapter::~ImguiNodeEditorAdapter() { ed::DestroyEditor(m_Impl->Context); }

    void ImguiNodeEditorAdapter::SyncFromGraph(const Ref<NodeGraph>& graph)
    {
        if (m_CurrentGraph != graph)
        {
            m_CurrentGraph = graph;
            m_NeedsSync = true;
        }

        if (!m_NeedsSync || !m_CurrentGraph)
            return;

        ed::SetCurrentEditor(m_Impl->Context);

        for (const auto& [id, node] : m_CurrentGraph->GetNodes())
        {
            ed::NodeId nodeId = m_Impl->GetOrCreateId(id);
            ed::SetNodePosition(nodeId, ImVec2(node->GetEditorPosition().x, node->GetEditorPosition().y));
        }

        m_NeedsSync = false;
        ed::SetCurrentEditor(nullptr);
    }

    void ImguiNodeEditorAdapter::SyncToGraph(const Ref<NodeGraph>& graph)
    {
        if (!m_CurrentGraph)
            return;

        ed::SetCurrentEditor(m_Impl->Context);

        if (ed::HasSelectionChanged())
        {
            m_SelectedNodes.clear();
            m_SelectedNodeID = UUID::EMPTY;

            const int selectedNodeCount = ed::GetSelectedObjectCount();
            if (selectedNodeCount > 0)
            {
                Vector<ed::NodeId> selectedNodes(selectedNodeCount);
                int count = ed::GetSelectedNodes(selectedNodes.data(), selectedNodeCount);

                for (int i = 0; i < count; ++i)
                {
                    const UUID uuid = m_Impl->GetUuid((uintptr_t)selectedNodes[i]);
                    if (!uuid.Empty())
                    {
                        m_SelectedNodes.push_back(uuid);
                        if (m_SelectedNodeID.Empty())
                            m_SelectedNodeID = uuid;
                    }
                }
            }
        }

        // Only sync positions if the user is interacting with the editor
        if (ed::IsActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            for (const auto& [id, node] : m_CurrentGraph->GetNodes())
            {
                const ed::NodeId nodeId = m_Impl->GetOrCreateId(id);
                const ImVec2 pos = ed::GetNodePosition(nodeId);
                // Use a small epsilon to avoid tiny precision-induced updates
                if (std::abs(pos.x - node->GetEditorPosition().x) > 0.01f || std::abs(pos.y - node->GetEditorPosition().y) > 0.01f)
                {
                    node->SetEditorPosition(glm::vec2(pos.x, pos.y));
                }
            }
        }

        ed::SetCurrentEditor(nullptr);
    }

    void ImguiNodeEditorAdapter::Render()
    {
        ed::SetCurrentEditor(m_Impl->Context);

        ed::Begin("##NodeEditorCanvas", ImGui::GetContentRegionAvail());

        if (m_CurrentGraph)
        {
            // Draw Nodes
            for (const auto& [id, node] : m_CurrentGraph->GetNodes())
            {
                ed::NodeId nodeId = m_Impl->GetOrCreateId(id);
                ed::BeginNode(nodeId);

                ImGui::TextUnformatted(node->GetDisplayName().c_str());
                if (node->GetTypeName() == "GraphInputNode")
                {
                    GraphInputNode* inputNode = static_cast<GraphInputNode*>(node.get());
                    const auto& inputs = m_CurrentGraph->GetInputs();
                    int selected = -1;
                    for (int i = 0; i < (int)inputs.size(); i++)
                    {
                        if (inputs[i].ID == inputNode->GetInputID())
                        {
                            selected = i;
                            break;
                        }
                    }

                    ImGui::PushItemWidth(120.0f);
                    if (ImGui::BeginCombo("##Input", selected >= 0 ? inputs[selected].Name.c_str() : "None"))
                    {
                        for (int i = 0; i < (int)inputs.size(); i++)
                        {
                            if (ImGui::Selectable(inputs[i].Name.c_str(), selected == i))
                                inputNode->SetInputID(inputs[i].ID);
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();
                }

                // Pins
                ImGui::BeginGroup();
                for (const auto& pin : node->GetInputPins())
                {
                    ed::PinId pinId = m_Impl->GetOrCreateId(pin->GetID());
                    ed::BeginPin(pinId, ed::PinKind::Input);
                    ImGui::Text("-> %s", pin->GetName().c_str());
                    ed::EndPin();
                }
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                for (const auto& pin : node->GetOutputPins())
                {
                    ed::PinId pinId = m_Impl->GetOrCreateId(pin->GetID());
                    ed::BeginPin(pinId, ed::PinKind::Output);
                    ImGui::Text("%s ->", pin->GetName().c_str());
                    ed::EndPin();
                }
                ImGui::EndGroup();

                ed::EndNode();
            }

            // Draw Links
            for (const auto& conn : m_CurrentGraph->GetConnections())
            {
                ed::LinkId linkId = m_Impl->GetOrCreateId(conn.ID);
                ed::PinId outId = m_Impl->GetOrCreateId(conn.OutputPinID);
                ed::PinId inId = m_Impl->GetOrCreateId(conn.InputPinID);
                ed::Link(linkId, outId, inId);
            }

            // Handle Link Creation
            if (ed::BeginCreate())
            {
                ed::PinId startPinId, endPinId;
                if (ed::QueryNewLink(&startPinId, &endPinId))
                {
                    if (startPinId && endPinId)
                    {
                        if (ed::AcceptNewItem())
                        {
                            const UUID startUuid = m_Impl->GetUuid((uintptr_t)startPinId);
                            const UUID endUuid = m_Impl->GetUuid((uintptr_t)endPinId);

                            Pin* startPin = nullptr;
                            Pin* endPin = nullptr;

                            for (const auto& [id, node] : m_CurrentGraph->GetNodes())
                            {
                                if (!startPin)
                                    startPin = node->FindPinByID(startUuid);
                                if (!endPin)
                                    endPin = node->FindPinByID(endUuid);
                            }

                            if (startPin && endPin)
                            {
                                if (startPin->GetDirection() == Pin::Direction::Input && endPin->GetDirection() == Pin::Direction::Output)
                                {
                                    std::swap(startPin, endPin);
                                }

                                if (startPin->GetDirection() == Pin::Direction::Output && endPin->GetDirection() == Pin::Direction::Input)
                                {
                                    const auto action = CreateRef<NodesConnectedAction>(m_CurrentGraph, startPin->GetID(), endPin->GetID());
                                    UndoRedo::Get().RegisterAction(action);
                                    m_CurrentGraph->ConnectByPinID(startPin->GetID(), endPin->GetID());
                                }
                            }
                        }
                    }
                }
            }
            ed::EndCreate();

            // Handle Node / Link Deletion
            if (ed::BeginDelete())
            {
                ed::LinkId deletedLinkId;
                while (ed::QueryDeletedLink(&deletedLinkId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        const UUID connId = m_Impl->GetUuid((uintptr_t)deletedLinkId);
                        if (!connId.Empty())
                        {
                            for (const auto& conn : m_CurrentGraph->GetConnections())
                            {
                                if (conn.ID == connId)
                                {
                                    const auto action = CreateRef<NodesDisconnectedAction>(m_CurrentGraph, conn.OutputPinID, conn.InputPinID);
                                    UndoRedo::Get().RegisterAction(action);
                                    m_CurrentGraph->Disconnect(connId);
                                    break;
                                }
                            }
                        }
                    }
                }

                ed::NodeId deletedNodeId;
                while (ed::QueryDeletedNode(&deletedNodeId))
                {
                    if (ed::AcceptDeletedItem())
                    {
                        const UUID nodeId = m_Impl->GetUuid((uintptr_t)deletedNodeId);
                        if (!nodeId.Empty())
                        {
                            const auto it = m_CurrentGraph->GetNodes().find(nodeId);
                            if (it != m_CurrentGraph->GetNodes().end())
                            {
                                const auto action = CreateRef<NodeRemovedAction>(m_CurrentGraph, it->second);
                                UndoRedo::Get().RegisterAction(action);
                                m_CurrentGraph->RemoveNode(nodeId);
                            }
                        }
                    }
                }
            }
            ed::EndDelete();
        }

        ed::End();
        ed::SetCurrentEditor(nullptr);
    }

    Vector<UUID> ImguiNodeEditorAdapter::GetSelectedNodes() const { return m_SelectedNodes; }

    void ImguiNodeEditorAdapter::RenderAddNodeMenu(const Ref<NodeGraph>& graph)
    {
        const auto& categories = NodeRegistry::Get().GetCategorizedTypes();
        for (const auto& [category, typeNames] : categories)
        {
            if (ImGui::BeginMenu(category.c_str()))
            {
                for (const auto& typeName : typeNames)
                {
                    if (ImGui::MenuItem(typeName.c_str()))
                    {
                        auto node = NodeRegistry::Get().Create(typeName);
                        if (node)
                        {
                            ed::SetCurrentEditor(m_Impl->Context);
                            const auto mousePos = ImGui::GetMousePos();
                            const auto canvasPos = ed::ScreenToCanvas(mousePos);

                            node->SetEditorPosition(glm::vec2(canvasPos.x, canvasPos.y));

                            const auto action = CreateRef<NodeAddedAction>(graph, node);
                            UndoRedo::Get().RegisterAction(action);
                            graph->AddNode(node);
                            m_NeedsSync = true;

                            ed::SetNodePosition(m_Impl->GetOrCreateId(node->GetID()), canvasPos);
                            ed::SetCurrentEditor(nullptr);
                        }
                    }
                }
                ImGui::EndMenu();
            }
        }
    }

} // namespace Crowny

#endif // CW_WITH_NODES