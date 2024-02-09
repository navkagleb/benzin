#include "bootstrap.hpp"
#include "scene_layer.hpp"

#include <benzin/core/math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/mapped_data.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/texture_loader.hpp>
#include <benzin/utility/random.hpp>

namespace sandbox
{

    namespace
    {

        enum GPUTimerIndex : uint32_t
        {
            GPUTimerIndex_GeometryPass,
            GPUTimerIndex_DeferredLightingPass,
            GPUTimerIndex_EnvironmentPass,
            GPUTimerIndex_BackBufferCopy,
            GPUTimerIndex_Total,
        };

        benzin::MeshCollectionResource CreatePointLightMeshCollection(uint32_t spheresPerRowCount, uint32_t spheresPerColumnCount)
        {
            const uint32_t totalCount = spheresPerRowCount * spheresPerColumnCount;

            benzin::MeshCollectionResource resultMeshCollection;
            resultMeshCollection.DebugName = "PointLights";
            resultMeshCollection.Meshes.push_back(benzin::GetDefaultGeoSphereMesh());
            resultMeshCollection.Materials.reserve(totalCount);
            resultMeshCollection.MeshInstances.reserve(totalCount);

            for (const auto _ : std::views::iota(0u, spheresPerRowCount * spheresPerColumnCount))
            {
                const DirectX::XMFLOAT3 emissiveFactor
                {
                    benzin::Random::Get<float>(0.0f, 1.0f),
                    benzin::Random::Get<float>(0.0f, 1.0f),
                    benzin::Random::Get<float>(0.0f, 1.0f)
                };

                resultMeshCollection.Materials.push_back(benzin::Material
                {
                    .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                    .EmissiveFactor = emissiveFactor,
                });

                resultMeshCollection.MeshInstances.push_back(benzin::MeshInstance
                {
                    .MeshIndex = 0, // Only one mesh in MeshCollectionResource
                    .MaterialIndex = (uint32_t)resultMeshCollection.Materials.size() - 1,
                });
            }

            return resultMeshCollection;
        }

        bool IsMeshCulled(const benzin::Camera& camera, const benzin::MeshCollection& meshCollection, uint32_t meshInstanceIndex, const DirectX::XMMATRIX& worldMatrix)
        {
            const auto& meshInstance = meshCollection.MeshInstances[meshInstanceIndex];
            const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

            if (!mesh.BoundingBox)
            {
                return false;
            }

            const auto GetNodeTransform = [&]
            {
                if (meshInstance.MeshNodeIndex == benzin::g_InvalidIndex<uint32_t>)
                {
                    return DirectX::XMMatrixIdentity();
                }

                return meshCollection.MeshNodes[meshInstance.MeshNodeIndex].Transform;
            };

            const auto localToViewSpaceTransformMatrix = GetNodeTransform() * worldMatrix * camera.GetViewMatrix();
            const auto viewSpaceMeshBoundingBox = benzin::TransformBoundingBox(*mesh.BoundingBox, localToViewSpaceTransformMatrix);

            return camera.GetProjection().GetBoundingFrustum().Contains(viewSpaceMeshBoundingBox) == DirectX::DISJOINT;
        }

        constexpr auto g_GBuffer_Color0Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_Color1Format = benzin::GraphicsFormat::RGBA16Float;
        constexpr auto g_GBuffer_Color2Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_Color3Format = benzin::GraphicsFormat::RGBA8Unorm;
        constexpr auto g_GBuffer_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

    } // anonymous namespace

    // GeometryPass

    GeometryPass::GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "GeometryPass",
            .VertexShader{ "geometry_pass.hlsl", "VS_Main" },
            .PixelShader{ "geometry_pass.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .RasterizerState
            {
                .TriangleOrder = benzin::TriangleOrder::CounterClockwise,
            },
            .RenderTargetFormats
            {
                g_GBuffer_Color0Format,
                g_GBuffer_Color1Format,
                g_GBuffer_Color2Format,
                g_GBuffer_Color3Format,
            },
            .DepthStencilFormat = g_GBuffer_DepthStencilFormat,
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void GeometryPass::OnRender(const benzin::Scene& scene)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Albedo, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Emissive, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.RoughnessMetalness, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::DepthWrite });

        commandList.SetRenderTargets(
            {
                m_GBuffer.Albedo->GetRenderTargetView(),
                m_GBuffer.WorldNormal->GetRenderTargetView(),
                m_GBuffer.Emissive->GetRenderTargetView(),
                m_GBuffer.RoughnessMetalness->GetRenderTargetView(),
            },
            &m_GBuffer.DepthStencil->GetDepthStencilView()
        );

        commandList.ClearRenderTarget(m_GBuffer.Albedo->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.WorldNormal->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.Emissive->GetRenderTargetView());
        commandList.ClearRenderTarget(m_GBuffer.RoughnessMetalness->GetRenderTargetView());
        commandList.ClearDepthStencil(m_GBuffer.DepthStencil->GetDepthStencilView());

        commandList.SetPipelineState(*m_PipelineState);

        const auto view = scene.GetEntityRegistry().view<const benzin::TransformComponent, const benzin::MeshInstanceComponent>();
        for (const auto entityHandle : view)
        {
            const auto& tc = view.get<const benzin::TransformComponent>(entityHandle);
            const auto& mic = view.get<const benzin::MeshInstanceComponent>(entityHandle);

            const auto& meshCollection = scene.GetMeshCollection(mic.MeshCollectionIndex);
            const auto& meshCollectionGPUStorage = scene.GetMeshCollectionGPUStorage(mic.MeshCollectionIndex);

            commandList.SetRootResource(joint::GeometryPassRC_MeshVertexBuffer, meshCollectionGPUStorage.VertexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MeshIndexBuffer, meshCollectionGPUStorage.IndexBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MeshInfoBuffer, meshCollectionGPUStorage.MeshInfoBuffer->GetShaderResourceView());
            commandList.SetRootConstant(joint::GeometryPassRC_MeshNodeBuffer, meshCollectionGPUStorage.MeshNodeBuffer ? meshCollectionGPUStorage.MeshNodeBuffer->GetShaderResourceView().GetHeapIndex() : benzin::g_InvalidIndex<uint32_t>);
            commandList.SetRootResource(joint::GeometryPassRC_MeshInstanceBuffer, meshCollectionGPUStorage.MeshInstanceBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_MaterialBuffer, meshCollectionGPUStorage.MaterialBuffer->GetShaderResourceView());
            commandList.SetRootResource(joint::GeometryPassRC_TransformBuffer, tc.Buffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()));

            const auto meshInstanceRange = mic.MeshInstanceRange.value_or(std::pair{ 0u, (uint32_t)meshCollection.MeshInstances.size() });
            for (uint32_t i = meshInstanceRange.first; i < meshInstanceRange.second; ++i)
            {
                if (IsMeshCulled(scene.GetCamera(), meshCollection, i, tc.GetMatrix()))
                {
                    continue;
                }

                commandList.SetRootConstant(joint::GeometryPassRC_MeshInstanceIndex, i);

                const auto& meshInstance = meshCollection.MeshInstances[i];
                const auto& mesh = meshCollection.Meshes[meshInstance.MeshIndex];

                commandList.SetPrimitiveTopology(mesh.PrimitiveTopology);
                commandList.DrawVertexed((uint32_t)mesh.Indices.size());
            }
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Albedo, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.WorldNormal, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.Emissive, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.RoughnessMetalness, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_GBuffer.DepthStencil, benzin::ResourceState::Present });
    }

    void GeometryPass::OnResize(uint32_t width, uint32_t height)
    {
        static const auto CreateGBufferRenderTarget = [](
            benzin::Device& device,
            benzin::GraphicsFormat format,
            uint32_t width,
            uint32_t height,
            std::string_view debugName,
            std::shared_ptr<benzin::Texture>& outRenderTarget
        )
        {
            outRenderTarget = std::make_shared<benzin::Texture>(device, benzin::TextureCreation
            {
                .DebugName = debugName,
                .Type = benzin::TextureType::Texture2D,
                .Format = format,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowRenderTarget,
                .ClearValue = benzin::g_DefaultClearColor,
                .IsNeedShaderResourceView = true,
                .IsNeedRenderTargetView = true,
            });
        };

        CreateGBufferRenderTarget(m_Device, g_GBuffer_Color0Format, width, height, "GBuffer_Albedo", m_GBuffer.Albedo);
        CreateGBufferRenderTarget(m_Device, g_GBuffer_Color1Format, width, height, "GBuffer_WorldNormal", m_GBuffer.WorldNormal);
        CreateGBufferRenderTarget(m_Device, g_GBuffer_Color2Format, width, height, "GBuffer_Emissive", m_GBuffer.Emissive);
        CreateGBufferRenderTarget(m_Device, g_GBuffer_Color3Format, width, height, "GBuffer_RoughnessMetalness", m_GBuffer.RoughnessMetalness);

        {
            m_GBuffer.DepthStencil = std::make_shared<benzin::Texture>(m_Device, benzin::TextureCreation
            {
                .DebugName = "GBuffer_DepthStencil",
                .Type = benzin::TextureType::Texture2D,
                .Format = g_GBuffer_DepthStencilFormat,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowDepthStencil,
                .ClearValue = benzin::g_DefaultClearDepthStencil,
                .IsNeedDepthStencilView = true,
            });

            m_GBuffer.DepthStencil->PushShaderResourceView({ .Format{ benzin::GraphicsFormat::D24Unorm_X8Typeless } });
        }
    }

    // DeferredLightingPass

    DeferredLightingPass::DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        m_PassConstantBuffer = benzin::CreateFrameDependentConstantBuffer<joint::DeferredLightingPassConstants>(m_Device, "DeferredLightingPassConstantBuffer");

        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "DeferredLightingPass",
            .VertexShader{ "fullscreen_triangle.hlsl", "VS_Main" },
            .PixelShader{ "deferred_lighting_pass.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .DepthState
            {
                .IsEnabled = false,
                .IsWriteEnabled = false,
            },
            .RenderTargetFormats{ benzin::GraphicsFormat::RGBA8Unorm },
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void DeferredLightingPass::OnUpdate(const benzin::Scene& scene)
    {
        const joint::DeferredLightingPassConstants constants
        {
            .SunColor = m_SunColor,
            .SunIntensity = m_SunIntensity,
            .SunDirection = m_SunDirection,
            .ActivePointLightCount = scene.GetStats().PointLightCount,
            .OutputType = magic_enum::enum_integer(m_OutputType),
        };

        benzin::MappedData passConstantBuffer{ *m_PassConstantBuffer };
        passConstantBuffer.Write(constants, m_Device.GetActiveFrameIndex());
    }

    void DeferredLightingPass::OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::RenderTarget });

        commandList.SetRenderTargets({ m_OutputTexture->GetRenderTargetView() });
        commandList.ClearRenderTarget(m_OutputTexture->GetRenderTargetView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootResource(joint::DeferredLightingPassRC_PassConstantBuffer, m_PassConstantBuffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()));
            commandList.SetRootResource(joint::DeferredLightingPassRC_AlbedoTexture, gbuffer.Albedo->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_WorldNormalTexture, gbuffer.WorldNormal->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_EmissiveTexture, gbuffer.Emissive->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_RoughnessMetalnessTexture, gbuffer.RoughnessMetalness->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_DepthStencilTexture, gbuffer.DepthStencil->GetShaderResourceView());
            commandList.SetRootResource(joint::DeferredLightingPassRC_PointLightBuffer, scene.GetPointLightBuffer()->GetShaderResourceView(m_Device.GetActiveFrameIndex()));

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ *m_OutputTexture, benzin::ResourceState::Present });
    }

    void DeferredLightingPass::OnImGuiRender()
    {
        ImGui::Begin("DeferredLightingPass");
        {
            ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&m_SunColor));
            ImGui::DragFloat("SunIntensity", &m_SunIntensity, 0.1f, 0.0f, 100.0f);

            if (ImGui::DragFloat3("SunDirection", reinterpret_cast<float*>(&m_SunDirection), 0.01f, -1.0f, 1.0f))
            {
                DirectX::XMStoreFloat3(&m_SunDirection, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_SunDirection)));
            }

            ImGui::Separator();

            static constexpr auto names = magic_enum::enum_names<joint::DeferredLightingOutputType>();

            if (ImGui::BeginListBox("##listbox", ImVec2{ -FLT_MIN, 0.0f }))
            {
                for (const auto i : std::views::iota(0u, joint::DeferredLightingOutputType_Count))
                {
                    const bool isSelected = m_OutputType == i;

                    if (ImGui::Selectable(names[i].data(), isSelected))
                    {
                        m_OutputType = (joint::DeferredLightingOutputType)i;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }
        }
        ImGui::End();
    }

    void DeferredLightingPass::OnResize(uint32_t width, uint32_t height)
    {
        m_OutputTexture = std::make_unique<benzin::Texture>(m_Device, benzin::TextureCreation
        {
            .DebugName = "DeferredLightingPass_OutputTexture",
            .Type = benzin::TextureType::Texture2D,
            .Format = benzin::GraphicsSettingsInstance::Get().BackBufferFormat,
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowRenderTarget,
            .ClearValue = benzin::g_DefaultClearColor,
            .IsNeedRenderTargetView = true,
        });
    }

    // EnvironmentPass

    EnvironmentPass::EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "EnvironmentPass",
            .VertexShader{ "fullscreen_triangle.hlsl", "VS_MainDepth1" },
            .PixelShader{ "environment_pass.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .DepthState
            {
                .IsWriteEnabled = false,
                .ComparisonFunction = benzin::ComparisonFunction::Equal,
            },
            .RenderTargetFormats{ benzin::GraphicsFormat::RGBA8Unorm },
            .DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint,
        });

        {
            m_CubeTexture = std::unique_ptr<benzin::Texture>{ m_Device.GetTextureLoader().LoadCubeMapFromHDRFile("scythian_tombs_2_4k.hdr", 1024) };
            m_CubeTexture->PushShaderResourceView();
        }
    }

    void EnvironmentPass::OnRender(const benzin::Scene& scene, benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil)
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::RenderTarget });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::DepthWrite });

        commandList.SetRenderTargets({ deferredLightingOutputTexture.GetRenderTargetView() }, &gbufferDepthStecil.GetDepthStencilView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootResource(joint::EnvironmentPassRC_CubeMapTexture, m_CubeTexture->GetShaderResourceView());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::Present });
        commandList.SetResourceBarrier(benzin::TransitionBarrier{ gbufferDepthStecil, benzin::ResourceState::Present });
    }

    // SceneLayer

    SceneLayer::SceneLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
        , m_GeometryPass{ m_Device, m_SwapChain }
        , m_DeferredLightingPass{ m_Device, m_SwapChain }
        , m_EnvironmentPass{ m_Device, m_SwapChain }
    {
        m_GPUTimer = std::make_unique<benzin::GPUTimer>(m_Device, m_Device.GetGraphicsCommandQueue(), magic_enum::enum_count<GPUTimerIndex>());
        m_CameraBuffer = benzin::CreateFrameDependentConstantBuffer<joint::CameraConstants>(m_Device, "CameraConstantBuffer");

        auto& camera = m_Scene.GetCamera();
        camera.SetPosition({ -3.0f, 2.0f, -0.25f });
        camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });

        if (auto* perspectiveProjection = dynamic_cast<benzin::PerspectiveProjection*>(&camera.GetProjection()))
        {
            perspectiveProjection->SetLens(DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f);
        }

        CreateEntities();
    }

    SceneLayer::~SceneLayer() = default;

    void SceneLayer::OnEndFrame()
    {
        m_GPUTimer->OnEndFrame(m_Device.GetGraphicsCommandQueue().GetCommandList());
    }

    void SceneLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<benzin::WindowResizedEvent>([&](auto& event)
        {
            m_GeometryPass.OnResize(event.GetWidth(), event.GetHeight());
            m_DeferredLightingPass.OnResize(event.GetWidth(), event.GetHeight());
            
            return false;
        });
    }

    void SceneLayer::OnUpdate()
    {
        const benzin::MilliSeconds dt = s_FrameTimer.GetDeltaTime();

        m_FlyCameraController.OnUpdate(dt);
        UpdateEntities(dt);

        m_Scene.OnUpdate();

        {
            const auto& camera = m_Scene.GetCamera();

            const joint::CameraConstants constants
            {
                .View = camera.GetViewMatrix(),
                .InverseView = camera.GetInverseViewMatrix(),
                .Projection = camera.GetProjectionMatrix(),
                .InverseProjection = camera.GetInverseProjectionMatrix(),
                .ViewProjection = camera.GetViewProjectionMatrix(),
                .InverseViewProjection = camera.GetInverseViewProjectionMatrix(),
                .InverseViewDirectionProjection = camera.GetInverseViewDirectionProjectionMatrix(),
                .WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),
            };

            benzin::MappedData cameraBuffer{ *m_CameraBuffer };
            cameraBuffer.Write(constants, m_Device.GetActiveFrameIndex());
        }

        m_DeferredLightingPass.OnUpdate(m_Scene);
    }

    void SceneLayer::OnRender()
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinIndexedGPUTimerScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex_Total);

        commandList.SetRootConstant(joint::GlobalRC_CameraConstantBuffer, m_CameraBuffer->GetConstantBufferView(m_Device.GetActiveFrameIndex()).GetHeapIndex());

        {
            BenzinIndexedGPUTimerScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex_GeometryPass);
            m_GeometryPass.OnRender(m_Scene);
        }

        {
            BenzinIndexedGPUTimerScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex_DeferredLightingPass);
            m_DeferredLightingPass.OnRender(m_Scene, m_GeometryPass.GetGBuffer());
        }

        if (m_DeferredLightingPass.GetOutputType() == joint::DeferredLightingOutputType_Final)
        {
            BenzinIndexedGPUTimerScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex_EnvironmentPass);
            m_EnvironmentPass.OnRender(m_Scene, m_DeferredLightingPass.GetOutputTexture(), *m_GeometryPass.GetGBuffer().DepthStencil);
        }

        {
            BenzinIndexedGPUTimerScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex_BackBufferCopy);

            auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();
            auto& deferredLightingOutputTexture = m_DeferredLightingPass.GetOutputTexture();

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::CopyDestination });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::CopySource });

            commandList.CopyResource(currentBackBuffer, m_DeferredLightingPass.GetOutputTexture());

            commandList.SetResourceBarrier(benzin::TransitionBarrier{ currentBackBuffer, benzin::ResourceState::Present });
            commandList.SetResourceBarrier(benzin::TransitionBarrier{ deferredLightingOutputTexture, benzin::ResourceState::Present });
        }
    }

    void SceneLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();
        m_DeferredLightingPass.OnImGuiRender();

        ImGui::Begin("Scene Stats");
        {
            struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
            {
                char do_thousands_sep() const override { return '\''; }
            
                std::string do_grouping() const override { return "\3"; }
            };

            static const std::locale customLocale{ std::locale::classic(), new ThoudandSeperatorApostrophe3 };

            std::locale::global(customLocale);
            BenzinExecuteOnScopeExit([]{ std::locale::global(std::locale::classic()); });

            const auto& sceneStats = m_Scene.GetStats();
            ImGui::Text(std::format("VertexCount: {:L}", sceneStats.VertexCount).c_str());
            ImGui::Text(std::format("TriangleCount: {:L}", sceneStats.TriangleCount).c_str());
            ImGui::Text(std::format("PointLightCount: {:L}", sceneStats.PointLightCount).c_str());
        }
        ImGui::End();

        ImGui::Begin("MainLayer TimeStats");
        {
            static constexpr size_t gpuTimingCount = magic_enum::enum_count<GPUTimerIndex>();
            static constexpr size_t enumNameOffset = "GPUTimerIndex"sv.size() + 1;

            static std::array<float, gpuTimingCount> gpuTimings;

            if (s_FrameStats.IsReady())
            {
                for (const auto i : std::views::iota(0u, gpuTimingCount))
                {
                    gpuTimings[i] = m_GPUTimer->GetElapsedTime(i).count();
                }
            }

            for (const auto i : std::views::iota(0u, gpuTimingCount))
            {
                const auto gpuTimingName = magic_enum::enum_name((GPUTimerIndex)i).substr(enumNameOffset);
                ImGui::Text(std::format("GPU {} Time: {:.4f} ms", gpuTimingName, gpuTimings[i]).c_str());
            }
        }
        ImGui::End();
    }

    void SceneLayer::CreateEntities()
    {
        auto& entityRegistry = m_Scene.GetEntityRegistry();

        benzin::MeshCollectionResource sponzaMeshCollection;
        const auto sponzaFuture = std::async(std::launch::async, [&]
        {
            const std::string_view fileName = "Sponza/glTF/Sponza.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, sponzaMeshCollection));
        });

        benzin::MeshCollectionResource boomBooxMeshCollection;
        {
            const std::string_view fileName = "BoomBox/glTF/BoomBox.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, boomBooxMeshCollection));
        }

        benzin::MeshCollectionResource damagedHelmetMeshCollection;
        {
            const std::string_view fileName = "DamagedHelmet/glTF/DamagedHelmet.gltf";

            BenzinLogTimeOnScopeExit(std::format("Loading MeshCollection from {}", fileName));
            BenzinAssert(LoadMeshCollectionFromGLTFFile(fileName, damagedHelmetMeshCollection));
        }

        const size_t spheresPerRowCount = 20;
        const size_t spheresPerColumnCount = 6;
        benzin::MeshCollectionResource pointLightMeshCollection = CreatePointLightMeshCollection(spheresPerRowCount, spheresPerColumnCount);

        sponzaFuture.wait();

        const uint32_t sponzaMeshCollectionIndex = m_Scene.PushMeshCollection(std::move(sponzaMeshCollection));
        const uint32_t boomBooxMeshCollectionIndex = m_Scene.PushMeshCollection(std::move(boomBooxMeshCollection));
        const uint32_t damagedHelmetMeshCollectionIndex = m_Scene.PushMeshCollection(std::move(damagedHelmetMeshCollection));
        const uint32_t pointLightsMeshCollectionIndex = m_Scene.PushMeshCollection(std::move(pointLightMeshCollection));

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshCollectionIndex = sponzaMeshCollectionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.Rotation = { 0.0f, DirectX::XM_PI, 0.0f };
        }

        {
            m_BoomBoxEntity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(m_BoomBoxEntity);
            mic.MeshCollectionIndex = boomBooxMeshCollectionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(m_BoomBoxEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 30.0f, 30.0f, 30.0f };
            tc.Translation = { 0.0f, 0.6f, 0.0f };
        }

        {
            m_DamagedHelmetEntity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(m_DamagedHelmetEntity);
            mic.MeshCollectionIndex = damagedHelmetMeshCollectionIndex;

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(m_DamagedHelmetEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 0.4f, 0.4f, 0.4f };
            tc.Translation = { 1.0f, 0.5f, -0.5f };
        }

        {
            for (const auto i : std::views::iota(0u, spheresPerRowCount))
            {
                for (const auto j : std::views::iota(0u, spheresPerColumnCount))
                {
                    const uint32_t currentPointLightIndex = i * spheresPerColumnCount + j;

                    const auto entity = entityRegistry.create();

                    auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
                    mic.MeshCollectionIndex = pointLightsMeshCollectionIndex;
                    mic.MeshInstanceRange = { currentPointLightIndex, currentPointLightIndex + 1 };

                    auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
                    tc.Scale = { 0.02f, 0.02f, 0.02f };
                    tc.Translation = DirectX::XMFLOAT3{ ((float)spheresPerRowCount / -2.0f + (float)i) * 0.5f, 0.2f, ((float)spheresPerColumnCount / -2.0f + (float)j) * 0.5f };

                    auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
                    plc.Color = m_Scene.GetMeshCollection(pointLightsMeshCollectionIndex).Materials[currentPointLightIndex].EmissiveFactor;
                    plc.Intensity = 3.0f;
                    plc.Range = 4.0f;
                }
            }
        }

        {
            BenzinLogTimeOnScopeExit("Upload scene data to GPU");
            m_Scene.UploadMeshCollections();
        }
    }

    void SceneLayer::UpdateEntities(benzin::MilliSeconds dt)
    {
        auto& entityRegistry = m_Scene.GetEntityRegistry();

        if (m_BoomBoxEntity != benzin::g_InvalidEntity)
        {
            auto& tc = entityRegistry.get<benzin::TransformComponent>(m_BoomBoxEntity);
            tc.Rotation.x += 0.0001f * dt.count();
            tc.Rotation.z += 0.0002f * dt.count();
        }

        if (m_DamagedHelmetEntity != benzin::g_InvalidEntity)
        {
            auto& tc = entityRegistry.get<benzin::TransformComponent>(m_DamagedHelmetEntity);
            tc.Rotation.x += 0.0001f * dt.count();
            tc.Rotation.y -= 0.00015f * dt.count();
        }
    }

} // namespace sandbox
