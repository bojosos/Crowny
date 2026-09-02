#pragma once

#include "Crowny/Common/Module.h"
#include "Crowny/Scripting/ManagedReload.h"
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

    struct CodeEditorSyncResult
    {
        bool Succeeded = false;
        bool Changed = false;

        operator bool() const { return Changed; }
    };

    class CodeEditorManager : public Module<CodeEditorManager>
    {
    public:
        CodeEditorManager();
        ~CodeEditorManager();

        // lineNumber 0 means it doesn't try to go to a line.
        void OpenFile(const Path& path, uint32_t lineNumber = 0) const;

        // Rebuilds the project graph and synchronizes it immediately.
        void SyncSolution(const String& projectName);
        void SyncSolution(const String& projectName, const ScriptProjectReference& engineAssemblyRef);

        // File-watch and settings callers only need to notify this module. It batches
        // changes, re-evaluates the graph, and reloads the active IDE only for changed output.
        void NotifyProjectInputChanged(const Path& path);
        void NotifyProjectSettingsChanged();
        void SyncIfNeeded();
        void Update();
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
        ManagedReloadDebouncer m_ProjectSyncDebouncer{ std::chrono::milliseconds(250) };
        String m_PendingProjectName;
        String m_LastProjectGraphFingerprint;
        Vector<Path> m_TrackedProjectInputs;
        String m_LastTrackedProjectInputFingerprint;

        bool BuildSolutionData(const String& projectName, const ScriptProjectReference& engineAssemblyRef, CodeSolutionData& outData) const;
        bool SyncSolutionData(const CodeSolutionData& data, bool force);
        void CaptureProjectInputs(const CodeSolutionData& data);
        void RequestSolutionSync(const String& projectName);
    };

    class CodeEditor
    {
    public:
        virtual ~CodeEditor() = default;

        virtual void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const = 0;
        // Changed controls reload; Succeeded keeps a failed write retryable by the graph synchronizer.
        virtual CodeEditorSyncResult Sync(const CodeSolutionData& data, const Path& solutionPath) const = 0;
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
