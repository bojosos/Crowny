#include "cwpch.h"

#include "Crowny/NodeGraph/BuiltinNodeTypes.h"
#include "Crowny/NodeGraph/NodeRegistry.h"

// Include all built-in node type headers so their constructors are referenced below,
// which forces the linker to pull in those translation units from the static library.
#include "Crowny/NodeGraph/Nodes/GeometryNodes.h"
#include "Crowny/NodeGraph/Nodes/MathNodes.h"
#include "Crowny/NodeGraph/Nodes/ModifierNodes.h"
#include "Crowny/NodeGraph/Nodes/OutputNodes.h"

namespace Crowny
{
    void RegisterBuiltinNodeTypes()
    {
        auto& reg = NodeRegistry::Get();

        // Geometry / Primitives
        reg.Register("BoxNode", "Geometry/Primitives", [](UUID id) -> Ref<Node> { return CreateRef<BoxNode>(id); });
        reg.Register("SphereNode", "Geometry/Primitives", [](UUID id) -> Ref<Node> { return CreateRef<SphereNode>(id); });
        reg.Register("PlaneNode", "Geometry/Primitives", [](UUID id) -> Ref<Node> { return CreateRef<PlaneNode>(id); });
        reg.Register("GridNode", "Geometry/Primitives", [](UUID id) -> Ref<Node> { return CreateRef<GridNode>(id); });

        // Geometry / Modifiers
        reg.Register("TransformGeometryNode", "Geometry/Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<TransformGeometryNode>(id); });
        reg.Register("MergeGeometryNode", "Geometry/Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<MergeGeometryNode>(id); });
        reg.Register("NoiseDisplaceNode", "Geometry/Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<NoiseDisplaceNode>(id); });
        reg.Register("RecalculateNormalsNode", "Geometry/Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<RecalculateNormalsNode>(id); });
        reg.Register("SubdivideNode", "Geometry/Modifiers", [](UUID id) -> Ref<Node> { return CreateRef<SubdivideNode>(id); });

        // Math
        reg.Register("FloatNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<FloatNode>(id); });
        reg.Register("IntNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<IntNode>(id); });
        reg.Register("Vec3Node", "Math", [](UUID id) -> Ref<Node> { return CreateRef<Vec3Node>(id); });
        reg.Register("AddNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<AddNode>(id); });
        reg.Register("MultiplyNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<MultiplyNode>(id); });
        reg.Register("RemapNode", "Math", [](UUID id) -> Ref<Node> { return CreateRef<RemapNode>(id); });
        reg.Register("SplitVec3Node", "Math", [](UUID id) -> Ref<Node> { return CreateRef<SplitVec3Node>(id); });

        // Output
        reg.Register("GeometryOutputNode", "Output", [](UUID id) -> Ref<Node> { return CreateRef<GeometryOutputNode>(id); });
    }

} // namespace Crowny
