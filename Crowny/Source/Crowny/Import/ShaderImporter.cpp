#include "cwpch.h"

#include "Crowny/Import/Importer.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    bool IsFileTypeSupported(const String& ext)
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwsl" || lower == "glsl" || lower == "vksl" || lower == "hlsl";
    }

    template <>
    Ref<Shader> Importer::Import(const Path& filepath, const Ref<ImportOptions>& importOptions, const UUID& uuid)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        ShaderCompiler compiler;
        Ref<Shader> vertex = Shader::Create(compiler.Compile(stream->GetAsString()));
        return Shader::Create(shaderDesc);
    }
} // namespace Crowny
