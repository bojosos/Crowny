#include "cwpch.h"

#include "Crowny/Physics/PhysicsMesh.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Renderer/Mesh.h"

#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace Crowny
{
    namespace
    {
        struct PositionKey
        {
            uint32_t X = 0;
            uint32_t Y = 0;
            uint32_t Z = 0;
            bool operator==(const PositionKey& other) const { return X == other.X && Y == other.Y && Z == other.Z; }
        };

        struct PositionKeyHash
        {
            size_t operator()(const PositionKey& key) const
            {
                size_t seed = 0;
                HashCombine(seed, key.X, key.Y, key.Z);
                return seed;
            }
        };

        uint32_t QuantizeComponent(float value, float tolerance)
        {
            if (tolerance <= 0.0f)
            {
                uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                if (bits == 0x80000000u) // -0.0f welds with +0.0f
                    bits = 0;
                return bits;
            }
            const double cell = std::floor(static_cast<double>(value) / tolerance + 0.5);
            return static_cast<uint32_t>(static_cast<int64_t>(cell));
        }

        PositionKey MakeKey(const glm::vec3& position, float tolerance)
        {
            return { QuantizeComponent(position.x, tolerance), QuantizeComponent(position.y, tolerance), QuantizeComponent(position.z, tolerance) };
        }

        // Keeps the extreme points along a fixed set of directions (guaranteed hull vertices), then fills the
        // remaining budget with farthest-point sampling so the cloud stays well spread. Deterministic and
        // dependency-free; the physics SDK computes the actual hull from the result.
        Vector<glm::vec3> DecimatePointCloud(const Vector<glm::vec3>& points, uint32_t maxPoints)
        {
            Vector<glm::vec3> result;
            if (points.size() <= maxPoints || maxPoints == 0)
                return result;

            Vector<uint8_t> selected(points.size(), 0);
            result.reserve(maxPoints);
            const auto select = [&](size_t index) {
                if (selected[index])
                    return;
                selected[index] = 1;
                result.push_back(points[index]);
            };

            for (int x = -1; x <= 1 && result.size() < maxPoints; x++)
                for (int y = -1; y <= 1 && result.size() < maxPoints; y++)
                    for (int z = -1; z <= 1 && result.size() < maxPoints; z++)
                    {
                        if (x == 0 && y == 0 && z == 0)
                            continue;
                        const glm::vec3 direction = glm::normalize(glm::vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)));
                        size_t best = 0;
                        float bestDot = -std::numeric_limits<float>::max();
                        for (size_t i = 0; i < points.size(); i++)
                        {
                            const float dot = glm::dot(points[i], direction);
                            if (dot > bestDot)
                            {
                                bestDot = dot;
                                best = i;
                            }
                        }
                        select(best);
                    }

            Vector<float> minDistance(points.size(), std::numeric_limits<float>::max());
            for (size_t i = 0; i < points.size(); i++)
                for (const glm::vec3& chosen : result)
                    minDistance[i] = std::min(minDistance[i], glm::dot(points[i] - chosen, points[i] - chosen));

            while (result.size() < maxPoints)
            {
                size_t best = 0;
                float bestDistance = -1.0f;
                for (size_t i = 0; i < points.size(); i++)
                {
                    if (!selected[i] && minDistance[i] > bestDistance)
                    {
                        bestDistance = minDistance[i];
                        best = i;
                    }
                }
                if (bestDistance < 0.0f)
                    break;
                select(best);
                const glm::vec3& chosen = points[best];
                for (size_t i = 0; i < points.size(); i++)
                    minDistance[i] = std::min(minDistance[i], glm::dot(points[i] - chosen, points[i] - chosen));
            }
            return result;
        }
    } // namespace

    const Vector<glm::u32vec2>& PhysicsMesh::GetEdges() const
    {
        if (m_EdgesBuilt)
            return m_Edges;
        m_EdgesBuilt = true;
        std::unordered_set<uint64_t> seen;
        seen.reserve(m_Indices.size());
        m_Edges.reserve(m_Indices.size());
        for (size_t i = 0; i + 2 < m_Indices.size(); i += 3)
        {
            for (size_t edge = 0; edge < 3; edge++)
            {
                const uint32_t a = m_Indices[i + edge];
                const uint32_t b = m_Indices[i + (edge + 1) % 3];
                const uint32_t low = std::min(a, b);
                const uint32_t high = std::max(a, b);
                if (seen.insert((static_cast<uint64_t>(low) << 32) | high).second)
                    m_Edges.emplace_back(low, high);
            }
        }
        m_Edges.shrink_to_fit();
        return m_Edges;
    }

    void PhysicsMesh::SetGeometry(Vector<glm::vec3> positions, Vector<uint32_t> indices, Vector<glm::vec3> convexPoints, uint32_t sourceVertexCount)
    {
        m_Positions = std::move(positions);
        m_Indices = std::move(indices);
        m_ConvexPoints = std::move(convexPoints);
        m_SourceVertexCount = sourceVertexCount;
        m_Edges.clear();
        m_EdgesBuilt = false;
        RecalculateBounds();
    }

    void PhysicsMesh::RecalculateBounds()
    {
        if (m_Positions.empty())
        {
            m_Bounds = AABox(glm::vec3(0.0f), glm::vec3(0.0f));
            return;
        }
        glm::vec3 minimum = m_Positions.front();
        glm::vec3 maximum = m_Positions.front();
        for (const glm::vec3& position : m_Positions)
        {
            minimum = glm::min(minimum, position);
            maximum = glm::max(maximum, position);
        }
        m_Bounds = AABox(minimum, maximum);
    }

    void PhysicsMesh::CopyScaled(const glm::vec3& scale, bool convex, Vector<glm::vec3>& outVertices, Vector<uint32_t>& outIndices) const
    {
        const Vector<glm::vec3>& source = convex ? GetConvexPoints() : m_Positions;
        outVertices.resize(source.size());
        for (size_t i = 0; i < source.size(); i++)
            outVertices[i] = source[i] * scale;

        if (convex)
        {
            outIndices.clear();
            return;
        }
        outIndices = m_Indices;
        if (scale.x * scale.y * scale.z < 0.0f)
        {
            for (size_t i = 0; i + 2 < outIndices.size(); i += 3)
                std::swap(outIndices[i + 1], outIndices[i + 2]);
        }
    }

    Ref<PhysicsMesh> PhysicsMesh::Build(const MeshData& meshData, const Vector<SubMesh>& subMeshes, const PhysicsMeshBuildSettings& settings)
    {
        if (meshData.GetVertexCount() == 0 || !meshData.GetBufferLayout().HasAttribute(VertexAttribute::Position))
            return nullptr;
        const Vector<glm::vec3> positions = meshData.GetPositions();
        const Vector<uint32_t> allIndices = meshData.GetIndices();

        Vector<uint32_t> triangleIndices;
        if (subMeshes.empty())
        {
            triangleIndices = allIndices;
        }
        else
        {
            for (const SubMesh& subMesh : subMeshes)
            {
                if (subMesh.MeshDrawMode != DrawMode::TRIANGLE_LIST)
                    continue;
                const size_t begin = std::min<size_t>(subMesh.IndexOffset, allIndices.size());
                const size_t end = std::min<size_t>(static_cast<size_t>(subMesh.IndexOffset) + subMesh.IndexCount, allIndices.size());
                triangleIndices.insert(triangleIndices.end(), allIndices.begin() + begin, allIndices.begin() + end);
            }
        }
        return Build(positions, triangleIndices, settings);
    }

    Ref<PhysicsMesh> PhysicsMesh::Build(const Vector<glm::vec3>& positions, const Vector<uint32_t>& indices, const PhysicsMeshBuildSettings& settings)
    {
        if (positions.empty())
            return nullptr;

        // Weld positions.
        std::unordered_map<PositionKey, uint32_t, PositionKeyHash> welded;
        welded.reserve(positions.size());
        Vector<uint32_t> remap(positions.size());
        Vector<glm::vec3> uniquePositions;
        uniquePositions.reserve(positions.size());
        glm::vec3 minimum = positions.front();
        glm::vec3 maximum = positions.front();
        for (size_t i = 0; i < positions.size(); i++)
        {
            const glm::vec3& position = positions[i];
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            {
                remap[i] = std::numeric_limits<uint32_t>::max();
                continue;
            }
            const auto [iter, inserted] =
              welded.try_emplace(MakeKey(position, settings.WeldTolerance), static_cast<uint32_t>(uniquePositions.size()));
            if (inserted)
            {
                uniquePositions.push_back(position);
                minimum = glm::min(minimum, position);
                maximum = glm::max(maximum, position);
            }
            remap[i] = iter->second;
        }
        if (uniquePositions.size() < 3)
            return nullptr;

        // Drop out-of-range, repeated-vertex and zero-area triangles. Jolt refuses whole meshes containing degenerates.
        const glm::vec3 extent = maximum - minimum;
        const float extentSq = glm::dot(extent, extent);
        const float areaEpsilon = 1.0e-12f * extentSq * extentSq;
        Vector<uint32_t> triangles;
        triangles.reserve(indices.size());
        Vector<uint8_t> used(uniquePositions.size(), 0);
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            if (indices[i] >= positions.size() || indices[i + 1] >= positions.size() || indices[i + 2] >= positions.size())
                continue;
            const uint32_t a = remap[indices[i]];
            const uint32_t b = remap[indices[i + 1]];
            const uint32_t c = remap[indices[i + 2]];
            if (a == std::numeric_limits<uint32_t>::max() || b == std::numeric_limits<uint32_t>::max() || c == std::numeric_limits<uint32_t>::max())
                continue;
            if (a == b || b == c || a == c)
                continue;
            const glm::vec3 cross = glm::cross(uniquePositions[b] - uniquePositions[a], uniquePositions[c] - uniquePositions[a]);
            if (glm::dot(cross, cross) <= areaEpsilon)
                continue;
            triangles.insert(triangles.end(), { a, b, c });
            used[a] = used[b] = used[c] = 1;
        }

        // Compact to referenced positions when triangles exist; a bare point cloud keeps every welded point.
        Vector<glm::vec3> finalPositions;
        if (!triangles.empty())
        {
            Vector<uint32_t> compact(uniquePositions.size(), std::numeric_limits<uint32_t>::max());
            finalPositions.reserve(uniquePositions.size());
            for (size_t i = 0; i < uniquePositions.size(); i++)
            {
                if (!used[i])
                    continue;
                compact[i] = static_cast<uint32_t>(finalPositions.size());
                finalPositions.push_back(uniquePositions[i]);
            }
            for (uint32_t& index : triangles)
                index = compact[index];
        }
        else
        {
            finalPositions = std::move(uniquePositions);
        }
        if (finalPositions.size() < 3)
            return nullptr;

        Vector<glm::vec3> convexPoints = DecimatePointCloud(finalPositions, settings.MaxConvexPoints);

        Ref<PhysicsMesh> result = CreateRef<PhysicsMesh>();
        result->SetGeometry(std::move(finalPositions), std::move(triangles), std::move(convexPoints), static_cast<uint32_t>(positions.size()));
        return result;
    }

    AssetHandle<PhysicsMesh> CreateRuntimePhysicsMesh(AssetManager& assetManager, const Ref<PhysicsMesh>& physicsMesh)
    {
        if (physicsMesh == nullptr)
            return {};
        return static_asset_cast<PhysicsMesh>(assetManager.CreateAssetHandle(physicsMesh));
    }

    namespace
    {
        struct TransientCollisionMesh
        {
            Ref<PhysicsMesh> Mesh;
            uint64_t Version = 0;
        };

        struct ResolverState
        {
            UnorderedMap<UUID, AssetHandle<PhysicsMesh>> Registered;
            UnorderedMap<UUID, TransientCollisionMesh> Transient;
        };

        ResolverState& GetResolverState()
        {
            static ResolverState state;
            return state;
        }

        void SetError(String* outError, String message)
        {
            if (outError != nullptr)
                *outError = std::move(message);
        }
    } // namespace

    void PhysicsMeshResolver::Register(const UUID& meshUuid, const AssetHandle<PhysicsMesh>& physicsMesh)
    {
        if (meshUuid == UUID::EMPTY)
            return;
        if (!physicsMesh.HasUUID())
        {
            Unregister(meshUuid);
            return;
        }
        GetResolverState().Registered[meshUuid] = physicsMesh;
    }

    void PhysicsMeshResolver::Unregister(const UUID& meshUuid)
    {
        GetResolverState().Registered.erase(meshUuid);
        GetResolverState().Transient.erase(meshUuid);
    }

    void PhysicsMeshResolver::Clear()
    {
        GetResolverState().Registered.clear();
        GetResolverState().Transient.clear();
    }

    Ref<PhysicsMesh> PhysicsMeshResolver::Resolve(const AssetHandle<Mesh>& mesh, String* outError)
    {
        if (!mesh.HasUUID())
        {
            SetError(outError, "No mesh is assigned.");
            return nullptr;
        }
        ResolverState& state = GetResolverState();
        const UUID& meshUuid = mesh.GetUUID();

        const auto registered = state.Registered.find(meshUuid);
        if (registered != state.Registered.end())
        {
            AssetHandle<PhysicsMesh>& handle = registered->second;
            if (!handle.IsLoaded() && AssetManager::TryGet() != nullptr)
                handle = AssetManager::TryGet()->LoadFromUUID<PhysicsMesh>(handle.GetUUID());
            if (handle.IsLoaded())
                return handle.GetInternalPtr();
        }

        AssetHandle<Mesh> loadedMesh = mesh;
        if (!loadedMesh.IsLoaded() && AssetManager::TryGet() != nullptr)
            loadedMesh = AssetManager::TryGet()->LoadFromUUID<Mesh>(meshUuid);
        if (!loadedMesh.IsLoaded())
        {
            SetError(outError, "Mesh " + meshUuid.ToString() + " is not loaded.");
            return nullptr;
        }

        const UUID& collisionUuid = loadedMesh->GetCollisionMeshUuid();
        if (collisionUuid != UUID::EMPTY && AssetManager::TryGet() != nullptr)
        {
            AssetHandle<PhysicsMesh> collision = AssetManager::TryGet()->LoadFromUUID<PhysicsMesh>(collisionUuid);
            if (collision.IsLoaded())
                return collision.GetInternalPtr();
            SetError(outError, "Collision data for mesh '" + loadedMesh->GetName() + "' is missing. Reimport the source asset.");
            return nullptr;
        }

        if (loadedMesh->IsCpuCached())
        {
            const Ref<MeshData> data = loadedMesh->GetMeshData();
            const uint64_t version = loadedMesh->GetGpuVersion();
            auto transient = state.Transient.find(meshUuid);
            if (transient != state.Transient.end() && transient->second.Version == version && transient->second.Mesh != nullptr)
                return transient->second.Mesh;
            Ref<PhysicsMesh> built = data != nullptr ? PhysicsMesh::Build(*data, loadedMesh->GetSubMeshes()) : nullptr;
            if (built != nullptr)
            {
                built->SetName(loadedMesh->GetName() + " Collision");
                state.Transient[meshUuid] = { built, version };
                return built;
            }
            SetError(outError, "Mesh '" + loadedMesh->GetName() + "' has no usable triangle geometry.");
            return nullptr;
        }

        SetError(outError, "Mesh '" + loadedMesh->GetName() + "' has no collision data. Reimport it with Generate Collision enabled.");
        return nullptr;
    }
} // namespace Crowny
