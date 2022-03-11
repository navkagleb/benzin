#include "fence.h"

namespace Spieler
{

    bool Fence::Init()
    {
        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateFence(Value, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), &Handle));

        return true;
    }

} // namespace Spieler