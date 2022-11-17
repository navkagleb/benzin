#pragma once

namespace sandbox
{

#if 0
    template <>
    inline const spieler::Shader& TestLayer::GetShader(spieler::ShaderType type, const spieler::ShaderPermutation<per::Color>& permutation)
    {
        static const std::unordered_map<spieler::ShaderType, std::string> entryPoints
        {
            { spieler::ShaderType::Vertex, "VS_Main" },
            { spieler::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/color2.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::ShaderConfig<per::Color> config
            {
                .Filename = filename,
                .EntryPoint = entryPoints.at(type),
                .Permutation = permutation
            };

            return m_ShaderLibrary.CreateShader(type, config);
        }

        return m_ShaderLibrary.GetShader(type, permutation);
    }

    template <>
    inline const spieler::Shader& TestLayer::GetShader(spieler::ShaderType type, const spieler::ShaderPermutation<per::Texture>& permutation)
    {
        static const std::unordered_map<spieler::ShaderType, std::string> entryPoints
        {
            { spieler::ShaderType::Vertex, "VS_Main" },
            { spieler::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/texture.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::ShaderConfig<per::Texture> config
            {
                .Filename = filename,
                .EntryPoint = entryPoints.at(type),
                .Permutation = permutation
            };

            return m_ShaderLibrary.CreateShader(type, config);
        }

        return m_ShaderLibrary.GetShader(type, permutation);
    }

    template <>
    inline const spieler::Shader& TestLayer::GetShader(spieler::ShaderType type, const spieler::ShaderPermutation<per::Billboard>& permutation)
    {
        static const std::unordered_map<spieler::ShaderType, std::string> entryPoints
        {
            { spieler::ShaderType::Vertex, "VS_Main" },
            { spieler::ShaderType::Pixel, "PS_Main" },
            { spieler::ShaderType::Geometry, "GS_Main" }
        };

        static const std::wstring filename{ L"assets/shaders/billboard.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::ShaderConfig<per::Billboard> config
            {
                .Filename = filename,
                .EntryPoint = entryPoints.at(type),
                .Permutation = permutation
            };

            return m_ShaderLibrary.CreateShader(type, config);
        }

        return m_ShaderLibrary.GetShader(type, permutation);
    }

    template <>
    inline const spieler::Shader& TestLayer::GetShader(spieler::ShaderType type, const spieler::ShaderPermutation<per::Composite>& permutation)
    {
        static const std::unordered_map<spieler::ShaderType, std::string> entryPoints
        {
            { spieler::ShaderType::Vertex, "VS_Main" },
            { spieler::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/composite.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::ShaderConfig<per::Composite> config
            {
                .Filename = filename,
                .EntryPoint = entryPoints.at(type),
                .Permutation = permutation
            };

            return m_ShaderLibrary.CreateShader(type, config);
        }

        return m_ShaderLibrary.GetShader(type, permutation);
    }
#endif

} // namespace sandbox