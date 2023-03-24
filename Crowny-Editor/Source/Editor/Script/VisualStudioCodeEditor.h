#pragma once

#include "Editor/Script/CodeEditor.h"

namespace Crowny
{
    class VisualStudioCodeEditor : public CodeEditor
    {
    public:
        virtual ~VisualStudioCodeEditor() = default;

        virtual void OpenFile(const Path& solitionPath, const Path& filePath, uint32_t line) const override;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const override;
    };
}