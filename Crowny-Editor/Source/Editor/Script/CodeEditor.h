#pragma once

#include "Crowny/Common/Module.h"
#include "Editor/Editor.h"

namespace Crowny
{
    class CodeEditor;
    class CodeEditorFactory;
    struct ScriptProjectReference;
    struct CodeSolutionData;

    struct CodeEditorInstallation
    {
        Path ExecutablePath;
        bool Prerelease;
        String Name;
        CodeEditorVersion Version;
    };

    class CodeEditorManager : public Module<CodeEditorManager>
    {
    public:
        CodeEditorManager();
        ~CodeEditorManager();

        // lineNumber 0 means it doesn't try to go to a line.
        void OpenFile(const Path& path, uint32_t lineNumber = 0) const;

        void SyncSolution(const String& projectName, const ScriptProjectReference& engineAssemblyRef) const;
        void SetEditorExecutablePath(const Path& path);
        Path GetSolutionPath() const;
        void SetActive(const Path& editorPath);
        const Vector<CodeEditorInstallation>& GetAvailableEditors() const { return m_Editors; }
        const Path& GetActiveEditorPath() const { return m_ActiveEditorPath; }
    private:
        Vector<CodeEditorInstallation> m_Editors;
        CodeEditor* m_ActiveEditor;
        Vector<CodeEditorFactory*> m_Factories;
        Map<Path, CodeEditorFactory*> m_FactoryPerEditor;
        Path m_ActiveEditorPath;
    };

    class CodeEditor
    {
    public:
        virtual ~CodeEditor() = default;

        virtual void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const = 0;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const = 0;
        virtual void SetEditorExecutablePath(const Path& path) = 0;
        virtual void ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const = 0;
    };

    class CodeEditorFactory
    {
    public:
        virtual ~CodeEditorFactory() = default;
        virtual const Vector<CodeEditorInstallation>& GetAvailableEditors() const = 0;
        virtual CodeEditor* Create(const Path& path) const = 0;
    };
} // namespace Crowny