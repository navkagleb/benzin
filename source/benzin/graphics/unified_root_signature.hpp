#pragma once

namespace benzin
{

    class Device;

    enum class UnifiedRootParameter
    {
        RootConstantBuffer,
        FrameConstantBuffer,
        RenderPassConstantBuffer,
        TopLevelAs,
    };
    BenzinEnableUnaryPlusForEnum(UnifiedRootParameter);

    class UnifiedRootSignature
    {
    public:
        UnifiedRootSignature(Device& device);
        ~UnifiedRootSignature();

    public:
        auto* GetD3D12RootSignature() const { return m_D3D12RootSignature; }

    private:
        ID3D12RootSignature* m_D3D12RootSignature = nullptr;
    };

}
