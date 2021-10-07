#pragma once

#include "Crowny/Layers/Layer.h"

namespace Crowny
{
    class LayerStack
    {
    public:
        LayerStack() = default;
        ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* layer);

        Vector<Layer*>::iterator begin() { return m_Layers.begin(); }
        Vector<Layer*>::iterator end() { return m_Layers.end(); }
        Vector<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
        Vector<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }

        Vector<Layer*>::const_iterator begin() const { return m_Layers.begin(); }
        Vector<Layer*>::const_iterator end() const { return m_Layers.end(); }
        Vector<Layer*>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
        Vector<Layer*>::const_reverse_iterator rend() const { return m_Layers.rend(); }

    private:
        Vector<Layer*> m_Layers;
        uint32_t m_LayerInsertIndex = 0;
    };
} // namespace Crowny