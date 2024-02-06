#pragma once

namespace benzin
{

    class CommandLineArgs
    {
    public:
        CommandLineArgs(int argc, char** argv);

    public:
        const auto& GetExecutablePath() const { return m_ExecutablePath; }

        auto GetWindowWidth() const { return m_WindowWidth; }
        auto GetWindowHeight() const { return m_WindowHeight; }
        auto IsWindowResizable() const { return m_IsWindowResizable; }

        auto GetAdapterIndex() const { return m_AdapterIndex; }
        auto GetFrameInFlightCount() const { return m_FrameInFlightCount; }
        auto IsEnabledGPUBasedValidation() const { return m_IsEnabledGPUBasedValidation; }

    private:
        std::filesystem::path m_ExecutablePath;

        uint32_t m_WindowWidth = 1280;
        uint32_t m_WindowHeight = 720;
        bool m_IsWindowResizable = true;

        std::optional<uint32_t> m_AdapterIndex; // #TODO: Remove optional
        std::optional<uint32_t> m_FrameInFlightCount; // #TODO: Remove optional
        bool m_IsEnabledGPUBasedValidation = false;
    };

    using CommandLineArgsInstance = SingletonInstanceWrapper<CommandLineArgs>;

} // namespace benzin
