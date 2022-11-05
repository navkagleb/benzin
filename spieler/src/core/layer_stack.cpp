#include "spieler/config/bootstrap.hpp"

#include "spieler/core/layer_stack.hpp"

#include "spieler/core/common.hpp"
#include "spieler/system/event.hpp"

namespace spieler
{

    LayerStack::~LayerStack()
    {
        for (auto& layer : m_Layers)
        {
            layer->OnDetach();
        }
    }

    void LayerStack::Pop(const std::shared_ptr<Layer>& layer)
    {
        auto layerIterator{ std::find(m_Layers.begin(), m_Layers.begin() + m_OverlayIndex, layer) };

        if (layerIterator != m_Layers.end())
        {
            SPIELER_ASSERT((*layerIterator)->OnDetach());

            m_Layers.erase(layerIterator);
            m_OverlayIndex--;
        }
    }

    void LayerStack::PopOverlay(const std::shared_ptr<Layer>& layer)
    {
        auto layerIterator{ std::find(m_Layers.begin() + m_OverlayIndex, m_Layers.end(), layer) };

        if (layerIterator != m_Layers.end())
        {
            SPIELER_ASSERT((*layerIterator)->OnDetach());

            m_Layers.erase(layerIterator);
        }
    }

} // namespace spieler