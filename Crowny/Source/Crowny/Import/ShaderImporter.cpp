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
        const Ref<const ShaderImportOptions> shaderImportOptions =
          std::static_pointer_cast<const ShaderImportOptions>(importOptions);

        const String ext = filepath.extension().string();
        const String source = FileSystem::OpenFile(filepath)->GetAsString();
        const ShaderDesc shaderDesc =
          ShaderCompiler::Compile(filepath, source, shaderImportOptions->Language, shaderImportOptions->GetDefines());
        return Shader::Create(shaderDesc);
    }

    Ref<ImportOptions> ShaderImporter::CreateImportOptions() const { return CreateRef<ShaderImportOptions>(); }
} // namespace Crowny
