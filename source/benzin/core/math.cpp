#include <benzin/config/bootstrap.hpp>
#include <benzin/core/math.hpp>

namespace benzin
{

    bool IsMatrixEqual(const DirectX::XMMATRIX lhs, const DirectX::XMMATRIX& rhs, float epsilon)
    {
        using namespace DirectX;

        for (uint32_t i = 0; i < 4; ++i)
        {
            const XMVECTOR diff = XMVectorAbs(XMVectorSubtract(lhs.r[i], rhs.r[i]));
            if (!XMVector4LessOrEqual(diff, XMVectorReplicate(epsilon)))
            {
                return false;
            }
        }

        return true;
    }

}
