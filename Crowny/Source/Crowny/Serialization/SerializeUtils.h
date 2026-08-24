#pragma once

#include "Crowny/Common/StringID.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Utils/SmallVector.h"

#include <cereal/cereal.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace glm // cereal requires that these in the glm namespace(maybe)
{
    template <class Archive> void Serialize(Archive& archive, glm::vec2& vec) { archive(vec.x, vec.y); }
    template <class Archive> void Serialize(Archive& archive, glm::vec3& vec) { archive(vec.x, vec.y, vec.z); }
    template <class Archive> void Serialize(Archive& archive, glm::vec4& vec) { archive(vec.x, vec.y, vec.z, vec.w); }
    template <class Archive> void Serialize(Archive& archive, glm::quat& quat) { archive(quat.x, quat.y, quat.z, quat.w); }
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
    template <class Archive> void Serialize(Archive& archive, UUID& uuid) { archive(uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3]); }
    template <class Archive> void Save(Archive& archive, const StringID& id)
    {
        String str = id.c_str();
        archive(str);
    }
    template <class Archive> void Load(Archive& archive, StringID& id)
    {
        String str;
        archive(str);
        id = StringID(str);
    }

    template <class Archive, typename Type, uint32_t N> void Save(Archive& archive, const SmallVector<Type, N>& vector)
    {
        archive(cereal::make_size_tag(static_cast<cereal::size_type>(vector.size())));
        for (auto const& i : vector)
            archive(i);
    }

    template <class Archive, typename Type, uint32_t N> void Load(Archive& archive, SmallVector<Type, N>& vector)
    {
        cereal::size_type size;
        archive(cereal::make_size_tag(size));
        vector.resize(static_cast<uint32_t>(size));
        for (auto& i : vector)
            archive(i);
    }
} // namespace Crowny
