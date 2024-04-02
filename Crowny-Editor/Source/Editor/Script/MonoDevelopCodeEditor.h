#pragma once

#include "Editor/Script/CodeEditor.h"

namespace Crowny
{
    class MonoDevelopCodeEditor : public CodeEditor
    {
    public:
        virtual void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const override;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const override{};
        virtual void SetEditorExecutablePath(const Path& path) override{};
        virtual void ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const override{};

    private:
        Path m_ExecPath;
    };

    class MonoDevelopCodeEditorFactory : public CodeEditorFactory
    {
    public:
        MonoDevelopCodeEditorFactory();
        virtual ~MonoDevelopCodeEditorFactory() = default;
        virtual const Vector<CodeEditorInstallation>& GetAvailableEditors() const override
        {
            return m_MonoDevelopInstallations;
        }
        virtual CodeEditor* Create(const Path& path) const override { return nullptr; }

    private:
        Vector<CodeEditorInstallation> m_MonoDevelopInstallations;
    };
} // namespace Crowny