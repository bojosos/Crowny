#include "cwpch.h"

#include "Crowny/Serialization/NodeGraphSerializer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Pin.h"

namespace Crowny
{
    static void SerializePinValueYAML(YAML::Emitter& out, const PinValue& val, PinDataType type)
    {
        switch (type)
        {
        case PinDataType::Float:
            out << std::get<float>(val);
            break;
        case PinDataType::Int:
            out << std::get<int32_t>(val);
            break;
        case PinDataType::Vec2:
            out << std::get<glm::vec2>(val);
            break;
        case PinDataType::Vec3:
            out << std::get<glm::vec3>(val);
            break;
        case PinDataType::Vec4:
            out << std::get<glm::vec4>(val);
            break;
        case PinDataType::Bool:
            out << std::get<bool>(val);
            break;
        default:
            out << 0.0f;
            break;
        }
    }

    static void DeserializePinValueYAML(const YAML::Node& node, PinDataType type, PinValue& outVal)
    {
        if (!node)
            return;
        switch (type)
        {
        case PinDataType::Float:
            outVal = node.as<float>(0.0f);
            break;
        case PinDataType::Int:
            outVal = node.as<int32_t>(0);
            break;
        case PinDataType::Vec2:
            outVal = node.as<glm::vec2>(glm::vec2(0.0f));
            break;
        case PinDataType::Vec3:
            outVal = node.as<glm::vec3>(glm::vec3(0.0f));
            break;
        case PinDataType::Vec4:
            outVal = node.as<glm::vec4>(glm::vec4(0.0f));
            break;
        case PinDataType::Bool:
            outVal = node.as<bool>(false);
            break;
        default:
            break;
        }
    }

    NodeGraphSerializer::NodeGraphSerializer(Ref<NodeGraph>& graph) : m_Graph(graph) {}

    String NodeGraphSerializer::SerializeToString()
    {
        YAML::Emitter out;
        out << YAML::Comment("Crowny NodeGraphAsset");

        out << YAML::BeginMap;

        SerializeValueYAML(out, "Version", (uint32_t)1);
        auto graph = m_Graph;
        if (graph)
        {
            SerializeValueYAML(out, "GraphID", graph->GetID());
            SerializeValueYAML(out, "GraphName", graph->GetName());
            SerializeEnumYAML(out, "Domain", graph->GetDomain());

            out << YAML::Key << "Inputs" << YAML::Value << YAML::BeginSeq;
            for (const auto& input : graph->GetInputs())
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", input.ID);
                SerializeValueYAML(out, "Name", input.Name);
                SerializeEnumYAML(out, "DataType", input.DataType);
                out << YAML::Key << "DefaultValue";
                SerializePinValueYAML(out, input.DefaultValue, input.DataType);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
            for (const auto& [nodeID, node] : graph->GetNodes())
            {
                if (!node)
                    continue;
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", node->GetID());
                SerializeValueYAML(out, "TypeName", node->GetTypeName());
                SerializeValueYAML(out, "EditorPosition", node->GetEditorPosition());

                if (node->GetTypeName() == "GraphInputNode")
                    SerializeValueYAML(out, "InputID", static_cast<GraphInputNode*>(node.get())->GetInputID());

                out << YAML::Key << "Pins" << YAML::Value << YAML::BeginSeq;
                for (const auto& pin : node->GetInputPins())
                {
                    out << YAML::BeginMap;
                    SerializeValueYAML(out, "ID", pin->GetID());
                    SerializeValueYAML(out, "Name", pin->GetName());
                    SerializeEnumYAML(out, "Direction", pin->GetDirection());
                    SerializeEnumYAML(out, "DataType", pin->GetDataType());
                    out << YAML::Key << "DefaultValue";
                    SerializePinValueYAML(out, pin->GetDefaultValue(), pin->GetDataType());
                    out << YAML::EndMap;
                }
                for (const auto& pin : node->GetOutputPins())
                {
                    out << YAML::BeginMap;
                    SerializeValueYAML(out, "ID", pin->GetID());
                    SerializeValueYAML(out, "Name", pin->GetName());
                    SerializeEnumYAML(out, "Direction", pin->GetDirection());
                    SerializeEnumYAML(out, "DataType", pin->GetDataType());
                    out << YAML::Key << "DefaultValue";
                    SerializePinValueYAML(out, pin->GetDefaultValue(), pin->GetDataType());
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq; // Pins

                out << YAML::EndMap; // Node
            }
            out << YAML::EndSeq; // Nodes

            out << YAML::Key << "Connections" << YAML::Value << YAML::BeginSeq;
            for (const auto& conn : graph->GetConnections())
            {
                out << YAML::BeginMap;
                SerializeValueYAML(out, "ID", conn.ID);
                SerializeValueYAML(out, "OutputNodeID", conn.OutputNodeID);
                SerializeValueYAML(out, "OutputPinID", conn.OutputPinID);
                SerializeValueYAML(out, "InputNodeID", conn.InputNodeID);
                SerializeValueYAML(out, "InputPinID", conn.InputPinID);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq; // Connections
        }

        out << YAML::EndMap;

        return String(out.c_str());
    }

    void NodeGraphSerializer::Serialize(const Path& filepath)
    {
        const String str = SerializeToString();
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        stream->Write(str.c_str(), str.size());
        stream->Close();
    }

    void NodeGraphSerializer::DeserializeFromString(const String& yamlString)
    {
        if (yamlString.empty())
            return;

        YAML::Node data;
        try
        {
            data = YAML::Load(yamlString);
        }
        catch (const std::exception& e)
        {
            CW_ENGINE_ERROR("Failed to parse NodeGraph YAML: {0}", e.what());
            return;
        }

        if (!data || !data.IsMap() || !data["GraphID"])
            return;

        UUID graphID;
        DeserializeValueYAML(data, "GraphID", graphID, UUID::EMPTY);
        Ref<NodeGraph> graph = CreateRef<NodeGraph>(graphID);

        if (data["GraphName"])
            graph->SetName(data["GraphName"].as<String>());

        if (data["Domain"])
            graph->SetDomain((NodeGraph::Domain)data["Domain"].as<uint32_t>());

        const YAML::Node& inputs = data["Inputs"];
        if (inputs)
        {
            for (auto inputYAML : inputs)
            {
                UUID id = inputYAML["ID"].as<UUID>();
                String name = inputYAML["Name"].as<String>();
                PinDataType type = (PinDataType)inputYAML["DataType"].as<uint32_t>();
                PinValue val = DefaultPinValue(type);
                DeserializePinValueYAML(inputYAML["DefaultValue"], type, val);

                GraphInput input = { id, name, type, val };
                graph->m_Inputs.push_back(input);
            }
        }

        const YAML::Node& nodes = data["Nodes"];
        if (nodes)
        {
            for (auto nodeYAML : nodes)
            {
                UUID nodeID = nodeYAML["ID"].as<UUID>();
                StringID typeName = nodeYAML["TypeName"].as<StringID>();
                glm::vec2 editorPos = nodeYAML["EditorPosition"].as<glm::vec2>();

                Ref<Node> node = NodeRegistry::Get().Create(typeName, nodeID);
                if (!node)
                {
                    CW_ENGINE_WARN("Failed to create node type '{0}' during deserialization.", typeName.c_str());
                    continue;
                }
                node->SetEditorPosition(editorPos);

                if (typeName == "GraphInputNode" && nodeYAML["InputID"])
                    static_cast<GraphInputNode*>(node.get())->SetInputID(nodeYAML["InputID"].as<UUID>());

                const YAML::Node& pins = nodeYAML["Pins"];
                if (pins)
                {
                    for (auto pinYAML : pins)
                    {
                        UUID pinID = pinYAML["ID"].as<UUID>();
                        String name = pinYAML["Name"].as<String>();
                        Pin::Direction dir = (Pin::Direction)pinYAML["Direction"].as<uint32_t>();
                        PinDataType dataType = (PinDataType)pinYAML["DataType"].as<uint32_t>();

                        Pin* pin = dir == Pin::Direction::Input ? node->FindInputPin(name) : node->FindOutputPin(name);
                        if (pin && pin->GetDataType() == dataType)
                        {
                            pin->SetID(pinID);
                            if (dir == Pin::Direction::Input && pinYAML["DefaultValue"])
                            {
                                PinValue val = pin->GetDefaultValue();
                                DeserializePinValueYAML(pinYAML["DefaultValue"], dataType, val);
                                pin->SetDefaultValue(val);
                            }
                        }
                    }
                }
                graph->AddNode(node);
            }
        }

        const YAML::Node& connections = data["Connections"];
        if (connections)
        {
            for (auto connYAML : connections)
            {
                Connection conn;
                conn.ID = connYAML["ID"].as<UUID>();
                conn.OutputNodeID = connYAML["OutputNodeID"].as<UUID>();
                conn.OutputPinID = connYAML["OutputPinID"].as<UUID>();
                conn.InputNodeID = connYAML["InputNodeID"].as<UUID>();
                conn.InputPinID = connYAML["InputPinID"].as<UUID>();

                Pin* outputPin = graph->FindPinByID(conn.OutputPinID);
                Pin* inputPin = graph->FindPinByID(conn.InputPinID);

                if (outputPin && inputPin)
                {
                    inputPin->SetConnectedPin(outputPin);
                    graph->m_Connections.push_back(conn);
                }
            }
        }

        m_Graph = graph;
    }

    void NodeGraphSerializer::Deserialize(const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (!stream)
        {
            CW_ENGINE_ERROR("Failed to open NodeGraph file: {0}", filepath);
            return;
        }
        String text = stream->GetAsString();
        DeserializeFromString(text);
    }

} // namespace Crowny
