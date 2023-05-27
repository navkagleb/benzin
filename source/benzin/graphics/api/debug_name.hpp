#pragma once

namespace benzin::dx
{

    template <typename T>
    concept DXObject = std::derived_from<T, ID3D12Object> || std::derived_from<T, IDXGIObject>;

    template <DXObject T>
    std::string_view GetFormattedClassName()
    {
        // "struct " = 7 chars
        return std::string_view{ typeid(T).name() }.substr(7);
    }

    template <DXObject T>
    std::string GetDebugName(T* dxObject)
    {
        BENZIN_ASSERT(dxObject);

        uint32_t bufferSize = 128;
        std::string buffer;
        buffer.resize(bufferSize);

        if (SUCCEEDED(dxObject->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, buffer.data())))
        {
            buffer.resize(bufferSize);
            return buffer;
        }

        buffer.clear();
        return buffer;
    }

    template <DXObject T>
    void SetDebugName(T* dxObject, std::string_view debugName)
    {
        BENZIN_ASSERT(dxObject);

        if (debugName.empty())
        {
            return;
        }

        const bool isCreated = GetDebugName(dxObject).empty();

        const std::string formattedDebugName = fmt::format("{}[{}]", GetFormattedClassName<T>(), debugName);
        BENZIN_HR_ASSERT(dxObject->SetPrivateData(
            WKPDID_D3DDebugObjectName,
            static_cast<UINT>(formattedDebugName.size()),
            formattedDebugName.data()
        ));

        if (isCreated)
        {
            BENZIN_TRACE("{} is created", formattedDebugName);
        }
    }

    template <std::derived_from<IUnknown> T>
    void SafeRelease(T*& unknown)
    {
        if (!unknown)
        {
            return;
        }

        std::string debugName{ GetFormattedClassName<T>() };

        if constexpr (DXObject<T>)
        {
            std::string dxDebugName = GetDebugName(unknown);

            if (!dxDebugName.empty())
            {
                debugName = std::move(dxDebugName);
            }
        }

        const uint32_t referenceCount = unknown->Release();
        //BENZIN_ASSERT(referenceCount == 0);
        unknown = nullptr;

        BENZIN_TRACE("{} is released. RefCount: {}", debugName, referenceCount);
    }

} // namespace benzin::dx

#define BENZIN_DX_DEBUG_NAME_IMPL(d3d12Object) \
	std::string GetDebugName() const { return ::benzin::dx::GetDebugName(d3d12Object); } \
    void SetDebugName(std::string_view debugName) { ::benzin::dx::SetDebugName(d3d12Object, debugName); }   
