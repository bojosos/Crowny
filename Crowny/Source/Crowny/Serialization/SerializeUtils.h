#pragma once

#include "Crowny/Common/Uuid.h"

#include <cereal/cereal.hpp>

#include <glm/gtc/type_ptr.hpp>

namespace glm // cereal requires that these in the glm namespace(maybe)
{
    template <class Archive> void Serialize(Archive& archive, glm::vec2& vec) { archive(vec.x, vec.y); }
    template <class Archive> void Serialize(Archive& archive, glm::vec3& vec) { archive(vec.x, vec.y, vec.z); }
    template <class Archive> void Serialize(Archive& archive, glm::vec4& vec) { archive(vec.x, vec.y, vec.z, vec.w); }
    template <class Archive> void Serialize(Archive& archive, glm::mat3& mat)
    {
        archive(cereal::binary_data(glm::value_ptr(mat), sizeof(glm::mat3)));
    }
    template <class Archive> void Serialize(Archive& archive, glm::mat4& mat)
    {
        archive(cereal::binary_data(glm::value_ptr(mat), sizeof(glm::mat4)));
    }
} // namespace glm

namespace std
{
    namespace filesystem
    {
        template <class Archive> void Load(Archive& archive, path& fp)
        {
            string res;
            archive(res);
            fp = res;
        }
        template <class Archive> void Save(Archive& archive, const path& fp) { archive(fp.string()); }
    } // namespace filesystem
} // namespace std

namespace Crowny
{
    template <class Archive> void Serialize(Archive& archive, UUID42& uuid)
    {
        archive(uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3]);
    }
} // namespace Crowny