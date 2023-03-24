#include "cwepch.h"

#include "Editor/Script/CodeEditor.h"
#include "Editor/Script/ScriptProjectGenerator.h"
#include "Editor/ProjectLibrary.h"
#include "Editor/Script/VisualStudioCodeEditor.h"
#include "Build/BuildManager.h"
#include "Editor/Editor.h"

namespace Crowny
{
    Path CodeEditorManager::GetSolutionPath() const
    {
        Path path = Editor::Get().GetProjectPath();
        path = path / (Editor::Get().GetProjectName() + ".sln");
        return path;
    }

    void CodeEditorManager::OpenFile(const Path& path, uint32_t lineNumber) const
    {
        if (m_ActiveEditor == nullptr)
            return;
        Path filepath = path;
        if (!filepath.is_absolute())
            filepath = ProjectLibrary::Get().GetAssetFolder() / filepath;

        m_ActiveEditor->OpenFile(GetSolutionPath(), filepath, lineNumber);
    }

    void CodeEditorManager::SyncSolution(const String& projectName,
                                         const ScriptProjectReference& engineAssemblyRef) const
    {
        if (m_ActiveEditor == nullptr)
        {
            // return;
            m_ActiveEditor = CreateRef<VisualStudioCodeEditor>();
        }

        CodeSolutionData solutionData;
        solutionData.Name = Editor::Get().GetProjectName();

        Vector<AssetType> assetTypes = {
            AssetType::ScriptCode, AssetType::PlainText, AssetType::Shader
        };
        Vector<Ref<LibraryEntry>> codeEntries = ProjectLibrary::Get().Search("*", assetTypes);
        PlatformType activePlatform = BuildManager::Get().GetActivePlatform();
        Vector<String> baseAssemblies = BuildManager::Get().GetBaseAssemblies(activePlatform);
        
        CodeProjectData& codeProjectData = solutionData.Projects.emplace_back();
        codeProjectData.Name = projectName;
        codeProjectData.Defines = BuildManager::Get().GetDefines(activePlatform);
        codeProjectData.AssemblyReferences.push_back(engineAssemblyRef);
        for (const String& assemblyName : baseAssemblies)
            // codeProjectData.AssemblyReferences.push_back(ScriptProjectReference{ assemblyName, Path("C:\\Program Files\\Mono\\lib\\mono\\4.8-api") / (assemblyName+".dll") });
            codeProjectData.AssemblyReferences.push_back(ScriptProjectReference{ assemblyName, Path() });

        for (const Ref<LibraryEntry>& entry : codeEntries)
        {
            if (entry->Type != LibraryEntryType::File)
                continue;

            FileEntry* fileEntry = static_cast<FileEntry*>(entry.get());
            if (fileEntry->Metadata->Type == AssetType::ScriptCode)
            {
                Ref<CSharpScriptImportOptions> scriptImportOptions =
                  std::static_pointer_cast<CSharpScriptImportOptions>(fileEntry->Metadata->ImportOptions);
                bool isEditorScript = false;
                if (scriptImportOptions != nullptr)
                    isEditorScript = scriptImportOptions->IsEditorScript;
                if (!isEditorScript)
                    codeProjectData.ScriptFiles.push_back(fileEntry->Filepath);
            }
            else
                codeProjectData.NonScriptFiles.push_back(fileEntry->Filepath);
        }

        m_ActiveEditor->Sync(solutionData, Editor::Get().GetProjectPath());
    }
} // namespace Crowny