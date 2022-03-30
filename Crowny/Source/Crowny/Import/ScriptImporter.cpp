#include "cwpch.h"

#include "Crowny/Import/ImportOptions.h"
#include "Crowny/Import/ScriptImporter.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"

namespace Crowny
{

    bool ScriptImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cs";
    }

    bool ScriptImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return true; }

    Ref<Asset> ScriptImporter::Import(const Path& filepath, Ref<const ImportOptions> importOptions)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        Ref<const CSharpScriptImportOptions> scriptImportOptions =
          std::static_pointer_cast<const CSharpScriptImportOptions>(importOptions);
        CW_ENGINE_INFO(stream->Size());
        Ref<ScriptCode> code = CreateRef<ScriptCode>(stream->GetAsString(), scriptImportOptions->IsEditorScript);
        code->SetName(filepath.filename().string());
        return code;
    }

    Ref<ImportOptions> ScriptImporter::CreateImportOptions() const { return CreateRef<CSharpScriptImportOptions>(); }

} // namespace Crowny