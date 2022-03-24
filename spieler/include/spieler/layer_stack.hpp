#pragma once

#include <vector>
#include <memory>

#include "layer.hpp"

namespace Spieler
{

    class LayerStack final
    {
    public:
        void OnEvent(Event& event);
        void OnUpdate(float dt);
        bool OnRender(float dt);
        void OnImGuiRender(float dt);

        template <typename T, typename... Args>
        std::shared_ptr<T> PushLayer(Args&&... args);

        bool PopLayer(const std::shared_ptr<Layer>& layer);

    private:
        std::vector<std::shared_ptr<Layer>> m_Layers;
    };

    template <typename T, typename... Args>
    std::shared_ptr<T> LayerStack::PushLayer(Args&&... args)
    {
        auto layer = std::make_shared<T>(std::forward<Args>(args)...);

        if (!layer->OnAttach())
        {
            return nullptr;
        }

        m_Layers.push_back(layer);

        return layer;
    }

} // namespace Spieler