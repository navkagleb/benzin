#include <d3dx12.h>

namespace Spieler
{

    template <typename T>
    void Resource::CopyDataToDefaultBuffer(ComPtr<ID3D12Resource>& buffer, const ComPtr<ID3D12Resource>& uploadBuffer, const T* data, std::uint32_t count)
    {
        D3D12_SUBRESOURCE_DATA subResourceData{};
        subResourceData.pData       = data;
        subResourceData.RowPitch    = sizeof(T) * count;
        subResourceData.SlicePitch  = subResourceData.RowPitch;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource    = buffer.Get();
        barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_COMMON;
        barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        GetCommandList()->ResourceBarrier(1, &barrier);

        UpdateSubresources<1>(GetCommandList().Get(), buffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_GENERIC_READ;

        GetCommandList()->ResourceBarrier(1, &barrier);
    }

} // namespace Spieler