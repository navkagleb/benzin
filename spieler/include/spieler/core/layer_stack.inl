namespace spieler
{

    template <typename T, typename... Args>
    std::shared_ptr<T> LayerStack::Push(Args&&... args)
    {
        std::shared_ptr<T> layer{ std::make_shared<T>(std::forward<Args>(args)...) };

        SPIELER_ASSERT(layer->OnAttach());

        m_Layers.emplace(m_Layers.begin() + m_OverlayIndex++, layer);

        return layer;
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> LayerStack::PushOverlay(Args&&... args)
    {
        std::shared_ptr<T> layer{ *std::make_unique<T>(std::forward<Args>(args)...) };

        SPIELER_ASSERT(layer->OnAttach());

        m_Layers.push_back(layer);

        return layer;
    }

} // namespace spieler
