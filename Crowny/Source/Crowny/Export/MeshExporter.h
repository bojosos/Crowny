#pragma once

#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    class MeshExporter
    {
    public:
        MeshExporter(const Ref<MeshData>& meshData);
        void Export(const Path& path);

    private:
        Ref<MeshData> m_MeshData;
    };
} // namespace Crowny