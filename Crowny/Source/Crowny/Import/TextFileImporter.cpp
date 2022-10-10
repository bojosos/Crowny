#include "cwpch.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/TextFileImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{
    bool TextFileImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "txt" || lower == "yaml" || lower == "json" || lower == "xml";
    }

    bool TextFileImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> TextFileImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        Ref<const ShaderImportOptions> shaderImportOptions =
          std::static_pointer_cast<const ShaderImportOptions>(importOptions);
        String ext = filepath.extension().string();
        String source = FileSystem::OpenFile(filepath)->GetAsString();
        // return Text::Create(ShaderCompiler::Compile(source, shaderImportOptions->Language));
        return nullptr;
    }

    Ref<ImportOptions> TextFileImporter::CreateImportOptions() const { return CreateRef<ShaderImportOptions>(); }
} // namespace Crowny
