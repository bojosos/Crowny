#pragma once

#include "Crowny/Common/Uuid.h"

#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/Shader.h"

namespace Crowny
{
    class AssetManager;

    class AssetManifest
    {
    public:
        void Serialize(const std::string& filepath);
        const std::string& GetName() const { return m_Name; }
        template<class T>
        Ref<T> Load(const Uuid& id);
        AssetManifest(const std::string& name);
        ~AssetManifest() = default;

    private:
        friend class AssetManager;
        void Deserialize(const std::string& filepath);

    private:
        std::string m_Name;
        std::unordered_map<std::string, Uuid> m_Uuids;
        std::unordered_map<Uuid, std::string> m_Paths;
    };

    template <>
    inline Ref<Texture2D> AssetManifest::Load<Texture2D>(const Uuid& id)
    {
        //TODO: Load the texture metadata here
        return Texture2D::Create(m_Paths[id]);
    }

    template <>
    inline Ref<Shader> AssetManifest::Load<Shader>(const Uuid& id)
    {
        return nullptr;
    }

}