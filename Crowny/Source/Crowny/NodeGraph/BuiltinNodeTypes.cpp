#include "cwpch.h"

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"
#include "Crowny/NodeGraph/NodeRegistry.h"

// Include all built-in node type headers so their constructors are referenced below,
// which forces the linker to pull in those translation units from the static library.
#include "Crowny/NodeGraph/Nodes/GeometryNodes.h"
#include "Crowny/NodeGraph/Nodes/InputNodes.h"
#include "Crowny/NodeGraph/Nodes/MathNodes.h"
#include "Crowny/NodeGraph/Nodes/ModifierNodes.h"
#include "Crowny/NodeGraph/Nodes/OutputNodes.h"

namespace Crowny
{
    // This would ideally allow a macro to register them(similarly to the C# macro)... But someone decided to build static libs...
    void RegisterBuiltinNodeTypes()
    {
        auto& reg = NodeRegistry::Get();

        // Primitives
        reg.Register("BoxNode", "Primitives", [](UUID id) -> Ref<Node> { return CreateRef<BoxNode>(id); });
        reg.Register("SphereNode", "Primitives", [](UUID id) -> Ref<Node> { return CreateRef<SphereNode>(id); });
        reg.Register("PlaneNode", "Primitives", [](UUID id) -> Ref<Node> { return CreateRef<PlaneNode>(id); });
        reg.Register("GridNode", "Primitives", [](UUID id) -> Ref<Node> { return CreateRef<GridNode>(id); });

        // Modifiers
        reg.Register("TransformGeometryNode", "Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<TransformGeometryNode>(id); });
        reg.Register("MergeGeometryNode", "Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<MergeGeometryNode>(id); });
        reg.Register("NoiseDisplaceNode", "Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<NoiseDisplaceNode>(id); });
        reg.Register("RecalculateNormalsNode", "Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<RecalculateNormalsNode>(id); });
        reg.Register("SubdivideNode", "Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<SubdivideNode>(id); });

        // Input
        reg.Register("GraphInputNode", "Input", [](UUID id) -> Ref<Node> { return CreateRef<GraphInputNode>(id); });
        reg.Register("FloatNode", "Input", [](UUID id) -> Ref<Node> { return CreateRef<FloatNode>(id); });
        reg.Register("IntNode", "Input", [](UUID id) -> Ref<Node> { return CreateRef<IntNode>(id); });
        reg.Register("Vec3Node", "Input", [](UUID id) -> Ref<Node> { return CreateRef<Vec3Node>(id); });

        // Math
        reg.Register("AddNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<AddNode>(id); });
        reg.Register("MultiplyNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<MultiplyNode>(id); });
        reg.Register("RemapNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<RemapNode>(id); });
        reg.Register("SplitVec3Node", "Math", [](UUID id) -> Ref<Node> { return CreateRef<SplitVec3Node>(id); });

        // Output
        reg.Register("GeometryOutputNode", "Output", [](UUID id) -> Ref<Node> { return CreateRef<GeometryOutputNode>(id); });
    }

} // namespace Crowny
