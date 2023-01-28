#pragma once

namespace benzin
{

    class Device;
    class SwapChain;
    class CommandQueue;
    class GraphicsCommandList;
    class Descriptor;
    class DescriptorHeap;
    class DescriptorManager;
    class ResourceViewBuilder;
    class ResourceLoader;
    class PipelineState;
    class RootSignature;
    class Shader;
    struct BlendState;
    struct RasterizerState;
    struct DepthStencilState;

    class Resource;
    class BufferResource;
    class TextureResource;

#define BENZIN_D3D12_ASSERT(d3d12Call) BENZIN_ASSERT(SUCCEEDED(d3d12Call))

#define BENZIN_DEBUG_NAME_D3D12_OBJECT(d3d12Object, prefix)                                     \
	void SetDebugName(const std::string& name)                                                  \
    {                                                                                           \
        static std::string debugNamePrefix = prefix;                                            \
                                                                                                \
        detail::SetD3D12ObjectDebugName(d3d12Object, debugNamePrefix + "{" + name + "}");       \
    }                                                                                           \
                                                                                                \
	std::string GetDebugName() const                                                            \
    {                                                                                           \
        return detail::GetD3D12ObjectDebugName(d3d12Object);                                    \
    }

    namespace detail
    {

        template <typename T>
        concept D3D12ObjectConcept = std::is_base_of_v<ID3D12Object, T> || std::is_base_of_v<IUnknown, T>;

        template <D3D12ObjectConcept T>
        void SetD3D12ObjectDebugName(T* d3d12Object, std::string_view name)
        {
            BENZIN_ASSERT(d3d12Object);

            BENZIN_D3D12_ASSERT(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data()));
        }

        template <D3D12ObjectConcept T>
        std::string GetD3D12ObjectDebugName(T* d3d12Object)
        {
            BENZIN_ASSERT(d3d12Object);

            UINT bufferSize = 64;
            std::string name;
            name.resize(bufferSize);

            BENZIN_D3D12_ASSERT(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, name.data()));
            name.resize(bufferSize);

            return name;
        }

    } // namespace detail

    template <detail::D3D12ObjectConcept T>
    inline void SafeReleaseD3D12Object(T*& d3d12Object)
    {
        if (d3d12Object)
        {
            d3d12Object->Release();
            d3d12Object = nullptr;
        }
    }

    enum class ShaderVisibility : uint8_t
    {
        All = D3D12_SHADER_VISIBILITY_ALL,
        Vertex = D3D12_SHADER_VISIBILITY_VERTEX,
        Hull = D3D12_SHADER_VISIBILITY_HULL,
        Domain = D3D12_SHADER_VISIBILITY_DOMAIN,
        Geometry = D3D12_SHADER_VISIBILITY_GEOMETRY,
        Pixel = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    enum class PrimitiveTopologyType : uint8_t
    {
        Unknown = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,

        Point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        Line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        Patch = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
    };

    enum class PrimitiveTopology : uint8_t
    {
        Unknown = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,

        PointList = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        LineList = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        LineStrip = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        TriangleList = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        TriangleStrip = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,

        ControlPointPatchlist4 = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
        ControlPointPatchlist16 = D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST,
    };

    enum class GraphicsFormat : uint8_t
    {
        Unknown = DXGI_FORMAT_UNKNOWN,

        R32G32B32A32Typeless = DXGI_FORMAT_R32G32B32A32_TYPELESS,
        R32G32B32A32Float = DXGI_FORMAT_R32G32B32A32_FLOAT,
        R32G32B32A32UnsignedInt = DXGI_FORMAT_R32G32B32A32_UINT,
        R32G32B32A32SignedInt = DXGI_FORMAT_R32G32B32A32_SINT,

        R32G32B32Typeless = DXGI_FORMAT_R32G32B32_TYPELESS,
        R32G32B32Float = DXGI_FORMAT_R32G32B32_FLOAT,
        R32G32B32UnsignedInt = DXGI_FORMAT_R32G32B32_UINT,
        R32G32B32SignedInt = DXGI_FORMAT_R32G32B32_SINT,

        R16G16B16A16Typeless = DXGI_FORMAT_R16G16B16A16_TYPELESS,
        R16G16B16A16Float = DXGI_FORMAT_R16G16B16A16_FLOAT,
        R16G16B16A16UnsignedNorm = DXGI_FORMAT_R16G16B16A16_UNORM,
        R16G16B16A16UnsignedInt = DXGI_FORMAT_R16G16B16A16_UINT,
        R16G16B16A16SignedNorm = DXGI_FORMAT_R16G16B16A16_SNORM,
        R16G16B16A16SignedInt = DXGI_FORMAT_R16G16B16A16_SINT,

        R32G32Typeless = DXGI_FORMAT_R32G32_TYPELESS,
        R32G32Float = DXGI_FORMAT_R32G32_FLOAT,
        R32G32UnsignedInt = DXGI_FORMAT_R32G32_UINT,
        R32G32SignedInt = DXGI_FORMAT_R32G32_SINT,

        R8G8B8A8Typeless = DXGI_FORMAT_R8G8B8A8_TYPELESS,
        R8G8B8A8UnsignedNorm = DXGI_FORMAT_R8G8B8A8_UNORM,
        R8G8B8A8UnsignedInt = DXGI_FORMAT_R8G8B8A8_UINT,
        R8G8B8A8SignedNorm = DXGI_FORMAT_R8G8B8A8_SNORM,
        R8G8B8A8SignedInt = DXGI_FORMAT_R8G8B8A8_SINT,

        R16G16Typeless = DXGI_FORMAT_R16G16_TYPELESS,
        R16G16Float = DXGI_FORMAT_R16G16_FLOAT,
        R16G16UnsignedNorm = DXGI_FORMAT_R16G16_UNORM,
        R16G16UnsignedInt = DXGI_FORMAT_R16G16_UINT,
        R16G16SignedNorm = DXGI_FORMAT_R16G16_SNORM,
        R16G16SignedInt = DXGI_FORMAT_R16G16_SINT,

        R32Typeless = DXGI_FORMAT_R32_TYPELESS,
        D32Float = DXGI_FORMAT_D32_FLOAT,
        R32Float = DXGI_FORMAT_R32_FLOAT,
        R32UnsignedInt = DXGI_FORMAT_R32_UINT,
        R32SignedInt = DXGI_FORMAT_R32_SINT,

        D24UnsignedNormS8UnsignedInt = DXGI_FORMAT_D24_UNORM_S8_UINT,
        R24UnsignedNormX8Typeless = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,

        R16Typeless = DXGI_FORMAT_R16_TYPELESS,
        R16Float = DXGI_FORMAT_R16_FLOAT,
        D16UnsignedNorm = DXGI_FORMAT_D16_UNORM,
        R16UnsignedNorm = DXGI_FORMAT_R16_UNORM,
        R16UnsignedInt = DXGI_FORMAT_R16_UINT,
        R16SignedNorm = DXGI_FORMAT_R16_SNORM,
        R16SignedInt = DXGI_FORMAT_R16_SINT,

        BC1UnsignedNormalized = DXGI_FORMAT_BC1_UNORM,
        BC3UnsignedNormalized = DXGI_FORMAT_BC3_UNORM,
        BC7UnsignedNormalized = DXGI_FORMAT_BC7_UNORM,

        R8G8R8A8UnsignedNormalized = DXGI_FORMAT_B8G8R8A8_UNORM
    };

    enum class ComparisonFunction : uint8_t
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greate = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreateEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS
    };

} // namespace benzin
