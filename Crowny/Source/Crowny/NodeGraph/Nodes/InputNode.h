#pragma once

#include "Crowny/NodeGraph/Node.h"

namespace Crowny
{

    class GraphInputNode : public Node
    {
    public:
        GraphInputNode(UUID id);
        virtual void Evaluate(NodeGraphEvaluator& evaluator) override;
        virtual String GetDisplayName() const override;
        virtual String GetCategory() const override { return "Input"; }

        void SetInputID(UUID inputId)
        {
            m_InputID = inputId;
            NotifyChanged();
        }
        UUID GetInputID() const { return m_InputID; }

    private:
        UUID m_InputID;
    };

} // namespace Crowny
