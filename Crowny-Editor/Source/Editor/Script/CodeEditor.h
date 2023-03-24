#pragma once

#include "Crowny/Common/Module.h"

namespace Crowny
{
    class CodeEditor;
    struct ScriptProjectReference;
    struct CodeSolutionData;

	class CodeEditorManager : public Module<CodeEditorManager>
    {
    public:
        void OpenFile(const Path& path, uint32_t lineNumber) const;
        void SyncSolution(const String& projectName, const ScriptProjectReference& engineAssemblyRef) const;
        Path GetSolutionPath() const;

    private:
        mutable Ref<CodeEditor> m_ActiveEditor;
    };

    class CodeEditor
    {
    public:
        virtual ~CodeEditor() = default;

        virtual void OpenFile(const Path& solitionPath, const Path& filePath, uint32_t line) const = 0;
        virtual void Sync(const CodeSolutionData& data, const Path& solutionPath) const = 0;
    };
}