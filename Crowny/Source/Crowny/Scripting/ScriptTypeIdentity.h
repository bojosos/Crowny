#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    struct ScriptTypeIdentity
    {
        String Assembly;
        String Namespace;
        String TypeName;

        bool IsValid() const { return !Assembly.empty() && !TypeName.empty(); }
        String GetFullName() const { return Namespace.empty() ? TypeName : Namespace + "." + TypeName; }
        bool operator==(const ScriptTypeIdentity&) const = default;
    };
} // namespace Crowny
