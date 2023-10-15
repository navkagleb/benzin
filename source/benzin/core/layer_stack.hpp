#pragma once

#include "benzin/core/layer.hpp"

namespace benzin
{

    class LayerStack final
    {
    private:
        using LayerContainer = std::vector<std::shared_ptr<Layer>>;

    public:
        template <typename T, typename... Args>
        std::shared_ptr<T> Push(Args&&... args)
        {
            const auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            m_Layers.insert(m_Layers.begin() + m_OverlayIndex++, layer);

            return layer;
        }

        template <typename T, typename... Args>
        std::shared_ptr<T> PushOverlay(Args&&... args)
        {
            const auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            m_Layers.push_back(layer);

            return layer;
        }

        void Pop(const std::shared_ptr<Layer>& layer);
        void PopOverlay(const std::shared_ptr<Layer>& layer);

    public:
        LayerContainer::iterator Begin() { return m_Layers.begin(); };
        LayerContainer::iterator End() { return m_Layers.end(); };

    private:
        LayerContainer m_Layers;
        size_t m_OverlayIndex = 0;
    };

    // For range based for
    inline auto begin(LayerStack& layerStack) { return layerStack.Begin(); }
    inline auto end(LayerStack& layerStack) { return layerStack.End(); }

} // namespace benzin
