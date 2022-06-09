#pragma once

namespace sandbox
{

    template <>
    inline const spieler::renderer::Shader& TestLayer::GetShader(spieler::renderer::ShaderType type, const spieler::renderer::ShaderPermutation<per::Color>& permutation)
    {
        static const std::unordered_map<spieler::renderer::ShaderType, std::string> entryPoints
        {
            { spieler::renderer::ShaderType::Vertex, "VS_Main" },
            { spieler::renderer::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/color2.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::renderer::ShaderConfig<per::Color> config
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
    inline const spieler::renderer::Shader& TestLayer::GetShader(spieler::renderer::ShaderType type, const spieler::renderer::ShaderPermutation<per::Texture>& permutation)
    {
        static const std::unordered_map<spieler::renderer::ShaderType, std::string> entryPoints
        {
            { spieler::renderer::ShaderType::Vertex, "VS_Main" },
            { spieler::renderer::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/texture.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::renderer::ShaderConfig<per::Texture> config
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
    inline const spieler::renderer::Shader& TestLayer::GetShader(spieler::renderer::ShaderType type, const spieler::renderer::ShaderPermutation<per::Billboard>& permutation)
    {
        static const std::unordered_map<spieler::renderer::ShaderType, std::string> entryPoints
        {
            { spieler::renderer::ShaderType::Vertex, "VS_Main" },
            { spieler::renderer::ShaderType::Pixel, "PS_Main" },
            { spieler::renderer::ShaderType::Geometry, "GS_Main" }
        };

        static const std::wstring filename{ L"assets/shaders/billboard.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const spieler::renderer::ShaderConfig<per::Billboard> config
            {
                .Filename = filename,
                .EntryPoint = entryPoints.at(type),
                .Permutation = permutation
            };

            return m_ShaderLibrary.CreateShader(type, config);
        }

        return m_ShaderLibrary.GetShader(type, permutation);
    }

} // namespace sandbox