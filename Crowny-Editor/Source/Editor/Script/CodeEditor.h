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
        // lineNumber 0 means it doesn't try to go to a line.
        void OpenFile(const Path& path, uint32_t lineNumber = 0) const;
        void SyncSolution(const String& projectName, const ScriptProjectReference& engineAssemblyRef) const;
        void SetEditorExecutablePath(const Path& path);
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
        virtual void SetEditorExecutablePath(const Path& path) = 0;
    };
} // namespace Crowny