#pragma once

namespace benzin
{

    class Device;

    enum class UnifiedRootParameter
    {
        Root32Consts,
        FrameConsts,
        RenderPassConsts,
        GpuPrintConsts,
        SceneTlas,
        ReadbackStatsBuffer,
    };

    class UnifiedRootSignature
    {
    public:
        UnifiedRootSignature(Device& device);
        ~UnifiedRootSignature();

        BenzinDefineNonCopyable(UnifiedRootSignature);
        BenzinDefineNonMoveable(UnifiedRootSignature);

        auto* GetD3D12RootSignature() const { return m_D3D12RootSignature; }

    private:
        ID3D12RootSignature* m_D3D12RootSignature = nullptr;
    };

}

BenzinAllowDereferenceOperatorForEnum(benzin::UnifiedRootParameter);
