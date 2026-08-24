#include "cwepch.h"

#ifdef CW_WITH_NODES

#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Panels/NodeEditor/ImNodeFlowAdapter.h"

#include "Panels/NodeEditor/NodeEditorActions.h"
#include "Panels/NodeEditor/NodeEditorPalette.h"
#include <ImNodeFlow.h>
#include <imgui.h>

#include "Crowny/NodeGraph/Nodes/InputNodes.h"

namespace Crowny
{
    // Wrapper node that bridges Crowny's Node to ImNodeFlow's BaseNode
    class VisualNode : public ImFlow::BaseNode
    {
    public:
        VisualNode(Node* coreNode, NodeGraph* graph) : m_CoreNode(coreNode), m_Graph(graph)
        {
            setTitle(coreNode->GetDisplayName().c_str());
            setStyle(ImFlow::NodeStyle::cyan());

            // Create input pins
            for (const auto& pin : coreNode->GetInputPins())
            {
                auto filter = [](ImFlow::Pin*, ImFlow::Pin*) { return true; };
                switch (pin->GetDataType())
                {
                case PinDataType::Float:
                    addIN<float>(pin->GetName().c_str(), 0.0f, filter, ImFlow::PinStyle::cyan());
                    break;
                case PinDataType::Int:
                    addIN<int>(pin->GetName().c_str(), 0, filter, ImFlow::PinStyle::green());
                    break;
                case PinDataType::Vec3:
                    addIN<float>(pin->GetName().c_str(), 0.0f, filter, ImFlow::PinStyle::blue());
                    break;
                case PinDataType::MeshData:
                    addIN<float>(pin->GetName().c_str(), 0.0f, filter, ImFlow::PinStyle::brown());
                    break;
                default:
                    addIN<float>(pin->GetName().c_str(), 0.0f, filter, ImFlow::PinStyle::white());
                    break;
                }
            }

            // Create output pins
            for (const auto& pin : coreNode->GetOutputPins())
            {
                switch (pin->GetDataType())
                {
                case PinDataType::Float:
                    addOUT<float>(pin->GetName().c_str(), ImFlow::PinStyle::cyan())->behaviour([]() { return 0.0f; });
                    break;
                case PinDataType::Int:
                    addOUT<int>(pin->GetName().c_str(), ImFlow::PinStyle::green())->behaviour([]() { return 0; });
                    break;
                case PinDataType::Vec3:
                    addOUT<float>(pin->GetName().c_str(), ImFlow::PinStyle::blue())->behaviour([]() { return 0.0f; });
                    break;
                case PinDataType::MeshData:
                    addOUT<float>(pin->GetName().c_str(), ImFlow::PinStyle::brown())->behaviour([]() { return 0.0f; });
                    break;
                default:
                    addOUT<float>(pin->GetName().c_str(), ImFlow::PinStyle::white())->behaviour([]() { return 0.0f; });
                    break;
                }
            }
        }

        void draw() override
        {
            if (m_CoreNode)
            {
                ImGui::TextUnformatted(m_CoreNode->GetTypeName().c_str());
                if (m_CoreNode->GetTypeName() == "GraphInputNode"_sid)
                {
                    GraphInputNode* inputNode = static_cast<GraphInputNode*>(m_CoreNode);
                    const auto& inputs = m_Graph->GetInputs();
                    int selected = -1;
                    for (int i = 0; i < (int)inputs.size(); i++)
                    {
                        if (inputs[i].ID == inputNode->GetInputID())
                        {
                            selected = i;
                            break;
                        }
                    }

                    if (ImGui::BeginCombo("Input", selected >= 0 ? inputs[selected].Name.c_str() : "None"))
                    {
                        for (int i = 0; i < (int)inputs.size(); i++)
                        {
                            if (ImGui::Selectable(inputs[i].Name.c_str(), selected == i))
                                inputNode->SetInputID(inputs[i].ID);
                        }
                        ImGui::EndCombo();
                    }
                }
            }
        }

        Node* GetCoreNode() const { return m_CoreNode; }
        UUID GetCoreNodeID() const { return m_CoreNode ? m_CoreNode->GetID() : UUID::EMPTY; }
        void ClearCoreNode() { m_CoreNode = nullptr; }

    private:
        Node* m_CoreNode;
        NodeGraph* m_Graph;
    };

    struct ImNodeFlowAdapter::Impl
    {
        ImFlow::ImNodeFlow Flow{ "NodeEditor" };
        UnorderedMap<UUID, std::shared_ptr<VisualNode>> NodeMap;
    };

    ImNodeFlowAdapter::ImNodeFlowAdapter() : m_Impl(CreateScope<Impl>()) {}

    ImNodeFlowAdapter::~ImNodeFlowAdapter() = default;

    void ImNodeFlowAdapter::SyncFromGraph(const Ref<NodeGraph>& graph)
    {
        auto& flow = m_Impl->Flow;
        auto& nodeMap = m_Impl->NodeMap;

        // Add nodes
        for (const auto& [id, node] : graph->GetNodes())
        {
            if (nodeMap.find(id) == nodeMap.end())
            {
                const auto pos = node->GetEditorPosition();
                const auto visualNode = flow.addNode<VisualNode>(ImVec2(pos.x, pos.y), node.get(), graph.get());
                nodeMap[id] = visualNode;
            }
        }

        // Remove nodes
        for (auto it = nodeMap.begin(); it != nodeMap.end();)
        {
            if (graph->GetNode(it->first) == nullptr)
            {
                it->second->ClearCoreNode();
                it->second->destroy();
                it = nodeMap.erase(it);
            }
            else
                ++it;
        }

        // Sync connections from core to visual
        auto& visualLinks = flow.getLinks();
        std::vector<ImFlow::Pin*> linksToDelete;

        // 1. Remove visual links that don't exist in core
        for (const auto& weakLink : visualLinks)
        {
            if (auto link = weakLink.lock())
            {
                ImFlow::Pin* const left = link->left();
                ImFlow::Pin* const right = link->right();

                auto* const leftNode = static_cast<VisualNode*>(left->getParent());
                auto* const rightNode = static_cast<VisualNode*>(right->getParent());

                Node* const outCore = leftNode ? leftNode->GetCoreNode() : nullptr;
                Node* const inCore = rightNode ? rightNode->GetCoreNode() : nullptr;

                if (outCore && inCore)
                {
                    Pin* const outPin = outCore->FindOutputPin(left->getName());
                    Pin* const inPin = inCore->FindInputPin(right->getName());

                    if (outPin && inPin)
                    {
                        bool exists = false;
                        for (const auto& conn : graph->GetConnections())
                        {
                            if (conn.OutputPinID == outPin->GetID() && conn.InputPinID == inPin->GetID())
                            {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists)
                            linksToDelete.push_back(right);
                    }
                }
            }
        }
        for (auto pin : linksToDelete)
            pin->deleteLink();

        // 2. Add visual links that exist in core but not in visual
        for (const auto& conn : graph->GetConnections())
        {
            const auto outNodeIt = nodeMap.find(conn.OutputNodeID);
            const auto inNodeIt = nodeMap.find(conn.InputNodeID);
            if (outNodeIt != nodeMap.end() && inNodeIt != nodeMap.end())
            {
                Node* const outCore = graph->GetNode(conn.OutputNodeID);
                Node* const inCore = graph->GetNode(conn.InputNodeID);
                if (outCore && inCore)
                {
                    Pin* const outPin = outCore->FindPinByID(conn.OutputPinID);
                    Pin* const inPin = inCore->FindPinByID(conn.InputPinID);
                    if (outPin && inPin)
                    {
                        ImFlow::Pin* const visOutPin = outNodeIt->second->outPin(outPin->GetName().c_str());
                        ImFlow::Pin* const visInPin = inNodeIt->second->inPin(inPin->GetName().c_str());

                        if (visOutPin && visInPin)
                        {
                            if (visInPin->getLink().expired())
                                visOutPin->createLink(visInPin);
                        }
                    }
                }
            }
        }

        m_NeedsSync = false;
    }

    void ImNodeFlowAdapter::SyncToGraph(const Ref<NodeGraph>& graph)
    {
        // Sync positions back to core nodes
        for (auto& [id, visualNode] : m_Impl->NodeMap)
        {
            Node* coreNode = graph->GetNode(id);
            if (coreNode)
            {
                const auto pos = visualNode->getPos();
                coreNode->SetEditorPosition(glm::vec2(pos.x, pos.y));
            }
        }

        // Update selected node
        m_SelectedNodeID = UUID::EMPTY;
        for (auto& [id, visualNode] : m_Impl->NodeMap)
        {
            if (visualNode->isSelected())
            {
                m_SelectedNodeID = id;
                break;
            }
        }

        // Diff connections from visual to core (user actions)
        auto& visualLinks = m_Impl->Flow.getLinks();

        // 1. Detect new visual links (NodesConnectedAction)
        for (const auto& weakLink : visualLinks)
        {
            if (auto link = weakLink.lock())
            {
                ImFlow::Pin* const left = link->left();
                ImFlow::Pin* const right = link->right();

                auto* const leftNode = static_cast<VisualNode*>(left->getParent());
                auto* const rightNode = static_cast<VisualNode*>(right->getParent());

                Node* const outCore = leftNode ? leftNode->GetCoreNode() : nullptr;
                Node* const inCore = rightNode ? rightNode->GetCoreNode() : nullptr;

                if (outCore && inCore)
                {
                    Pin* const outPin = outCore->FindOutputPin(left->getName());
                    Pin* const inPin = inCore->FindInputPin(right->getName());

                    if (outPin && inPin)
                    {
                        bool exists = false;
                        for (const auto& conn : graph->GetConnections())
                        {
                            if (conn.OutputPinID == outPin->GetID() && conn.InputPinID == inPin->GetID())
                            {
                                exists = true;
                                break;
                            }
                        }

                        if (!exists)
                        {
                            const auto action = CreateRef<NodesConnectedAction>(graph, outPin->GetID(), inPin->GetID());
                            action->Commit();
                            UndoRedo::Get().RegisterAction(action);
                        }
                    }
                }
            }
        }

        // 2. Detect removed visual links (NodesDisconnectedAction)
        std::vector<UUID> connectionsToRemove;
        for (const auto& conn : graph->GetConnections())
        {
            bool existsVisually = false;
            for (const auto& weakLink : visualLinks)
            {
                if (auto link = weakLink.lock())
                {
                    auto* const leftNode = static_cast<VisualNode*>(link->left()->getParent());
                    auto* const rightNode = static_cast<VisualNode*>(link->right()->getParent());

                    if (leftNode && rightNode)
                    {
                        Node* const outCore = leftNode->GetCoreNode();
                        Node* const inCore = rightNode->GetCoreNode();
                        if (outCore && inCore)
                        {
                            Pin* const outPin = outCore->FindOutputPin(link->left()->getName());
                            Pin* const inPin = inCore->FindInputPin(link->right()->getName());

                            if (outPin && inPin && outPin->GetID() == conn.OutputPinID && inPin->GetID() == conn.InputPinID)
                            {
                                existsVisually = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (!existsVisually)
                connectionsToRemove.push_back(conn.ID);
        }

        for (UUID connId : connectionsToRemove)
        {
            // Find the connection so we can register the undo action
            for (const auto& conn : graph->GetConnections())
            {
                if (conn.ID == connId)
                {
                    const auto action = CreateRef<NodesDisconnectedAction>(graph, conn);
                    action->Commit();
                    UndoRedo::Get().RegisterAction(action);
                    break;
                }
            }
        }
    }

    void ImNodeFlowAdapter::Render() { m_Impl->Flow.update(); }

    Vector<UUID> ImNodeFlowAdapter::GetSelectedNodes() const
    {
        Vector<UUID> selected;
        for (const auto& [id, visualNode] : m_Impl->NodeMap)
        {
            if (visualNode->isSelected())
                selected.push_back(id);
        }
        return selected;
    }

    void ImNodeFlowAdapter::RenderAddNodeMenu(const Ref<NodeGraph>& graph)
    {
        if (ImGui::IsWindowAppearing())
        {
            const auto mousePos = ImGui::GetMousePos();
            const auto gridPos = m_Impl->Flow.getPos();
            m_AddNodePosition = glm::vec2(mousePos.x - gridPos.x, mousePos.y - gridPos.y);
        }

        RenderNodePalette(m_AddNodeSearch, m_FocusAddNodeSearch, [this, &graph](StringID typeName) {
            auto node = NodeRegistry::Get().Create(typeName);
            if (node)
            {
                node->SetEditorPosition(m_AddNodePosition);

                const auto action = CreateRef<NodeAddedAction>(graph, node);
                action->Commit();
                UndoRedo::Get().RegisterAction(action);
                m_NeedsSync = true;
            }
        });
    }

} // namespace Crowny

#endif // CW_WITH_NODES
