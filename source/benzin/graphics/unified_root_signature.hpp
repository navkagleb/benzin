#pragma once

namespace benzin
{

    class Device;

    enum class UnifiedRootParameter
    {
        RootConstantBuffer,
        FrameConstantBuffer,
        RenderPassConstantBuffer0,
        RenderPassConstantBuffer1,
        LightStructuredBuffer,
        SceneTlas,
    };
    BenzinEnableUnaryPlusForEnum(UnifiedRootParameter);

    class UnifiedRootSignature
    {
    public:
        UnifiedRootSignature(Device& device);
        ~UnifiedRootSignature();

        BenzinDefineNonCopyable(UnifiedRootSignature);
        BenzinDefineNonMoveable(UnifiedRootSignature);

    public:
        auto* GetD3D12RootSignature() const { return m_D3D12RootSignature; }

    private:
        ID3D12RootSignature* m_D3D12RootSignature = nullptr;
    };

}
