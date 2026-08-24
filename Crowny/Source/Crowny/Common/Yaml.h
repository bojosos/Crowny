#pragma once

#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Physics/PhysicsMaterial.h"

#include <glm/glm.hpp>
#include <type_traits>
#include <yaml-cpp/yaml.h>

namespace Crowny
{
    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::quat& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const glm::mat4& mat)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                out << mat[i][j];
            }
        }
        out << YAML::EndSeq;
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const Crowny::UUID& uuid)
    {
        out << uuid.ToString();
        return out;
    }

    inline YAML::Emitter& operator<<(YAML::Emitter& out, const Crowny::StringID& id)
    {
        out << id.c_str();
        return out;
    }

#ifdef CW_DEBUG
    static Stack<String> mapKeys; // Used for debugging missing/mismatched YAML::BeginMap-YAML::EndMap
#endif

    static inline void BeginYAMLMap(YAML::Emitter& out, const String& key) { BeginYAMLMap(out, key.c_str()); }

    static inline void BeginYAMLMap(YAML::Emitter& out, const char* key)
    {
#ifdef CW_DEBUG
        mapKeys.push(key);
#endif
        out << YAML::Key << key;
        out << YAML::Value << YAML::BeginMap;
    }

    static inline void EndYAMLMap(YAML::Emitter& out, const String& mapEnd)
    {
#ifdef CW_DEBUG
        if (!mapKeys.empty())
        {
            const String& stackTop = mapKeys.top();
            CW_ENGINE_ASSERT(stackTop == mapEnd, "Unmatched YAML serialization map begin/end: expected: " + stackTop + " got: " + mapEnd);
            mapKeys.pop();
        }
#endif

        out << YAML::EndMap;
    }

    static inline void EndYAMLSeq(YAML::Emitter& out) { out << YAML::EndSeq; }

    template <typename Type> static inline void SerializeValueYAML(YAML::Emitter& out, const char* key, const Type& value)
    {
        if constexpr (std::is_same_v<Type, uint8_t> || std::is_same_v<Type, int8_t>)
            out << YAML::Key << key << YAML::Value << (int32_t)value;
        else
            out << YAML::Key << key << YAML::Value << value;
    }

    template <typename Type> static inline void SerializeValueYAML(YAML::Emitter& out, const Type& value)
    {
        if constexpr (std::is_same_v<Type, uint8_t> || std::is_same_v<Type, int8_t>)
            out << (int32_t)value;
        else
            out << value;
    }

    template <typename TEnum, typename TUnderlying = std::underlying_type_t<TEnum>>
    static inline void SerializeEnumYAML(YAML::Emitter& out, const char* key, const TEnum& value)
    {
        out << YAML::Key << key << YAML::Value << +(TUnderlying)value;
    }

    template <typename Enum, typename Storage> static inline void SerializeFlagsYAML(YAML::Emitter& out, const char* key, Flags<Enum, Storage> flags)
    {
        out << YAML::Key << key << YAML::Value << +(Storage)flags;
    }

    /// Deserialize an enum. If an errorMessageFormat is provided the underlying value of the enum is checked
    /// if it falls within the [min-max) bounds. The print using the errorMessageFormat will also get the
    /// underlying value as parameter so you can use {0} in the format.
    template <typename TEnum, typename TUnderlying = std::underlying_type_t<TEnum>>
    static void DeserializeEnumYAML(const YAML::Node& node, const char* key, TEnum& value, const TEnum& defaultValue, const char* errorMessageFormat,
                                    TUnderlying minValue, TUnderlying maxValue)
    {
        if constexpr (sizeof(TUnderlying) == 1)
            value = (TEnum)node[key].as<uint32_t>((uint32_t)defaultValue);
        else
            value = (TEnum)node[key].as<TUnderlying>((TUnderlying)defaultValue);

        TUnderlying underlyingValue = (TUnderlying)value;
        if (errorMessageFormat && (underlyingValue < minValue || underlyingValue >= maxValue))
        {
            CW_ENGINE_WARN(fmt::runtime(errorMessageFormat), (TUnderlying)value);
            value = defaultValue;
        }
    }

    template <typename TEnum, typename TUnderlying = std::underlying_type_t<TEnum>>
    static void DeserializeEnumYAML(const YAML::Node& node, const char* key, TEnum& value, const TEnum& defaultValue)
    {
        if constexpr (sizeof(TUnderlying) == 1)
            value = (TEnum)node[key].as<uint32_t>((uint32_t)defaultValue);
        else
            value = (TEnum)node[key].as<TUnderlying>((TUnderlying)defaultValue);
    }

    template <typename TEnum, typename TUnderlying = std::underlying_type_t<TEnum>>
    static inline void DeserializeEnumYAML(const YAML::Node& node, const char* key, TEnum& value, const TEnum& defaultValue,
                                           const char* errorMessageFormat, TUnderlying minValue, TEnum maxValue)
    {
        DeserializeEnumYAML(node, key, value, defaultValue, errorMessageFormat, minValue, (TUnderlying)maxValue);
    }

    template <typename Type> static inline void DeserializeValueYAML(const YAML::Node& node, const char* key, Type& value, const Type& defaultValue)
    {
        if constexpr (std::is_same_v<Type, uint8_t> || std::is_same_v<Type, int8_t>)
            value = (Type)node[key].as<int32_t>((int32_t)defaultValue);
        else
            value = node[key].as<Type>(defaultValue);
    }

    template <typename Type>
    static inline void DeserializeValueYAML(const YAML::Node& node, const char* key, Type& value, const Type& defaultValue,
                                            const char* errorMessageFormat, const Type& minValue, const Type& maxValue)
    {
        if constexpr (std::is_same_v<Type, uint8_t> || std::is_same_v<Type, int8_t>)
            value = (Type)node[key].as<int32_t>((int32_t)defaultValue);
        else
            value = node[key].as<Type>(defaultValue);

        if (errorMessageFormat && (value < minValue || value > maxValue))
        {
            CW_ENGINE_WARN(fmt::runtime(errorMessageFormat), (int32_t)value);
            value = defaultValue;
        }
    }

    template <typename Enum, typename Storage>
    static inline void DeserializeFlagsYAML(const YAML::Node& node, const char* key, Flags<Enum, Storage>& flags, Enum defaultValue)
    {
        if constexpr (sizeof(Storage) == 1)
            flags = Flags<Enum, Storage>((Storage)node[key].as<uint32_t>((uint32_t)defaultValue));
        else
            flags = Flags<Enum, Storage>(node[key].as<Storage>((Storage)defaultValue));
    }

} // namespace Crowny

namespace YAML
{

    template <> struct convert<Crowny::UUID>
    {
        static Node encode(const Crowny::UUID& uuid)
        {
            Node node;
            node = uuid.ToString();
            return node;
        }

        static bool decode(const Node& node, Crowny::UUID& rhs)
        {
            if (!node.IsScalar())
                return false;
            rhs = Crowny::UUID(node.as<std::string>());

            return true;
        }
    };

    template <> struct convert<Crowny::StringID>
    {
        static Node encode(const Crowny::StringID& id)
        {
            Node node;
            node = id.c_str();
            return node;
        }

        static bool decode(const Node& node, Crowny::StringID& rhs)
        {
            if (!node.IsScalar())
                return false;
            rhs = Crowny::StringID(node.as<std::string>());

            return true;
        }
    };

    template <> struct convert<glm::vec2>
    {
        static Node encode(const glm::vec2& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec2& rhs)
        {
            if (!node.IsSequence() || node.size() != 2)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();

            return true;
        }
    };

    template <> struct convert<glm::vec3>
    {
        static Node encode(const glm::vec3& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& rhs)
        {
            if (!node.IsSequence() || node.size() != 3)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();

            return true;
        }
    };

    template <> struct convert<glm::vec4>
    {
        static Node encode(const glm::vec4& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();

            return true;
        }
    };

    template <> struct convert<glm::quat>
    {
        static Node encode(const glm::quat& rhs)
        {
            Node node;
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::quat& rhs)
        {
            if (!node.IsSequence() || node.size() != 4)
                return false;
            rhs.x = node[0].as<float>();
            rhs.y = node[1].as<float>();
            rhs.z = node[2].as<float>();
            rhs.w = node[3].as<float>();

            return true;
        }
    };

    template <> struct convert<glm::mat4>
    {
        static Node encode(const glm::mat4& rhs)
        {
            Node node;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    node.push_back(rhs[i][j]);
                }
            }
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::mat4& rhs)
        {
            if (!node.IsSequence() || node.size() != 16)
                return false;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    rhs[i][j] = node[i * 4 + j].as<float>();
                }
            }

            return true;
        }
    };

} // namespace YAML
