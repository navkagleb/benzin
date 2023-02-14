#pragma once

#include "benzin/graphics/format.hpp"

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
    struct DepthState;
    struct StencilState;

    class Resource;
    class BufferResource;
    class TextureResource;

#define BENZIN_D3D12_ASSERT(d3d12Call) BENZIN_ASSERT(SUCCEEDED(d3d12Call))

#define BENZIN_DEBUG_NAME_D3D12_OBJECT(d3d12Object, className)                                              \
	void SetDebugName(std::string_view name, bool isCreated = false)                                        \
    {                                                                                                       \
        const std::string formatedName = fmt::format("{}[{}]", className, name);                            \
        detail::SetD3D12ObjectDebutName(d3d12Object, formatedName, isCreated);                              \
    }                                                                                                       \
                                                                                                            \
	std::string GetDebugName() const                                                                        \
    {                                                                                                       \
        return detail::GetD3D12ObjectDebugName(d3d12Object);                                                \
    }

    namespace detail
    {

        template <typename T>
        concept IsD3D12Nameable = std::is_base_of_v<ID3D12Object, T> || std::is_base_of_v<IDXGIObject, T>;

        template <typename T>
        concept IsD3D12Releasable = requires (T t)
        {
            { t.Release() };
        };

        template <IsD3D12Releasable T>
        inline std::string_view GetD3D12ClassName()
        {
            const std::string_view d3d12ClassName = typeid(T).name();

            size_t beginNameIndex = d3d12ClassName.find("ID3D12");

            if (beginNameIndex == std::string_view::npos)
            {
                beginNameIndex = d3d12ClassName.find("IDXGI");
            }

            BENZIN_ASSERT(beginNameIndex != std::string_view::npos);

            return d3d12ClassName.substr(beginNameIndex);
        }

        template <IsD3D12Nameable T>
        inline void SetD3D12ObjectDebutName(T* d3d12Object, std::string_view name, bool isCreated)
        {
            if (name.empty())
            {
                return;
            }

            BENZIN_ASSERT(d3d12Object);

            BENZIN_D3D12_ASSERT(d3d12Object->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data()));

            if (isCreated)
            {
                BENZIN_INFO("D3D12 Object is created: {}", name.data());
            }
        }

        template <IsD3D12Nameable T>
        inline std::string GetD3D12ObjectDebugName(T* d3d12Object)
        {
            BENZIN_ASSERT(d3d12Object);

            UINT bufferSize = 64;
            std::string name;
            name.resize(bufferSize);

            if (SUCCEEDED(d3d12Object->GetPrivateData(WKPDID_D3DDebugObjectName, &bufferSize, name.data())))
            {
                name.resize(bufferSize);
                return name;
            }
            
            name.clear();

            return name;
        }

    } // namespace detail

    template <detail::IsD3D12Releasable T>
    inline void SafeReleaseD3D12Object(T*& d3d12Object)
    {
        if (d3d12Object)
        {
            if constexpr (detail::IsD3D12Nameable<T>)
            {
                std::string debugName = detail::GetD3D12ObjectDebugName(d3d12Object);

                if (debugName.empty())
                {
                    debugName = detail::GetD3D12ClassName<T>();
                }

                BENZIN_INFO("D3D12 Object is released: {}", debugName);
            }

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
