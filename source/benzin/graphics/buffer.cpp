#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/buffer.hpp>

#include <benzin/core/math.hpp>
#include <benzin/graphics/d3d12_assert.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

namespace benzin
{

    struct BufferSrv {};
    struct BufferUav {};

    static D3D12_RESOURCE_DESC ToD3D12ResourceDesc(const BufferCreation& creation)
    {
        BenzinAssert(creation.ElementSizeInBytes != 0);
        BenzinAssert(creation.ElementCount != 0);

        switch (creation.Type)
        {
            case BufferType::Format:
            {
                BenzinAssert(creation.ElementSizeInBytes == GetFormatSizeInBytes(creation.Format));
                break;
            }
            case BufferType::Const:
            {
                BenzinEnsure(creation.ElementSizeInBytes % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0);
                break;
            }
            case BufferType::Structured:
            {
                // Performance tip: Align structures on sizeof(float4) boundary
                // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance

                constexpr uint32_t alignment = sizeof(DirectX::XMFLOAT4);

                BenzinWarningIf(
                    creation.ElementSizeInBytes % alignment != 0,
                    "Buffer '{}' is not properly aligned. BufferElementSize: {}, StructuredBufferAlignment: {}",
                    creation.DebugName,
                    creation.ElementSizeInBytes,
                    alignment);

                break;
            }
        }

        D3D12_RESOURCE_FLAGS d3d12ResourceFlags = D3D12_RESOURCE_FLAG_NONE;
        if (creation.IsUnorderedAccessAllowed)
        {
            d3d12ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
        d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        d3d12ResourceDesc.Alignment = 0;
        d3d12ResourceDesc.Width = creation.ElementSizeInBytes * creation.ElementCount;
        d3d12ResourceDesc.Height = 1;
        d3d12ResourceDesc.DepthOrArraySize = 1;
        d3d12ResourceDesc.MipLevels = 1;
        d3d12ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        d3d12ResourceDesc.SampleDesc = { 1, 0 };
        d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        d3d12ResourceDesc.Flags = d3d12ResourceFlags;

        return d3d12ResourceDesc;
    }

    static D3D12_RESOURCE_STATES GetInitialBufferState(const Device& device, BufferType bufferType, GpuHeapType heapType)
    {
        if (bufferType == BufferType::RayTracing_AccelerationStructure)
            return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

        if (heapType == GpuHeapType::Upload && !device.GetCaps().IsGpuUploadHeapsSupported) // TODO: Don't check if GpuUpload heaps are supported here
        {
            // Case only for D3D12_HEAP_TYPE_UPLOAD
            // D3D12_HEAP_TYPE_GPU_UPLOAD requires D3D12_RESOURCE_STATE_COMMON
            return D3D12_RESOURCE_STATE_GENERIC_READ;
        }

        if (heapType == GpuHeapType::Readback)
            return D3D12_RESOURCE_STATE_COPY_DEST;

        return D3D12_RESOURCE_STATE_COMMON;
    }

    static ID3D12Resource* CreateCommittedD3D12Resource(const Device& device, const BufferCreation& creation, D3D12_RESOURCE_STATES d3d12InitialState)
    {
        BenzinAssert(!IsMaxEnum(creation.HeapType));

        const D3D12_HEAP_PROPERTIES d3d12HeapProperties = GetD3D12HeapProperties(ToD3D12HeapType(device, creation.HeapType));
        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(creation);

        ID3D12Resource* d3d12Resource = nullptr;
        BenzinD3D12Call(device.GetD3D12Device()->CreateCommittedResource(
            &d3d12HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &d3d12ResourceDesc,
            d3d12InitialState,
            nullptr,
            IID_PPV_ARGS(&d3d12Resource)));

        BenzinEnsure(d3d12Resource != nullptr);
        return d3d12Resource;
    }

    static ID3D12Resource* CreatePlacedD3D12Resource(
        const Device& device,
        const GpuHeap& gpuHeap,
        uint64_t gpuHeapOffsetInBytes,
        const BufferCreation& creation,
        D3D12_RESOURCE_STATES d3d12InitialState)
    {
        BenzinAssert(IsMaxEnum(creation.HeapType));

        const D3D12_RESOURCE_DESC d3d12ResourceDesc = ToD3D12ResourceDesc(creation);

        ID3D12Resource* d3d12Resource = nullptr;
        BenzinD3D12Call(device.GetD3D12Device()->CreatePlacedResource(
            gpuHeap.GetD3D12Heap(),
            gpuHeapOffsetInBytes,
            &d3d12ResourceDesc,
            d3d12InitialState,
            nullptr,
            IID_PPV_ARGS(&d3d12Resource)));

        BenzinEnsure(d3d12Resource != nullptr);
        return d3d12Resource;
    }

    static D3D12_SHADER_RESOURCE_VIEW_DESC ToD3D12ShaderResoureViewDesc(const Buffer& buffer)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = {};
        d3d12SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (buffer.GetType())
        {
            case BufferType::Byte:
            {
                // Note: ByteAddressBuffers supports only 'DXGI_FORMAT_R32_TYPELESS' format 
                // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro#raw-views-of-buffers

                BenzinAssert(buffer.GetSizeInBytes() % sizeof(uint32_t) == 0);

                d3d12SrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                d3d12SrvDesc.Buffer.FirstElement = 0;
                d3d12SrvDesc.Buffer.NumElements = (uint32_t)(buffer.GetSizeInBytes() / sizeof(uint32_t));
                d3d12SrvDesc.Buffer.StructureByteStride = 0;
                d3d12SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

                break;
            }
            case BufferType::Format:
            {
                BenzinAssert(buffer.GetFormat() != GraphicsFormat::Unknown);

                d3d12SrvDesc.Format = (DXGI_FORMAT)buffer.GetFormat();
                d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                d3d12SrvDesc.Buffer.FirstElement = 0;
                d3d12SrvDesc.Buffer.NumElements = (uint32_t)buffer.GetElementCount();
                d3d12SrvDesc.Buffer.StructureByteStride = 0;
                d3d12SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

                break;
            }
            case BufferType::Structured:
            {
                // Ref: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_buffer_srv#remarks

                d3d12SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
                d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                d3d12SrvDesc.Buffer.FirstElement = 0;
                d3d12SrvDesc.Buffer.NumElements = (uint32_t)buffer.GetElementCount();
                d3d12SrvDesc.Buffer.StructureByteStride = buffer.GetElementSizeInBytes();
                d3d12SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

                break;
            }
            case BufferType::RayTracing_AccelerationStructure:
            {
                d3d12SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
                d3d12SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                d3d12SrvDesc.RaytracingAccelerationStructure.Location = buffer.GetGpuVirtualAddress();

                break;
            }
            default:
            {
                BenzinEnsure(
                    false,
                    "Unknown BufferType for SRV: {} ({})",
                    magic_enum::enum_name(buffer.GetType()),
                    magic_enum::enum_integer(buffer.GetType()));
                break;
            }
        }

        return d3d12SrvDesc;
    }

    static D3D12_UNORDERED_ACCESS_VIEW_DESC ToD3D12UnorderedAccessViewDesc(const Buffer& buffer)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = {};
        d3d12UavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        switch (buffer.GetType())
        {
            case BufferType::Byte:
            {
                // Note: ByteAddressBuffers supports only 'DXGI_FORMAT_R32_TYPELESS' format 
                // Ref: https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-intro#raw-views-of-buffers

                BenzinAssert(buffer.GetSizeInBytes() % sizeof(uint32_t) == 0);

                d3d12UavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                d3d12UavDesc.Buffer.FirstElement = 0;
                d3d12UavDesc.Buffer.NumElements = (uint32_t)(buffer.GetSizeInBytes() / sizeof(uint32_t));
                d3d12UavDesc.Buffer.StructureByteStride = 0;
                d3d12UavDesc.Buffer.CounterOffsetInBytes = 0;
                d3d12UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

                break;
            }
            case BufferType::Format:
            {
                BenzinAssert(buffer.GetFormat() != GraphicsFormat::Unknown);

                d3d12UavDesc.Format = (DXGI_FORMAT)buffer.GetFormat();
                d3d12UavDesc.Buffer.FirstElement = 0;
                d3d12UavDesc.Buffer.NumElements = (uint32_t)buffer.GetElementCount();
                d3d12UavDesc.Buffer.StructureByteStride = 0;
                d3d12UavDesc.Buffer.CounterOffsetInBytes = 0;
                d3d12UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

                break;
            }
            case BufferType::Structured:
            {
                d3d12UavDesc.Format = DXGI_FORMAT_UNKNOWN;
                d3d12UavDesc.Buffer.FirstElement = 0;
                d3d12UavDesc.Buffer.NumElements = (uint32_t)buffer.GetElementCount();
                d3d12UavDesc.Buffer.StructureByteStride = buffer.GetElementSizeInBytes();
                d3d12UavDesc.Buffer.CounterOffsetInBytes = 0;
                d3d12UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

                break;
            }
            default:
            {
                BenzinEnsure(
                    false,
                    "Unknown BufferType for UAV: {} ({})",
                    magic_enum::enum_name(buffer.GetType()),
                    magic_enum::enum_integer(buffer.GetType()));
                break;
            }
        }

        return d3d12UavDesc;
    }

    //

    Buffer::Buffer(Device& device, const BufferCreation& creation)
        : Resource{ device }
    {
        m_D3D12CurrentState = GetInitialBufferState(m_Device, creation.Type, creation.HeapType);
        m_D3D12Resource = CreateCommittedD3D12Resource(m_Device, creation, m_D3D12CurrentState);

        SetupCreation(creation);
    }

    Buffer::Buffer(GpuHeap& gpuHeap, uint64_t gpuHeapOffsetInBytes, const BufferCreation& creation)
        : Resource{ gpuHeap.m_Device }
    {
        m_D3D12CurrentState = GetInitialBufferState(m_Device, creation.Type, gpuHeap.GetType());
        m_D3D12Resource = CreatePlacedD3D12Resource(m_Device, gpuHeap, gpuHeapOffsetInBytes, creation, m_D3D12CurrentState);

        SetupCreation(creation, &gpuHeap);
    }

    Buffer::~Buffer()
    {
        if (m_CpuMappedData != nullptr)
        {
            m_D3D12Resource->Unmap(0, nullptr);
        }
    }

    uint64_t Buffer::GetSizeInBytes() const
    {
        return m_ElementSizeInBytes * m_ElementCount;
    }

    uint64_t Buffer::GetGpuVirtualAddress(uint32_t elementIndex) const
    {
        BenzinAssert(m_D3D12Resource != nullptr);
        BenzinAssert(elementIndex < m_ElementCount);

        return m_D3D12Resource->GetGPUVirtualAddress() + elementIndex * m_ElementSizeInBytes;
    }

    const Descriptor& Buffer::GetSrv() const
    {
        return TryGetViewDescriptor(
            GetStdHash(BufferSrv{}),
            [&] { return CreateDetachedSrv(); });
    }

    const Descriptor& Buffer::GetUav() const
    {
        return TryGetViewDescriptor(
            GetStdHash(BufferUav{}),
            [&] { return CreateDetachedUav(); });
    }

    Descriptor Buffer::CreateDetachedSrv() const
    {
        ID3D12Resource* d3d12Resource = nullptr;
        if (m_Type != BufferType::RayTracing_AccelerationStructure)
        {
            BenzinAssert(m_D3D12Resource != nullptr);
            d3d12Resource = m_D3D12Resource;
        }

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv, [&](uint64_t cpuHandle)
        {
            const D3D12_SHADER_RESOURCE_VIEW_DESC d3d12SrvDesc = ToD3D12ShaderResoureViewDesc(*this);

            m_Device.GetD3D12Device()->CreateShaderResourceView(
                d3d12Resource,
                &d3d12SrvDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    Descriptor Buffer::CreateDetachedUav() const
    {
        BenzinAssert(m_IsUnorderedAccessAllowed);

        return m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Uav, [&](uint64_t cpuHandle)
        {
            const D3D12_UNORDERED_ACCESS_VIEW_DESC d3d12UavDesc = ToD3D12UnorderedAccessViewDesc(*this);

            m_Device.GetD3D12Device()->CreateUnorderedAccessView(
                m_D3D12Resource,
                nullptr,
                &d3d12UavDesc,
                D3D12_CPU_DESCRIPTOR_HANDLE{ cpuHandle });
        });
    }

    void Buffer::MapReadbackData(uint64_t offsetInBytes, uint64_t dataSizeInBytes, const MapReadbackCallback& callback) const
    {
        BenzinAssert(m_HeapType == GpuHeapType::Readback);
        BenzinAssert(offsetInBytes + dataSizeInBytes <= GetSizeInBytes());

        D3D12_RANGE d3d12ReadbackRange = {};
        d3d12ReadbackRange.Begin = offsetInBytes;
        d3d12ReadbackRange.End = offsetInBytes + dataSizeInBytes;

        std::byte* mappedData = nullptr;
        BenzinD3D12Call(m_D3D12Resource->Map(0, &d3d12ReadbackRange, reinterpret_cast<void**>(&mappedData)));

        callback(mappedData);

        m_D3D12Resource->Unmap(0, nullptr);
    }

    void Buffer::SetupCreation(const BufferCreation& creation, const GpuHeap* gpuHeap)
    {
        BenzinAssert(m_D3D12Resource != nullptr);

        SetD3DObjectDebugName(m_D3D12Resource, creation.DebugName);

        m_HeapType = gpuHeap != nullptr ? gpuHeap->GetType() : creation.HeapType;
        m_Type = creation.Type;
        m_Format = creation.Format;
        m_ElementSizeInBytes = creation.ElementSizeInBytes;
        m_ElementCount = creation.ElementCount;
        m_IsUnorderedAccessAllowed = creation.IsUnorderedAccessAllowed;

        if (m_HeapType == GpuHeapType::Upload || m_HeapType == GpuHeapType::GpuUpload)
        {
            const D3D12_RANGE d3d12Range{ .Begin = 0, .End = 0 }; // Writing only range
            BenzinD3D12Call(m_D3D12Resource->Map(0, &d3d12Range, reinterpret_cast<void**>(&m_CpuMappedData)));
        }
    }

}

BenzinDefineStdHashForType(benzin::BufferSrv, bufferSrv,
{
    return typeid(benzin::BufferSrv).hash_code();
});

BenzinDefineStdHashForType(benzin::BufferUav, bufferUav,
{
    return typeid(benzin::BufferUav).hash_code();
});
