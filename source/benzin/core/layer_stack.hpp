#pragma once

namespace benzin
{

    class Layer;

    class LayerStack
    {
    private:
        using LayerContainer = std::vector<std::unique_ptr<Layer>>;

    public:
        template <std::derived_from<Layer> LayerT, typename... Args>
        LayerT* const Push(Args&&... args)
        {
            auto layer = std::make_unique<LayerT>(std::forward<Args>(args)...);
            auto* rawLayer = layer.get();

            m_Layers.insert(m_Layers.begin() + m_OverlayIndex++, std::move(layer));

            return rawLayer;
        }

        template <std::derived_from<Layer> LayerT, typename... Args>
        LayerT* const PushOverlay(Args&&... args)
        {
            auto layer = std::make_unique<LayerT>(std::forward<Args>(args)...);
            auto* rawLayer = layer.get();

            m_Layers.push_back(std::move(layer));

            return rawLayer;
        }

        template <std::derived_from<Layer> LayerT>
        void Pop(LayerT*& rawLayer)
        {
            const auto layerIterator = std::find_if(m_Layers.begin(), m_Layers.begin() + m_OverlayIndex, [rawLayer](const auto& layer)
            {
                return layer.get() == rawLayer;
            });

            if (layerIterator != m_Layers.end())
            {
                m_Layers.erase(layerIterator);
                m_OverlayIndex--;

                rawLayer = nullptr;
            }
        }

        template <std::derived_from<Layer> LayerT>
        void PopOverlay(LayerT*& rawLayer)
        {
            const auto layerIterator = std::find_if(m_Layers.begin() + m_OverlayIndex, m_Layers.end(), [rawLayer](const auto& layer)
            {
                return layer.get() == rawLayer;
            });

            if (layerIterator != m_Layers.end())
            {
                m_Layers.erase(layerIterator);

                rawLayer = nullptr;
            }
        }

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
