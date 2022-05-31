namespace spieler::renderer
{

    // ShaderPermutation
    template <typename Permutations>
    template <Permutations Permutation, typename T>
    void ShaderPermutation<Permutations>::Set(T value)
    {
        const std::string_view key{ magic_enum::enum_name(Permutation) };

        if constexpr (std::is_same_v<bool, T>)
        {
            if (value)
            {
                m_Defines[key];
            }
        }
        else
        {
            m_Defines[key] = std::to_string(value);
        }

        return;
    }

    template <typename Permutations>
    uint64_t ShaderPermutation<Permutations>::GetHash() const
    {
        // TODO: Improve hash function
        uint64_t hash{ typeid(Permutations).hash_code() };

        for (const auto& [name, value] : m_Defines)
        {
            using NameType = std::decay_t<decltype(name)>;
            using ValueType = std::decay_t<decltype(value)>;

            hash += std::hash<NameType>{}(name) / std::hash<ValueType>{}(value);
        }

        return hash;
    }

    // ShaderLibrary
    template <ShaderType Type, typename Permutations>
    Shader& ShaderLibrary::CreateShader(const std::wstring& filename, const std::string& entryPoint, const ShaderPermutation<Permutations>& permutation)
    {
        SPIELER_ASSERT(!HasShader<Type>(permutation));

        const Shader::Config config
        {
            .Type = Type,
            .Filename = filename,
            .EntryPoint = entryPoint,
            .Defines = permutation.GetDefines()
        };

        return m_Shaders[Type][permutation.GetHash()] = config;
    }

    template <ShaderType Type, typename Permutations>
    bool ShaderLibrary::HasShader(const ShaderPermutation<Permutations>& permutation) const
    {
        return m_Shaders.contains(Type) && m_Shaders.at(Type).contains(permutation.GetHash());
    }

    template <ShaderType Type, typename Permutations>
    const Shader& ShaderLibrary::GetShader(const ShaderPermutation<Permutations>& permutation) const
    {
        SPIELER_ASSERT(HasShader<Type>(permutation));

        return m_Shaders.at(Type).at(permutation.GetHash());
    }

} // namespace spieler::renderer