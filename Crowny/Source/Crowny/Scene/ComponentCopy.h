#pragma once

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

#include <type_traits>

namespace Crowny
{

    template <typename... Component> void CopyComponentIfExists(Entity dst, Entity src)
    {
        (
          [&]() {
              if constexpr (std::is_same_v<Component, RelationshipComponent>)
                  return;

              if (src.HasComponent<Component>())
                  dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
          }(),
          ...);
    }

    template <typename... Component> void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
    {
        CopyComponentIfExists<Component...>(dst, src);
    }

    inline void CopyAllExistingComponents(Entity dst, Entity src) { CopyComponentIfExists(AllComponents{}, dst, src); }

} // namespace Crowny
