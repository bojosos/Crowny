#pragma once

#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    struct Connection
    {
        UUID ID;
        UUID OutputNodeID;
        UUID OutputPinID;
        UUID InputNodeID;
        UUID InputPinID;
    };

} // namespace Crowny
