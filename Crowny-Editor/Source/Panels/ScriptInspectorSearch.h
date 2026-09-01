#pragma once

#include "Crowny/Scripting/Managed/ManagedTypes.h"

namespace Crowny::ScriptInspectorSearch
{
    bool Matches(StringView propertyName, const ScriptValue& value, StringView query, const ScriptSearchSettings& settings,
                 ScriptValueKind declaredKind = ScriptValueKind::Null, const ScriptTypeIdentity* declaredType = nullptr);
}
