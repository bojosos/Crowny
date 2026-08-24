#include "cwpch.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/TextFileImporter.h"

#include "Crowny/Common/FileSystem.h"

namespace Crowny
{
    bool TextFileImporter::IsExtensionSupported(const String& ext) const { return ext == "txt" || ext == "yaml" || ext == "json" || ext == "xml"; }

    bool TextFileImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> TextFileImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        const Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        const Ref<const ShaderImportOptions> shaderImportOptions = StaticRefCast<const ShaderImportOptions>(importOptions);
        const String ext = filepath.extension().string();
        const String source = FileSystem::OpenFile(filepath)->GetAsString();
        // return Text::Create(ShaderCompiler::Compile(source, shaderImportOptions->Language));
        return nullptr;
    }

    Ref<ImportOptions> TextFileImporter::CreateImportOptions() const { return CreateRef<ShaderImportOptions>(); }
} // namespace Crowny
