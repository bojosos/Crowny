#include "cwpch.h"

#include "Crowny/Import/ShaderImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    bool ShaderImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwsl" || lower == "glsl"; // || lower == "vksl" || lower == "hlsl";
    }

    bool ShaderImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> ShaderImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        Ref<const ShaderImportOptions> shaderImportOptions =
          std::static_pointer_cast<const ShaderImportOptions>(importOptions);

        ShaderCompiler compiler;
        ShaderType shaderType;
        String ext = filepath.extension().string();
        String source = FileSystem::OpenFile(filepath)->GetAsString();
        return Shader::Create(
          ShaderCompiler::Compile(source, shaderImportOptions->Language, shaderImportOptions->GetDefines()));
    }

    Ref<ImportOptions> ShaderImporter::CreateImportOptions() const { return CreateRef<ShaderImportOptions>(); }
} // namespace Crowny
