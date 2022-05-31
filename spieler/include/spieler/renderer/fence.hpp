#pragma once

namespace spieler::renderer
{

    class Device;
    class Context;

    class Fence
    {
    public:
        Fence(Device& device);

    public:
        uint64_t GetValue() const { return m_Value; }

    public:
        void Increment();
        bool Signal(Context& context) const;
        bool WaitForGPU() const;

    private:
        bool Init(Device& device);

    private:
        ComPtr<ID3D12Fence> m_Fence;

        uint64_t m_Value{ 0 };
    };

} // namespace spieler::renderer