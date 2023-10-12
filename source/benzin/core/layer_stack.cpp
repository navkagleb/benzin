#include "benzin/config/bootstrap.hpp"
#include "benzin/core/layer_stack.hpp"

#include "benzin/system/event.hpp"

namespace benzin
{

    void LayerStack::Pop(const std::shared_ptr<Layer>& layer)
    {
        const auto layerIterator = std::find(m_Layers.begin(), m_Layers.begin() + m_OverlayIndex, layer);

        if (layerIterator != m_Layers.end())
        {
            m_Layers.erase(layerIterator);
            m_OverlayIndex--;
        }
    }

    void LayerStack::PopOverlay(const std::shared_ptr<Layer>& layer)
    {
        const auto layerIterator = std::find(m_Layers.begin() + m_OverlayIndex, m_Layers.end(), layer);

        if (layerIterator != m_Layers.end())
        {
            m_Layers.erase(layerIterator);
        }
    }

} // namespace benzin
