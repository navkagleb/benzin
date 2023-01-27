#pragma once

namespace sandbox
{

#if 0
    template <>
    inline const benzin::Shader& TestLayer::GetShader(benzin::ShaderType type, const benzin::ShaderPermutation<per::Color>& permutation)
    {
        static const std::unordered_map<benzin::ShaderType, std::string> entryPoints
        {
            { benzin::ShaderType::Vertex, "VS_Main" },
            { benzin::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/color2.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const benzin::ShaderConfig<per::Color> config
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
    inline const benzin::Shader& TestLayer::GetShader(benzin::ShaderType type, const benzin::ShaderPermutation<per::Texture>& permutation)
    {
        static const std::unordered_map<benzin::ShaderType, std::string> entryPoints
        {
            { benzin::ShaderType::Vertex, "VS_Main" },
            { benzin::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/texture.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const benzin::ShaderConfig<per::Texture> config
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
    inline const benzin::Shader& TestLayer::GetShader(benzin::ShaderType type, const benzin::ShaderPermutation<per::Billboard>& permutation)
    {
        static const std::unordered_map<benzin::ShaderType, std::string> entryPoints
        {
            { benzin::ShaderType::Vertex, "VS_Main" },
            { benzin::ShaderType::Pixel, "PS_Main" },
            { benzin::ShaderType::Geometry, "GS_Main" }
        };

        static const std::wstring filename{ L"assets/shaders/billboard.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const benzin::ShaderConfig<per::Billboard> config
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
    inline const benzin::Shader& TestLayer::GetShader(benzin::ShaderType type, const benzin::ShaderPermutation<per::Composite>& permutation)
    {
        static const std::unordered_map<benzin::ShaderType, std::string> entryPoints
        {
            { benzin::ShaderType::Vertex, "VS_Main" },
            { benzin::ShaderType::Pixel, "PS_Main" },
        };

        static const std::wstring filename{ L"assets/shaders/composite.hlsl" };

        if (!m_ShaderLibrary.HasShader(type, permutation))
        {
            const benzin::ShaderConfig<per::Composite> config
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