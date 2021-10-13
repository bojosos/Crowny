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
        return lower == "cwsl"; // || lower == "glsl" || lower == "vksl" || lower == "hlsl";
    }

    template <> Ref<Shader> Importer::Import(const Path& filepath, const Ref<ImportOptions>& importOptions)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        Ref<ShaderImportOptions> shaderImportOptions = std::static_pointer_cast<ShaderImportOptions>(importOptions);
        ShaderCompiler compiler;
        ShaderType shaderType;
        String ext = filepath.extension();
        String source = FileSystem::OpenFile(filepath)->GetAsString();
        return Shader::Create(ShaderCompiler::Compile(source, importOptions != nullptr ? shaderImportOptions->Language
                                                                                       : ShaderLanguage::ALL));
    }
} // namespace Crowny
