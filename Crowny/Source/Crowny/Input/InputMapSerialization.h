#pragma once

#include "Crowny/Input/InputMap.h"

namespace YAML
{
    class Node;
    class Emitter;
} // namespace YAML

namespace Crowny
{
    void SerializeInputMap(const InputMap& inputMap, YAML::Emitter& out);
    InputMap DeserializeInputMap(const YAML::Node& node);
} // namespace Crowny
