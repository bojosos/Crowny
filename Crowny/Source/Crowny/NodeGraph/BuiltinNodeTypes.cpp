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
    using namespace Literals;

    // This would ideally allow a macro to register them(similarly to the C# macro)... But someone decided to build static libs...
    void RegisterBuiltinNodeTypes()
    {
        auto& reg = NodeRegistry::Get();

        // Primitives
        reg.Register("BoxNode"_sid, "Primitives"_sid, [](UUID id) -> Ref<Node> { return CreateRef<BoxNode>(id); });
        reg.Register("SphereNode"_sid, "Primitives"_sid, [](UUID id) -> Ref<Node> { return CreateRef<SphereNode>(id); });
        reg.Register("CylinderNode"_sid, "Primitives"_sid, [](UUID id) -> Ref<Node> { return CreateRef<CylinderNode>(id); });
        reg.Register("PlaneNode"_sid, "Primitives"_sid, [](UUID id) -> Ref<Node> { return CreateRef<PlaneNode>(id); });
        reg.Register("GridNode"_sid, "Primitives"_sid, [](UUID id) -> Ref<Node> { return CreateRef<GridNode>(id); });

        // Modifiers
        reg.Register("TransformGeometryNode"_sid, "Modifiers"_sid, [](UUID id) -> Ref<Node> { return CreateRef<TransformGeometryNode>(id); });
        reg.Register("MergeGeometryNode"_sid, "Modifiers"_sid, [](UUID id) -> Ref<Node> { return CreateRef<MergeGeometryNode>(id); });
        reg.Register("NoiseDisplaceNode"_sid, "Modifiers"_sid, [](UUID id) -> Ref<Node> { return CreateRef<NoiseDisplaceNode>(id); });
        reg.Register("RecalculateNormalsNode"_sid, "Modifiers"_sid, [](UUID id) -> Ref<Node> { return CreateRef<RecalculateNormalsNode>(id); });
        reg.Register("SubdivideNode"_sid, "Modifiers"_sid, [](UUID id) -> Ref<Node> { return CreateRef<SubdivideNode>(id); });

        // Input
        reg.Register("GraphInputNode"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<GraphInputNode>(id); });
        reg.Register("FloatNode"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<FloatNode>(id); });
        reg.Register("IntNode"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<IntNode>(id); });
        reg.Register("BoolNode"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<BoolNode>(id); });
        reg.Register("Vec2Node"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<Vec2Node>(id); });
        reg.Register("Vec3Node"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<Vec3Node>(id); });
        reg.Register("Vec4Node"_sid, "Input"_sid, [](UUID id) -> Ref<Node> { return CreateRef<Vec4Node>(id); });

        // Math
        reg.Register("AddNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<AddNode>(id); });
        reg.Register("MultiplyNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<MultiplyNode>(id); });
        reg.Register("SubtractNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<SubtractNode>(id); });
        reg.Register("DivideNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<DivideNode>(id); });
        reg.Register("ClampNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<ClampNode>(id); });
        reg.Register("RemapNode"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<RemapNode>(id); });
        reg.Register("SplitVec3Node"_sid, "Math"_sid, [](UUID id) -> Ref<Node> { return CreateRef<SplitVec3Node>(id); });
        reg.Register("CombineVec3Node"_sid, "Vector"_sid, [](UUID id) -> Ref<Node> { return CreateRef<CombineVec3Node>(id); });
        reg.Register("DotNode"_sid, "Vector"_sid, [](UUID id) -> Ref<Node> { return CreateRef<DotNode>(id); });
        reg.Register("CrossNode"_sid, "Vector"_sid, [](UUID id) -> Ref<Node> { return CreateRef<CrossNode>(id); });
        reg.Register("NormalizeNode"_sid, "Vector"_sid, [](UUID id) -> Ref<Node> { return CreateRef<NormalizeNode>(id); });

        // Output
        reg.Register("GeometryOutputNode"_sid, "Output"_sid, [](UUID id) -> Ref<Node> { return CreateRef<GeometryOutputNode>(id); });
    }

} // namespace Crowny
