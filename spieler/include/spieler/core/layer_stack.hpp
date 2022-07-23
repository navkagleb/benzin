#pragma once

#include "spieler/core/layer.hpp"

namespace spieler
{

    class LayerStack final
    {
    private:
        using LayerContainer = std::vector<std::shared_ptr<Layer>>;

    public:
        ~LayerStack();

    public:
        template <typename T, typename... Args>
        std::shared_ptr<T> Push(Args&&... args);

        template <typename T, typename... Args>
        std::shared_ptr<T> PushOverlay(Args&&... args);

        void Pop(const std::shared_ptr<Layer>& layer);
        void PopOverlay(const std::shared_ptr<Layer>& layer);

    public:
        LayerContainer::iterator Begin() { return m_Layers.begin(); };
        LayerContainer::iterator End() { return m_Layers.end(); };

        LayerContainer::reverse_iterator ReverseBegin() { return m_Layers.rbegin(); };
        LayerContainer::reverse_iterator ReverseEnd() { return  m_Layers.rend(); }

    private:
        LayerContainer m_Layers;
        size_t m_OverlayIndex{ 0 };
    };

    // For range based for
    inline auto begin(LayerStack& layerStack) { return layerStack.Begin(); }
    inline auto end(LayerStack& layerStack) { return layerStack.End(); }

} // namespace spieler

#include "layer_stack.inl"
