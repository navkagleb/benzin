namespace spieler::renderer
{

    template <concepts::ResourceView View>
    bool ViewContainer::HasView() const
    {
        SPIELER_ASSERT(m_Resource.GetDX12Resource());

        return m_Views.contains(typeid(View).hash_code());
    }

    template <concepts::ResourceView View>
    const View& ViewContainer::GetView() const
    {
        SPIELER_ASSERT(m_Resource.GetDX12Resource());
        SPIELER_ASSERT(HasView<View>());

        return std::get<View>(m_Views.at(typeid(View).hash_code()));
    }

    template <concepts::BufferView View>
    void ViewContainer::CreateView()
    {
        SPIELER_ASSERT(m_Resource.GetDX12Resource());
        SPIELER_ASSERT(!HasView<View>());

        m_Views[typeid(View).hash_code()] = View{ static_cast<const BufferResource&>(m_Resource) };
    }

    template <concepts::DescriptorView View>
    void ViewContainer::CreateView(Device& device)
    {
        SPIELER_ASSERT(m_Resource.GetDX12Resource());
        SPIELER_ASSERT(!HasView<View>());

        m_Views[typeid(View).hash_code()] = View{ device, m_Resource };
    }

} // namespace spieler::renderer
