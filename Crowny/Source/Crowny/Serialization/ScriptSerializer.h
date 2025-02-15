#pragma once

#include "Crowny/Common/DataStream.h"
#include "Crowny/Scripting/Serialization/SerializableObject.h"
// #include "Crowny/Scripting/Serialization/"

namespace Crowny
{

    class ScriptSerializer
    {
    public:
        ScriptSerializer() = default;

        void Serialize(SerializableObject* object);
        // Ref<SerializableScriptObject> Deserialize(Ref<MemoryDataStream>& stream);
    private:
        Scene* m_Scene;
    };

} // namespace Crowny