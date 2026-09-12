#pragma once

#include "Crowny/Scene/Prefab.h"

namespace Crowny
{

    class PrefabSerializer
    {
    public:
        PrefabSerializer(const Ref<Prefab>& prefab);
        explicit PrefabSerializer(Prefab& prefab);

        void Serialize(const Path& filepath);
        void Deserialize(const Path& filepath);
        String SerializeToString();
        void DeserializeFromString(const String& text);

    private:
        Ref<Prefab> m_Owner;
        Prefab* m_Prefab;
    };

} // namespace Crowny
