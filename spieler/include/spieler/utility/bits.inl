namespace spieler
{

    template <typename T>
    bool Bits<T>::IsSet(T bit) const
    {
        return m_Bits[static_cast<UnderlyingType>(bit)];
    }

    template <typename T>
    void Bits<T>::Set(T bit, bool value)
    {
        m_Bits[static_cast<UnderlyingType>(bit)] = value;
    }

} // namespace spieler