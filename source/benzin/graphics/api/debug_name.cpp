#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/debug_name.hpp"

#if 0
namespace benzin
{

    namespace
    {

        const uint32_t g_MaxDebugNameSize = 128;

        template <typename T>
        concept DXNameable = std::is_same_v<T, ID3D12Object> || std::is_same_v<T, IDXGIObject>;

        template <DXNameable T>
        std::string GetDebugName(T* dxObject)
        {
            BENZIN_ASSERT(dxObject);

            uint32_t bufferSize = g_MaxDebugNameSize;
            std::string buffer;
            name.resize(bufferSize);

            if (SUCCEEDED(dxObject->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, buffer.data())))
            {
                name.resize(bufferSize);
                return name;
            }

            name.clear();
            return name;
        }

        template <DXNameable T>
        void SetDebugName(T* dxObject, std::string_view     )
        {
            BENZIN_ASSERT(dxObject);

            if (debugName.empty())
            {
                return;
            }

            const bool isCreated = GetDebugName(dxObject).empty();

            BENZIN_HR_ASSERT(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(debugName.size()), debugName.data()));

            if (isCreated)
            {
                BENZIN_TRACE("{} is created: {}", detail::g_D3D12ObjectStyledString, name.data());
            }
        }

    } // anonymous namespace

    namespace d3d12
    {

        std::string GetDebugName(ID3D12Object* d3d12Object)
        {
            return GetDebugName(d3d12Object);
        }

        void SetDebugName(ID3D12Object* d3d12Object, std::string_view debugName)
        {
            return SetDebugName(d3d12Object, debugName);
        }

    } // namespace detail

    namespace dxgi
    {

        std::string GetDebugName(IDXGIObject* dxgiObject)
        {
            return GetDebugName(dxgiObject);
        }

        void SetDebugName(IDXGIObject* dxgiObject, std::string_view debugName)
        {
            return SetDebugName(dxgiObject, debugName);
        }

    } // namespace dxgi

    namespace dx
    {

        void SafeRelease(IUnknown*& unknown)
        {
            if (!unknown)
            {
                return;
            }

            std::string debugName;

            {
                ComPtr<ID3D12Object> d3d12Object;
                if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&d3d12Object))))
                {

                }
            }
            
            {
                ComPtr<ID3D12Object> d3d12Object;
                if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&d3d12Object))))
                {

                }
            }

            std::string debugName = detail::GetD3D12ObjectDebugName(unknown);

            if (debugName.empty())
            {
                debugName = detail::GetD3D12ClassName<T>();
            }

            const uint32_t referenceCount = unknown->Release();
            //BENZIN_ASSERT(referenceCount == 0);
            unknown = nullptr;

            BENZIN_TRACE("{} is released: {}. RefCount: {}", detail::g_D3D12ObjectStyledString, debugName, referenceCount);
        }

    } // namespace dx

} // namespace benzin
#endif
