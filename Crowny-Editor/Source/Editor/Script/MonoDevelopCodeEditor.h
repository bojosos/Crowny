#pragma once

#include "Editor/Script/CodeEditor.h"

namespace Crowny
{
    class MonoDevelopCodeEditor : public CodeEditor
    {
        virtual void OpenFile(const Path& solutionPath, const Path& filePath, uint32_t line) const override;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const override;
        virtual void SetEditorExecutablePath(const Path& path) override;
        virtual void ReloadSolution(const CodeSolutionData& data, const Path& solutionPath) const override;
    };

	class MonoDevelopCodeEditorFactory : public CodeEditorFactory
	{
    public:
        virtual ~CodeEditorFactory() = default;
        virtual const Vector<CodeEditorInstallation>& GetAvailableEditors() const override;
        virtual CodeEditor* Create(const Path& path) const override;
	};
}