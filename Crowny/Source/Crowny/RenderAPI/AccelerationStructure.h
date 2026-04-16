#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Common/Types.h"

namespace Crowny
{

    enum class InstanceFlags
    {
        None = 0,
        NoCull = 1 << 0,
        CounterClockwise = 1 << 2,
        Opaque = 1 << 3
    };

    class AccelerationStructure;
    struct AccelerationInstance
    {
        glm::mat3x4 Transform; // ROW MAJOR xd!!!!
        uint32_t InstanceID : 24;
        uint32_t InstanceMask : 8;
        uint32_t InstanceContribToHitGroupIndex : 24;
        InstanceFlags Flags : 8;

        union {
            AccelerationStructure* BottomLevelAccel;
            uint64_t BlasDeviceAddress;
        };

        AccelerationInstance() : Transform(glm::mat3(1.0f)), Flags(InstanceFlags::None) {}
    };
    static_assert(sizeof(AccelerationInstance) == 64);

    enum class GeometryType
    {
        Triangles,
        AABBs,
        Spheres,
        Lss,
    };

    struct GeometryTriangles
    {
        Ref<VertexBuffer> VertexBuffer = nullptr;
        Ref<IndexBuffer> IndexBuffer = nullptr;
        uint64_t IndexOffset = 0;
        uint64_t VertexOffset = 0;
        uint32_t IndexCount = 0;
        uint32_t VertexCount = 0;
        uint32_t VertexStride = 0;
        IndexType IndexFormat = IndexType::Index_32;
        GpuBufferFormat VertexFormat = BF_32X3F;

        // TODO: Opacity map
    };

    struct GeometryAABBS
    {
        void* VertexBuffer = nullptr;
        void* Data = nullptr;
        uint64_t Offset = 0;
        uint32_t Count = 0;
        uint32_t Stride = 0;
    };

    struct GeometrySpheres
    {
        void* VertexBuffer = nullptr;
        void* IndexBuffer = nullptr;
        IndexType IndexFormat = IndexType::Index_32;
        GpuBufferFormat VertexFormat = BF_32X3F;
        GpuBufferFormat VertexRadiusFormat = BF_32X1F; // ?
        uint64_t IndexOffset = 0;
        uint64_t VertexPositionsOffset = 0;
        uint64_t VertexRadiusOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t IndexStride = 0;
        uint32_t VertexPositionStride = 0;
        uint32_t VertexRadiusStride = 0;
    };

    enum class GeometryLinearSweptSpheresFormat
    {
        List,
        SuccessiveImplicit
    };

    enum class GeometryLssEndcapMode
    {
        None,
        Chained
    };

    struct GeometryLinearSweptSpheres
    {
        void* IndexBuffer = nullptr;
        void* VertexBuffer = nullptr;
        IndexType IndexFormat = IndexType::Index_32;
        GpuBufferFormat VertexFormat = BF_32X3F;
        GpuBufferFormat VertexRadiusFormat = BF_32X1F; // ?
        uint64_t IndexOffset = 0;
        uint64_t VertexPositionsOffset = 0;
        uint64_t VertexRadiusOffset = 0;
        uint32_t VertexCount = 0;
        uint32_t IndexStride = 0;
        uint32_t VertexPositionStride = 0;
        uint32_t primitiveCount = 0;
        GeometryLinearSweptSpheresFormat PrimitiveFormat = GeometryLinearSweptSpheresFormat::List;
        GeometryLssEndcapMode EndcapMode = GeometryLssEndcapMode::None;
    };

    enum GeometryFlags
    {
        GF_None = 0,
        Opaque = 1 << 0,
        NoDuplicateAnyHitInvocation = 1 << 1
    };

    struct AccelerationGeometry
    {
        AccelerationGeometry() = default;
        struct Geometries
        {
            GeometryTriangles Triangles;
            // GeometryAABBS AABBs;
            // GeometrySpheres Spheres;
            // GeometryLinearSweptSpheres LinearSweptSpheres;

        } GeometryData;
        bool UseTransform = false;
        GeometryType Type = GeometryType::Triangles;
        glm::mat3x4 Transform = glm::mat3x4(1.0f);
        GeometryFlags Flags = GeometryFlags::GF_None;
    };

    enum class AccelerationStructBuildBits
    {
        None = 0,
        AllowUpdate = 1 << 0,
        AllowCompaction = 1 << 1,
        PreferFastTrace = 1 << 2,
        PreferFastBuild = 1 << 3,
        MinimizeMemory = 1 << 4,
        DoUpdate = 1 << 5
    };
    typedef Flags<AccelerationStructBuildBits> AccelerationStructBuildFlags;
    CW_FLAGS_OPERATORS(AccelerationStructBuildBits);

    class AccelerationStructure : public RefCounted
    {
    public:
        virtual ~AccelerationStructure() = default;

        virtual void BuildBottomLevel(const Ref<CommandBuffer>& buffer, const AccelerationGeometry* geometry, size_t numGeoms,
                                      AccelerationStructBuildFlags buildFlags) = 0;
        virtual void BuildTopLevel(const Ref<CommandBuffer>& commandBuffer, AccelerationInstance* instances, size_t numInstances,
                                   AccelerationStructBuildFlags buildFlags) = 0;

        // static Ref<AccelerationStructure> Create(const Vector<AccelerationGeometry>& bottomLevelGeometry, AccelerationStructBuildFlags flags);
        static Ref<AccelerationStructure> Create(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel,
                                                 uint32_t maxTopLevelInstances = 0,
                                                 AccelerationStructBuildFlags flags = AccelerationStructBuildBits::None);

    protected:
        AccelerationStructure(const Vector<AccelerationGeometry>& topLevelInstances, bool isTopLevel, uint32_t maxTopLevelInstances = 0,
                              AccelerationStructBuildFlags flags = AccelerationStructBuildBits::None);
        // TODO: Seperate constructors for top and bottom level
        AccelerationStructure(const Vector<AccelerationGeometry>& bottomLevelGeometry, AccelerationStructBuildFlags flags) {}
        size_t m_TopLevelMaxInstances = 0;
        Vector<AccelerationGeometry> m_Geometries;
        AccelerationStructBuildBits m_BuildFlags;
        String m_DebugName;
        bool m_IsTopLevel;
        bool m_AllowUpdate;
    };
} // namespace Crowny