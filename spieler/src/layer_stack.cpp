#include "layer_stack.hpp"

#include "common.hpp"
#include "window/event.hpp"

namespace Spieler
{

    void LayerStack::OnEvent(Event& event)
    {
        for (auto& layer : m_Layers)
        {
            layer->OnEvent(event);
        }
    }

    void LayerStack::OnUpdate(float dt)
    {
        for (auto& layer : m_Layers)
        {
            layer->OnUpdate(dt);
        }
    }

    bool LayerStack::OnRender(float dt)
    {
        for (auto& layer : m_Layers)
        {
            SPIELER_RETURN_IF_FAILED(layer->OnRender(dt));
        }

        return true;
    }

    void LayerStack::OnImGuiRender(float dt)
    {
        for (auto& layer : m_Layers)
        {
            layer->OnImGuiRender(dt);
        }
    }

    bool LayerStack::PopLayer(const std::shared_ptr<Layer>& layer)
    {
        SPIELER_RETURN_IF_FAILED(layer->OnDetach());

        m_Layers.erase(std::remove(m_Layers.begin(), m_Layers.end(), layer), m_Layers.end());

        return true;
    }

} // namespace Spieler