#include "cwpch.h"

#include "Crowny/Common/PlatformUtils.h"

#include <uuid/uuid.h>

namespace Crowny
{
    Uuid PlatformUtils::GenerateUuid()
    {
        uuid_t native;
        uuid_generate(native);

        return Uuid(*(uint32_t*)&native[0], *(uint32_t*)&native[4], *(uint32_t*)&native[8], *(uint32_t*)&native[12]);
    }
}