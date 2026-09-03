#pragma once

#include "Crowny/Common/StdHeaders.h"

#include "Crowny/Common/Constants.h"
#include "Crowny/Common/Flags.h"
#include "Crowny/Common/Log.h"
#include "Crowny/Common/Math.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Memory/Memory.h"

#include <chrono>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

#include <spdlog/fmt/fmt.h>

#pragma warning(push, 0)

#include <spdlog/spdlog.h>

// Do not put this over the other spdlog include
#include <spdlog/fmt/ostr.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include <entt/entt.hpp>

#include <yaml-cpp/yaml.h>

#include <cereal/cereal.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <vulkan/vulkan.h>

#pragma warning(pop)
