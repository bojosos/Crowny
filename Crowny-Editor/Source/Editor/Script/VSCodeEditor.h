#pragma once

#include "Editor/Script/CodeEditor.h"

namespace Crowny
{
    class VSCodeEditor final : public CodeEditor
    {
    public:
        explicit VSCodeEditor(Path executablePath);

        void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const override;
        CodeEditorSyncResult Sync(const CodeSolutionData& data, const Path& solutionPath) const override;
        void SetEditorExecutablePath(const Path& path) override;
        void ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const override;

    private:
        Path m_ExecutablePath;
    };

    class VSCodeEditorFactory final : public CodeEditorFactory
    {
    public:
        VSCodeEditorFactory();

        const Vector<CodeEditorInstallation>& GetAvailableEditors() const override { return m_Installations; }
        CodeEditor* Create(const Path& path) const override;

    private:
        Vector<CodeEditorInstallation> m_Installations;
    };
} // namespace Crowny
