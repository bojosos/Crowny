#pragma once

#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    struct PrefabComponent;

    class PrefabSync
    {
    public:
        // Sync all non-overridden properties from prefabEntity to instanceEntity.
        static void SyncEntity(Entity instanceEntity, Entity prefabEntity, const PrefabComponent& pc);

        template <typename T> static void SyncComponent(Entity instance, Entity prefab, const PrefabComponent& pc);
    };

} // namespace Crowny
