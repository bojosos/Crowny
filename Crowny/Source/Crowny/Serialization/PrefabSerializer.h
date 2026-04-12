#pragma once

#include "Crowny/Scene/Prefab.h"

namespace Crowny
{

    class PrefabSerializer
    {
    public:
        PrefabSerializer(const Ref<Prefab>& prefab);

        void Serialize(const Path& filepath);
        void Deserialize(const Path& filepath);

    private:
        Ref<Prefab> m_Prefab;
    };

} // namespace Crowny
