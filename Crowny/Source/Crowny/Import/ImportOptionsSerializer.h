#pragma once

#include "Crowny/Import/ImportOptions.h"

namespace YAML
{
    class Emitter;
    class Node;
} // namespace YAML

namespace Crowny
{

    class ImportOptionsSerializer
    {
    public:
        static void Serialize(YAML::Emitter& emitter, const Ref<ImportOptions>& importOptions);
        static Ref<ImportOptions> Deserialize(const YAML::Node& data);
    };

} // namespace Crowny