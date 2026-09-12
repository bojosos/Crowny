#include "DecalShowcase.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/PrimitiveMeshLibrary.h"
#include "Crowny/Serialization/ImportOptionsSerializer.h"
#include "Crowny/Serialization/MaterialSerializer.h"
#include "Crowny/Serialization/SceneSerializer.h"
#include "RenderTestImage.h"

#include <iomanip>
#include <sstream>

namespace Crowny::RenderTests
{
    namespace
    {
        void WriteText(const Path& path, const String& text)
        {
            String error;
            if (!FileSystem::WriteTextFileAtomic(path, text, &error))
                throw std::runtime_error(error);
        }
    } // namespace

    DecalShowcaseWriter::DecalShowcaseWriter(const Path& project) : m_Project(fs::absolute(project))
    {
        if (fs::exists(m_Project) && !fs::is_empty(m_Project))
            throw std::runtime_error("Choose an empty directory for the decal showcase; existing projects are preserved.");
        fs::create_directories(m_Project / "Assets");
        WriteText(m_Project / "ProjectSettings.yaml", "EditorCameraDistance: 3\nEditorCameraFocalPoint: [0, 0, 0]\n"
                                                      "EditorCameraPosition: [0, 0, 3]\nEditorCameraRotation: [0, 0]\n"
                                                      "GizmoMode: 4\nLastAssetBrowserEntry: ''\n");
    }

    void DecalShowcaseWriter::WriteMetadata(const Path& source, const UUID& id, AssetType type, const Ref<ImportOptions>& options)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", 2);
        SerializeValueYAML(out, "Uuid", id);
        SerializeValueYAML(out, "IncludeInBuild", true);
        SerializeValueYAML(out, "TypeId", static_cast<uint32_t>(type));
        if (options)
            ImportOptionsSerializer::Serialize(out, options);
        out << YAML::EndMap;
        WriteText(Path(source.string() + ".meta"), out.c_str());
    }

    void DecalShowcaseWriter::WriteTexture(const AssetHandle<Texture>& texture)
    {
        if (!texture || !m_Exported.insert(texture.GetUUID()).second)
            return;
        const auto& desc = texture->GetDesc();
        PixelData pixels(desc.Width, desc.Height, 1, desc.Format);
        pixels.AllocateInternalBuffer();
        texture->ReadData(pixels);
        Image image(desc.Width, desc.Height);
        for (uint32_t y = 0; y < desc.Height; ++y)
            for (uint32_t x = 0; x < desc.Width; ++x)
            {
                const auto color = glm::clamp(pixels.GetColorAt(x, y), glm::vec4(0), glm::vec4(1));
                for (uint32_t channel = 0; channel < 4; ++channel)
                    image.Pixel(x, y)[channel] = static_cast<uint8_t>(std::round(color[channel] * 255.0f));
            }
        const Path path = m_Project / "Assets" / (texture.GetUUID().ToString() + ".bmp");
        String error;
        if (!SaveBmp(path, image, error))
            throw std::runtime_error(error);
        const auto options = CreateRef<TextureImportOptions>();
        options->SRGB = desc.sRGB;
        options->GenerateMips = true;
        WriteMetadata(path, texture.GetUUID(), AssetType::Texture, options);
    }

    void DecalShowcaseWriter::WriteMaterial(const AssetHandle<Material>& material)
    {
        if (!material || !m_Exported.insert(material.GetUUID()).second)
            return;
        for (const auto& [name, descriptor] : material->GetTextureDescriptors())
            WriteTexture(material->GetTextureHandle(name));
        const Path path = m_Project / "Assets" / (material.GetUUID().ToString() + ".cwmat");
        if (!MaterialSerializer(material.GetInternalPtr()).Serialize(path))
            throw std::runtime_error("Could not export decal showcase material.");
        WriteMetadata(path, material.GetUUID(), AssetType::Material);
    }

    void DecalShowcaseWriter::WriteMesh(const AssetHandle<Mesh>& mesh)
    {
        if (!mesh || PrimitiveMeshLibrary::IsPrimitiveMesh(mesh.GetUUID()) || !m_Exported.insert(mesh.GetUUID()).second)
            return;
        auto data = mesh->IsCpuCached() ? mesh->GetMeshData() : nullptr;
        if (!data)
        {
            data = MeshData::Create(mesh->GetVertexCount(), mesh->GetIndexCount(), mesh->GetVertexLayout(), mesh->GetIndexType());
            mesh->ReadData(data);
        }
        std::ostringstream out;
        out << std::setprecision(9) << "# Crowny tapered decal receiver\n";
        for (const auto& position : data->GetPositions())
            out << "v " << position.x << ' ' << position.y << ' ' << position.z << '\n';
        for (const auto& uv : data->GetUVs())
            out << "vt " << uv.x << ' ' << uv.y << '\n';
        for (const auto& normal : data->GetNormals())
            out << "vn " << normal.x << ' ' << normal.y << ' ' << normal.z << '\n';
        const auto indices = data->GetIndices();
        for (size_t triangle = 0; triangle + 2 < indices.size(); triangle += 3)
        {
            out << 'f';
            for (uint32_t vertex = 0; vertex < 3; ++vertex)
            {
                const uint32_t index = indices[triangle + vertex] + 1;
                out << ' ' << index << '/' << index << '/' << index;
            }
            out << '\n';
        }
        const Path path = m_Project / "Assets" / (mesh.GetUUID().ToString() + ".obj");
        WriteText(path, out.str());
        const auto options = CreateRef<MeshImportOptions>();
        options->TangentsMode = NormalsImportMode::Calculate;
        options->ImportMaterials = false;
        options->GenerateCollision = false;
        options->GenerateLods = false;
        WriteMetadata(path, mesh.GetUUID(), AssetType::Mesh, options);
    }

    bool DecalShowcaseWriter::WriteScene(const Ref<Scene>& scene, const String& name, String& error)
    {
        Entity camera;
        try
        {
            for (const auto handle : scene->GetAllEntitiesWith<MeshRendererComponent>())
            {
                const auto& renderer = Entity(handle, scene.get()).GetComponent<MeshRendererComponent>();
                WriteMesh(renderer.MeshHandle);
                for (const auto& material : renderer.Materials)
                    WriteMaterial(material);
            }
            for (const auto handle : scene->GetAllEntitiesWith<DecalComponent>())
                WriteMaterial(Entity(handle, scene.get()).GetComponent<DecalComponent>().Material);
            camera = scene->CreateEntity("Showcase camera");
            camera.SetPosition({ 0, 0, 3 });
            camera.AddComponent<CameraComponent>().Camera.SetPerspective(glm::radians(45.0f), 0.1f, 100.0f);
            const Path path = m_Project / "Assets" / (name + ".cwscene");
            // This harness deliberately has no physics runtime. Reuse entity codecs while leaving
            // optional global scene settings at their editor defaults.
            YAML::Emitter out;
            out << YAML::BeginMap;
            SerializeValueYAML(out, "Version", SceneSerializer::FORMAT_VERSION);
            SerializeValueYAML(out, "Scene", name);
            SerializeValueYAML(out, "Entities", YAML::BeginSeq);
            SceneSerializer serializer(scene);
            for (const auto handle : scene->GetAllEntitiesWith<IDComponent>())
                serializer.SerializeEntity(out, Entity(handle, scene.get()));
            out << YAML::EndSeq << YAML::EndMap;
            WriteText(path, out.c_str());
            WriteMetadata(path, UuidGenerator::Generate(), AssetType::Scene);
            scene->DestroyEntity(camera);
            return true;
        }
        catch (const std::exception& failure)
        {
            if (camera)
                scene->DestroyEntity(camera);
            error = failure.what();
            return false;
        }
    }
} // namespace Crowny::RenderTests
