#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Scripting/Mono/Mono.h"

namespace Crowny::MonoObjectIdentity
{
    bool SetEntity(MonoObject* instance, const UUID& value);
    bool SetComponentEntity(MonoObject* instance, const UUID& value);
    bool SetAsset(MonoObject* instance, const UUID& value);
} // namespace Crowny::MonoObjectIdentity
