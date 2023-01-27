#include "benzin/config/bootstrap.hpp"

#include "benzin/core/layer_stack.hpp"

#include "benzin/core/common.hpp"
#include "benzin/system/event.hpp"

namespace benzin
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
            BENZIN_ASSERT((*layerIterator)->OnDetach());

            m_Layers.erase(layerIterator);
            m_OverlayIndex--;
        }
    }

    void LayerStack::PopOverlay(const std::shared_ptr<Layer>& layer)
    {
        auto layerIterator{ std::find(m_Layers.begin() + m_OverlayIndex, m_Layers.end(), layer) };

        if (layerIterator != m_Layers.end())
        {
            BENZIN_ASSERT((*layerIterator)->OnDetach());

            m_Layers.erase(layerIterator);
        }
    }

} // namespace benzin