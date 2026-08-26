#pragma once

#include "Crowny/Ecs/Entity.h"
#include "Crowny/Physics/Physics3DTypes.h"
#include "Crowny/Utils/SmallVector.h"

namespace Crowny
{
    struct Collision2D
    {
        SmallVector<glm::vec2, 2> Points;
        Array<Entity, 2> Colliders;
    };

    struct Collision3D
    {
        Array<Entity, 2> Colliders;
        SmallVector<PhysicsContactPoint3D, 4> Points;
    };
} // namespace Crowny
