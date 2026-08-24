#include "cwpch.h"

#include "Crowny/Serialization/NodeGraphSerializer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/NodeGraph/BuiltinNodeTypes.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/NodeGraph/UnknownNode.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t NODE_GRAPH_VERSION = 1;

        void SerializePinValueYAML(YAML::Emitter& out, const PinValue& value, PinDataType type)
        {
            PinValue converted;
            if (type != PinDataType::Any && !ConvertPinValue(value, type, converted))
                converted = DefaultPinValue(type);
            else if (type == PinDataType::Any)
                converted = value;

            std::visit(
              [&out](const auto& current) {
                  using T = std::decay_t<decltype(current)>;
                  if constexpr (std::is_same_v<T, Ref<MeshData>>)
                      out << 0.0f;
                  else
                      out << current;
              },
              converted);
        }

        PinValue DeserializePinValueYAML(const YAML::Node& node, PinDataType type)
        {
            if (!node)
                return DefaultPinValue(type);
            switch (type)
            {
            case PinDataType::Float:
                return node.as<float>();
            case PinDataType::Int:
                return node.as<int32_t>();
            case PinDataType::Vec2:
                return node.as<glm::vec2>();
            case PinDataType::Vec3:
                return node.as<glm::vec3>();
            case PinDataType::Vec4:
                return node.as<glm::vec4>();
            case PinDataType::Bool:
                return node.as<bool>();
            case PinDataType::Any:
                return node.as<float>(0.0f);
            case PinDataType::MeshData:
                return Ref<MeshData>();
            }
            return 0.0f;
        }

        bool IsDirectionValid(Pin::Direction direction) { return direction == Pin::Direction::Input || direction == Pin::Direction::Output; }
    } // namespace

    NodeGraphSerializer::NodeGraphSerializer(Ref<NodeGraph>& graph) : m_Graph(graph) {}

    String NodeGraphSerializer::SerializeToString()
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny NodeGraphAsset") << YAML::BeginMap;
        SerializeValueYAML(out, "Version", NODE_GRAPH_VERSION);

        if (m_Graph)
        {
            SerializeValueYAML(out, "GraphID", m_Graph->GetID());
            SerializeValueYAML(out, "GraphName", m_Graph->GetName());
            SerializeEnumYAML(out, "Domain", m_Graph->GetDomain());

            Vector<const GraphInput*> inputs;
            for (const GraphInput& input : m_Graph->GetInputs())
                inputs.push_back(&input);
            std::sort(inputs.begin(), inputs.end(), [](const GraphInput* lhs, const GraphInput* rhs) { return lhs->ID < rhs->ID; });
            out << YAML::Key << "Inputs" << YAML::Value << YAML::BeginSeq;
            for (const GraphInput* input : inputs)
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", input->ID);
                SerializeValueYAML(out, "Name", input->Name);
                SerializeEnumYAML(out, "DataType", input->DataType);
                out << YAML::Key << "DefaultValue" << YAML::Value;
                SerializePinValueYAML(out, input->DefaultValue, input->DataType);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            Vector<const Node*> nodes;
            for (const auto& [id, node] : m_Graph->GetNodes())
                if (node)
                    nodes.push_back(node.get());
            std::sort(nodes.begin(), nodes.end(), [](const Node* lhs, const Node* rhs) { return lhs->GetID() < rhs->GetID(); });
            out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
            for (const Node* node : nodes)
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", node->GetID());
                SerializeValueYAML(out, "TypeName", node->GetTypeName());
                SerializeValueYAML(out, "EditorPosition", node->GetEditorPosition());
                if (node->GetTypeName() == "GraphInputNode"_sid)
                    SerializeValueYAML(out, "InputID", static_cast<const GraphInputNode*>(node)->GetInputID());

                out << YAML::Key << "Pins" << YAML::Value << YAML::BeginSeq;
                const auto serializePins = [&](const auto& pins) {
                    for (const Ref<Pin>& pin : pins)
                    {
                        out << YAML::BeginMap;
                        SerializeValueYAML(out, "ID", pin->GetID());
                        SerializeValueYAML(out, "Name", pin->GetName());
                        SerializeEnumYAML(out, "Direction", pin->GetDirection());
                        SerializeEnumYAML(out, "DataType", pin->GetDataType());
                        out << YAML::Key << "DefaultValue" << YAML::Value;
                        SerializePinValueYAML(out, pin->GetDefaultValue(), pin->GetDataType());
                        out << YAML::EndMap;
                    }
                };
                serializePins(node->GetInputPins());
                serializePins(node->GetOutputPins());
                out << YAML::EndSeq << YAML::EndMap;
            }
            out << YAML::EndSeq;

            Vector<const Connection*> connections;
            for (const Connection& connection : m_Graph->GetConnections())
                connections.push_back(&connection);
            std::sort(connections.begin(), connections.end(), [](const Connection* lhs, const Connection* rhs) { return lhs->ID < rhs->ID; });
            out << YAML::Key << "Connections" << YAML::Value << YAML::BeginSeq;
            for (const Connection* connection : connections)
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", connection->ID);
                SerializeValueYAML(out, "OutputNodeID", connection->OutputNodeID);
                SerializeValueYAML(out, "OutputPinID", connection->OutputPinID);
                SerializeValueYAML(out, "InputNodeID", connection->InputNodeID);
                SerializeValueYAML(out, "InputPinID", connection->InputPinID);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
        }
        out << YAML::EndMap;
        return String(out.c_str());
    }

    bool NodeGraphSerializer::Serialize(const Path& filepath)
    {
        if (!m_Graph || filepath.empty())
            return false;
        const String text = SerializeToString();
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        if (!stream)
            return false;
        const bool success = stream->Write(text.data(), text.size()) == text.size();
        stream->Close();
        return success;
    }

    bool NodeGraphSerializer::DeserializeFromString(const String& yamlString)
    {
        if (yamlString.empty())
            return false;

        try
        {
            RegisterBuiltinNodeTypes();
            const YAML::Node data = YAML::Load(yamlString);
            if (!data || !data.IsMap() || !data["GraphID"])
                return false;
            const uint32_t version = data["Version"].as<uint32_t>(NODE_GRAPH_VERSION);
            if (version > NODE_GRAPH_VERSION)
                CW_ENGINE_WARN("Node graph version {0} is newer than supported version {1}.", version, NODE_GRAPH_VERSION);

            const UUID graphId = data["GraphID"].as<UUID>();
            if (graphId.Empty())
                return false;
            Ref<NodeGraph> graph = CreateRef<NodeGraph>(graphId);
            graph->m_Name = data["GraphName"].as<String>("Untitled Graph");
            const auto domain = static_cast<NodeGraph::Domain>(data["Domain"].as<uint32_t>(0));
            if (static_cast<uint32_t>(domain) > static_cast<uint32_t>(NodeGraph::Domain::Script))
                return false;
            graph->m_Domain = domain;

            if (const YAML::Node inputs = data["Inputs"])
            {
                UnorderedSet<UUID> inputIds;
                UnorderedSet<StringID> inputNames;
                for (const YAML::Node& inputYaml : inputs)
                {
                    GraphInput input;
                    input.ID = inputYaml["ID"].as<UUID>();
                    input.Name = inputYaml["Name"].as<StringID>();
                    input.DataType = static_cast<PinDataType>(inputYaml["DataType"].as<uint32_t>());
                    if (input.ID.Empty() || input.Name.IsEmpty() || !inputIds.insert(input.ID).second || !inputNames.insert(input.Name).second ||
                        !IsPinDataTypeValid(input.DataType))
                        return false;
                    input.DefaultValue = DeserializePinValueYAML(inputYaml["DefaultValue"], input.DataType);
                    graph->m_Inputs.push_back(std::move(input));
                }
            }

            if (const YAML::Node nodes = data["Nodes"])
            {
                for (const YAML::Node& nodeYaml : nodes)
                {
                    const UUID nodeId = nodeYaml["ID"].as<UUID>();
                    const StringID typeName = nodeYaml["TypeName"].as<StringID>();
                    if (nodeId.Empty() || typeName.IsEmpty())
                        return false;
                    const bool knownType = NodeRegistry::Get().HasType(typeName);
                    Ref<Node> node;
                    if (knownType)
                        node = NodeRegistry::Get().Create(typeName, nodeId);
                    else
                        node = CreateRef<UnknownNode>(nodeId, typeName);
                    node->SetEditorPosition(nodeYaml["EditorPosition"].as<glm::vec2>(glm::vec2(0.0f)));
                    if (knownType && typeName == "GraphInputNode"_sid && nodeYaml["InputID"])
                        static_cast<GraphInputNode*>(node.get())->SetInputID(nodeYaml["InputID"].as<UUID>());

                    if (const YAML::Node pins = nodeYaml["Pins"])
                    {
                        for (const YAML::Node& pinYaml : pins)
                        {
                            const UUID pinId = pinYaml["ID"].as<UUID>();
                            const StringID name = pinYaml["Name"].as<StringID>();
                            const auto direction = static_cast<Pin::Direction>(pinYaml["Direction"].as<uint32_t>());
                            const auto type = static_cast<PinDataType>(pinYaml["DataType"].as<uint32_t>());
                            if (pinId.Empty() || name.IsEmpty() || !IsDirectionValid(direction) || !IsPinDataTypeValid(type))
                                return false;
                            const PinValue defaultValue = DeserializePinValueYAML(pinYaml["DefaultValue"], type);
                            if (!knownType)
                            {
                                static_cast<UnknownNode*>(node.get())->AddSerializedPin(pinId, name, direction, type, defaultValue);
                                continue;
                            }
                            Pin* pin = direction == Pin::Direction::Input ? node->FindInputPin(name) : node->FindOutputPin(name);
                            if (!pin || pin->GetDataType() != type)
                            {
                                CW_ENGINE_WARN("Ignoring obsolete pin '{0}' on node type '{1}'.", name.c_str(), typeName.c_str());
                                continue;
                            }
                            pin->m_ID = pinId;
                            if (direction == Pin::Direction::Input)
                                pin->m_DefaultValue = defaultValue;
                        }
                    }
                    if (!graph->AddNode(std::move(node)))
                        return false;
                }
            }

            if (const YAML::Node connections = data["Connections"])
            {
                for (const YAML::Node& connectionYaml : connections)
                {
                    const UUID id = connectionYaml["ID"].as<UUID>();
                    const UUID outputPinId = connectionYaml["OutputPinID"].as<UUID>();
                    const UUID inputPinId = connectionYaml["InputPinID"].as<UUID>();
                    if (!graph->ConnectByPinID(outputPinId, inputPinId, id))
                        CW_ENGINE_WARN("Ignoring invalid node graph connection '{0}'.", id);
                }
            }

            graph->m_Version = 0;
            graph->m_EvaluationVersion = 0;
            graph->InvalidateEvaluation();
            m_Graph = std::move(graph);
            return true;
        }
        catch (const std::exception& exception)
        {
            CW_ENGINE_ERROR("Failed to deserialize node graph: {0}", exception.what());
            return false;
        }
    }

    bool NodeGraphSerializer::Deserialize(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (!stream)
        {
            CW_ENGINE_ERROR("Failed to open node graph file: {0}", filepath);
            return false;
        }
        const String text = stream->GetAsString();
        stream->Close();
        return DeserializeFromString(text);
    }
} // namespace Crowny
