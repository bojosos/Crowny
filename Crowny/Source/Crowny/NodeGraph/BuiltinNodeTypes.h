#pragma once

namespace Crowny
{
    // Explicitly registers all built-in node types with the NodeRegistry.
    // Must be called once before using the NodeRegistry (e.g., from EditorLayer::OnAttach or
    // Application::Init). This function exists because the node .cpp files live inside a
    // static library; without a direct symbol reference from the executable/editor, the
    // linker would strip those translation units and the self-registration macros would never run.
    void RegisterBuiltinNodeTypes();

} // namespace Crowny
