#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/NodeGraph/BuiltinNodeTypes.h"
#include "Crowny/NodeGraph/NodeGraph.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Nodes/GeometryNodes.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Nodes/MathNodes.h"
#include "Crowny/NodeGraph/Nodes/OutputNodes.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/NodeGraph/UnknownNode.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Serialization/NodeGraphSerializer.h"

using namespace Crowny;

namespace
{
    void RegisterNodesOnce()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc description;
            description.Name = "NodeGraphTests";
            description.Headless = true;
            description.WorkingDirectory = fs::current_path();
            Application::StartUp(description);
        }
        static const bool registered = [] {
            RegisterBuiltinNodeTypes();
            return true;
        }();
        (void)registered;
    }

    Ref<NodeGraph> MakeBoxGraph(Ref<BoxNode>& box, Ref<GeometryOutputNode>& output, UUID connectionId)
    {
        auto graph = CreateRef<NodeGraph>();
        box = CreateRef<BoxNode>(UuidGenerator::Generate());
        output = CreateRef<GeometryOutputNode>(UuidGenerator::Generate());
        REQUIRE(graph->AddNode(box));
        REQUIRE(graph->AddNode(output));
        REQUIRE(graph->ConnectByPinID(box->FindOutputPin("Geometry")->GetID(), output->FindInputPin("Geometry")->GetID(), connectionId));
        return graph;
    }
} // namespace

TEST_CASE("Node graph serialization preserves identity", "[NodeGraph]")
{
    RegisterNodesOnce();
    Ref<BoxNode> box;
    Ref<GeometryOutputNode> output;
    const UUID connectionId = UuidGenerator::Generate();
    Ref<NodeGraph> graph = MakeBoxGraph(box, output, connectionId);
    const UUID boxId = box->GetID();
    const UUID boxOutputId = box->FindOutputPin("Geometry")->GetID();
    const UUID outputInputId = output->FindInputPin("Geometry")->GetID();

    NodeGraphSerializer serializer(graph);
    const String firstYaml = serializer.SerializeToString();
    Ref<NodeGraph> loaded;
    NodeGraphSerializer loader(loaded);
    REQUIRE(loader.DeserializeFromString(firstYaml));
    REQUIRE(loaded->GetNode(boxId));
    REQUIRE(loaded->GetPin(boxOutputId));
    REQUIRE(loaded->GetPin(outputInputId));
    REQUIRE(loaded->GetConnection(connectionId));

    NodeGraphSerializer loadedSerializer(loaded);
    CHECK(loadedSerializer.SerializeToString() == firstYaml);
}

TEST_CASE("Node graph serialization publishes atomically", "[NodeGraph][Serialization]")
{
    RegisterNodesOnce();
    Ref<BoxNode> box;
    Ref<GeometryOutputNode> output;
    Ref<NodeGraph> graph = MakeBoxGraph(box, output, UuidGenerator::Generate());
    NodeGraphSerializer serializer(graph);

    const String uniqueName = UuidGenerator::Generate().ToString();
    const Path graphPath = fs::temp_directory_path() / ("crowny-node-graph-" + uniqueName + ".cwng");
    const Path blockedPath = fs::temp_directory_path() / ("crowny-node-graph-blocked-" + uniqueName);
    std::error_code filesystemError;
    fs::remove(graphPath, filesystemError);
    fs::remove_all(blockedPath, filesystemError);

    String writeError;
    REQUIRE(FileSystem::WriteTextFileAtomic(graphPath, "stale graph", &writeError));
    REQUIRE(serializer.Serialize(graphPath));
    const String publishedGraph = FileSystem::ReadTextFile(graphPath);
    CHECK(publishedGraph == serializer.SerializeToString());
    CHECK_FALSE(serializer.Serialize({}));

    REQUIRE(fs::create_directory(blockedPath));
    CHECK_FALSE(serializer.Serialize(blockedPath));
    CHECK(fs::is_directory(blockedPath));

    Ref<NodeGraph> nullGraph;
    NodeGraphSerializer nullSerializer(nullGraph);
    CHECK_FALSE(nullSerializer.Serialize(graphPath));
    CHECK(FileSystem::ReadTextFile(graphPath) == publishedGraph);

    fs::remove(graphPath, filesystemError);
    fs::remove_all(blockedPath, filesystemError);
}

TEST_CASE("Node graphs reject invalid connections and cycles", "[NodeGraph]")
{
    RegisterNodesOnce();
    auto graph = CreateRef<NodeGraph>();
    auto first = CreateRef<AddNode>(UuidGenerator::Generate());
    auto second = CreateRef<AddNode>(UuidGenerator::Generate());
    auto boolean = CreateRef<BoolNode>(UuidGenerator::Generate());
    auto scalar = CreateRef<FloatNode>(UuidGenerator::Generate());
    REQUIRE(graph->AddNode(first));
    REQUIRE(graph->AddNode(second));
    REQUIRE(graph->AddNode(boolean));
    REQUIRE(graph->AddNode(scalar));

    REQUIRE(graph->ConnectByPinID(first->FindOutputPin("Result")->GetID(), second->FindInputPin("A")->GetID()));
    CHECK_FALSE(graph->ConnectByPinID(second->FindOutputPin("Result")->GetID(), first->FindInputPin("A")->GetID()));
    CHECK_FALSE(graph->ConnectByPinID(boolean->FindOutputPin("Value")->GetID(), scalar->FindInputPin("Value")->GetID()));
}

TEST_CASE("Geometry evaluation cache follows semantic changes", "[NodeGraph]")
{
    RegisterNodesOnce();
    Ref<BoxNode> box;
    Ref<GeometryOutputNode> output;
    Ref<NodeGraph> graph = MakeBoxGraph(box, output, UuidGenerator::Generate());

    const Ref<MeshData> first = graph->EvaluateGeometry();
    REQUIRE(first);
    CHECK(graph->EvaluateGeometry().get() == first.get());

    box->SetEditorPosition({ 32.0f, 48.0f });
    CHECK(graph->EvaluateGeometry().get() == first.get());

    REQUIRE(box->FindInputPin("Width")->SetDefaultValue(2.0f));
    const Ref<MeshData> changed = graph->EvaluateGeometry();
    REQUIRE(changed);
    CHECK(changed.get() != first.get());
}

TEST_CASE("Parameterized node graph evaluation reuses scratch after warm-up", "[NodeGraph][Memory][Frame]")
{
    RegisterNodesOnce();
    auto graph = CreateRef<NodeGraph>();
    auto input = CreateRef<GraphInputNode>(UuidGenerator::Generate());
    auto output = CreateRef<GeometryOutputNode>(UuidGenerator::Generate());
    const Ref<MeshData> mesh = CreateRef<MeshData>();
    const Ref<MeshData> alternateMesh = CreateRef<MeshData>();
    const UUID inputId = graph->AddInput("Geometry", PinDataType::MeshData, mesh);
    REQUIRE_FALSE(inputId.Empty());
    input->SetInputID(inputId);
    REQUIRE(graph->AddNode(input));
    REQUIRE(graph->AddNode(output));
    REQUIRE(graph->ConnectByPinID(input->FindOutputPin("Value")->GetID(), output->FindInputPin("Geometry")->GetID()));

    UnorderedMap<UUID, PinValue> inputValues;
    inputValues.emplace(inputId, mesh);
    REQUIRE(graph->EvaluateGeometry(inputValues).get() == mesh.get());
    inputValues[inputId] = alternateMesh;
    REQUIRE(graph->EvaluateGeometry(inputValues).get() == alternateMesh.get());

    for (const uint32_t evaluationCount : { 1u, 1000u, 10000u })
    {
        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        const MeshData* result = nullptr;
        bool allResultsMatched = true;
        for (uint32_t evaluation = 0; evaluation < evaluationCount; evaluation++)
        {
            const Ref<MeshData>& expected = (evaluation & 1u) == 0u ? mesh : alternateMesh;
            inputValues[inputId] = expected;
            result = graph->EvaluateGeometry(inputValues).get();
            allResultsMatched = allResultsMatched && result == expected.get();
        }
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, Memory::GetThreadAllocationSnapshot());

        INFO("Evaluation count: " << evaluationCount);
        CHECK(result != nullptr);
        CHECK(allResultsMatched);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
}

TEST_CASE("Missing node types survive node graph round trips", "[NodeGraph]")
{
    RegisterNodesOnce();
    auto graph = CreateRef<NodeGraph>();
    auto missing = CreateRef<UnknownNode>(UuidGenerator::Generate(), "PluginExtrudeNode");
    const UUID inputId = UuidGenerator::Generate();
    const UUID outputId = UuidGenerator::Generate();
    missing->AddSerializedPin(inputId, "Distance", Pin::Direction::Input, PinDataType::Float, 1.0f);
    missing->AddSerializedPin(outputId, "Geometry", Pin::Direction::Output, PinDataType::MeshData, Ref<MeshData>());
    REQUIRE(graph->AddNode(missing));

    NodeGraphSerializer serializer(graph);
    Ref<NodeGraph> loaded;
    NodeGraphSerializer loader(loaded);
    REQUIRE(loader.DeserializeFromString(serializer.SerializeToString()));
    Node* loadedNode = loaded->GetNode(missing->GetID());
    REQUIRE(loadedNode);
    CHECK(loadedNode->GetTypeName() == "PluginExtrudeNode");
    CHECK(loadedNode->GetCategory() == "Missing");
    CHECK(loaded->GetPin(inputId));
    CHECK(loaded->GetPin(outputId));
}

TEST_CASE("Cylinder node produces bounded topology", "[NodeGraph]")
{
    RegisterNodesOnce();
    auto graph = CreateRef<NodeGraph>();
    auto cylinder = CreateRef<CylinderNode>(UuidGenerator::Generate());
    auto output = CreateRef<GeometryOutputNode>(UuidGenerator::Generate());
    REQUIRE(cylinder->FindInputPin("Segments")->SetDefaultValue(8));
    REQUIRE(graph->AddNode(cylinder));
    REQUIRE(graph->AddNode(output));
    REQUIRE(graph->ConnectByPinID(cylinder->FindOutputPin("Geometry")->GetID(), output->FindInputPin("Geometry")->GetID()));

    const Ref<MeshData> mesh = graph->EvaluateGeometry();
    REQUIRE(mesh);
    CHECK(mesh->GetVertexCount() == 38);
    CHECK(mesh->GetIndexCount() == 96);
}
