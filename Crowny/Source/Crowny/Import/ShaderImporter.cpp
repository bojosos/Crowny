#include "cwpch.h"

#include "Crowny/Import/ShaderImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Utils/ShaderCompiler.h"

namespace Crowny
{

    bool ShaderImporter::IsExtensionSupported(const String& ext) const
    {
        return ext == "cwsl" || ext == "glsl"; // || ext == "vksl" || ext == "hlsl";
    }

    bool ShaderImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> ShaderImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        const Ref<const ShaderImportOptions> shaderImportOptions = StaticRefCast<const ShaderImportOptions>(importOptions);
        if (shaderImportOptions == nullptr)
        {
            CW_ENGINE_ERROR("Shader import options are missing for '{}'.", filepath);
            return nullptr;
        }

        const Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        if (stream == nullptr)
        {
            CW_ENGINE_ERROR("Failed to open shader source '{}'.", filepath);
            return nullptr;
        }

        const String source = stream->GetAsString();
        stream->Close();
        ShaderCompileResult compileResult =
          ShaderCompiler::CompileWithDiagnostics(filepath, source, shaderImportOptions->Language, shaderImportOptions->GetDefines());
        for (const ShaderDiagnostic& diagnostic : compileResult.Diagnostics)
        {
            const String location = diagnostic.Line == 0 ? filepath.string() : filepath.string() + ":" + std::to_string(diagnostic.Line);
            if (diagnostic.Severity == ShaderDiagnosticSeverity::Error)
                CW_ENGINE_ERROR("{}: {}", location, diagnostic.Message);
            else
                CW_ENGINE_WARN("{}: {}", location, diagnostic.Message);
        }
        if (!compileResult.Succeeded())
            return nullptr;
        return Shader::Create(compileResult.Description);
    }

    Ref<ImportOptions> ShaderImporter::CreateImportOptions() const { return CreateRef<ShaderImportOptions>(); }
} // namespace Crowny
