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

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
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
#include <cereal/types/array.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

// Engine headers that were measured to be re-parsed by nearly every
// translation unit; precompiling them keeps per-file cost close to the
// third-party header cost above. Do not add glad/glfw here: the editor and
// test projects compile this header without those include directories.
#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/PlatformUtils.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Renderer.h"

#include <vulkan/vulkan.h>

#pragma warning(pop)
