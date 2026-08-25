#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny
{
    ManagedOperationResult ParseManagedCatalogJson(StringView json, ScriptCatalog& catalog, ManagedBackendId backend);
    ManagedOperationResult ParseManagedStateJson(StringView json, ScriptState& state, ManagedBackendId backend,
                                                 const ScriptTypeSchema* schema = nullptr);
    String WriteManagedStateJson(const ScriptState& state);
    Vector<ManagedDiagnostic> ParseManagedDiagnosticsJson(StringView json, ManagedBackendId backend);
} // namespace Crowny
