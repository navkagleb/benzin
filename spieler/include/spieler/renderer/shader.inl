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
    template <typename Permutations>
    Shader& ShaderLibrary::CreateShader(ShaderType type, const ShaderConfig<Permutations>& config)
    {
        // #TODO: Refactor ShaderLibrary
        //SPIELER_ASSERT(!HasShader(type, config.Permutation));

        return m_Shaders[type][config.Permutation.GetHash()] = Shader{ type, config.Filename, config.EntryPoint, config.Permutation.GetDefines() };
    }

    template <typename Permutations>
    bool ShaderLibrary::HasShader(ShaderType type, const ShaderPermutation<Permutations>& permutation) const
    {
        return m_Shaders.contains(type) && m_Shaders.at(type).contains(permutation.GetHash());
    }

    template <typename Permutations>
    const Shader& ShaderLibrary::GetShader(ShaderType type, const ShaderPermutation<Permutations>& permutation) const
    {
        SPIELER_ASSERT(HasShader(type, permutation));

        return m_Shaders.at(type).at(permutation.GetHash());
    }

} // namespace spieler::renderer