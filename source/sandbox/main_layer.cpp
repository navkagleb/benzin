#include "bootstrap.hpp"
#include "main_layer.hpp"

#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/mesh_collection.hpp>
#include <benzin/engine/model.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture_loader.hpp>
#include <benzin/utility/random.hpp>

#include "ibl_texture_generator.hpp"

namespace sandbox
{

    namespace
    {

        struct GBufferPassData
        {
            DirectX::XMMATRIX ViewMatrix;
            DirectX::XMMATRIX ProjectionMatrix;
            DirectX::XMMATRIX ViewProjectionMatrix;
            DirectX::XMFLOAT3 WorldCameraPosition;
        };

        struct EnvironmentPassData
        {
            DirectX::XMMATRIX InverseViewProjectionMatrix;
        };

        struct DeferredLightingPassData
        {
            DirectX::XMMATRIX InverseViewMatrix;
            DirectX::XMMATRIX InverseProjectionMatrix;
            DirectX::XMFLOAT3 WorldCameraPosition;
            uint32_t PointLightCount;
            DirectX::XMFLOAT3 SunColor;
            float SunIntensity;
            DirectX::XMFLOAT3 SunDirection;
        };

        struct PointLight
        {
            DirectX::XMFLOAT3 Color;
            float Intensity;
            DirectX::XMFLOAT3 WorldPosition;
            float ConstantAttenuation;
            float LinearAttenuation;
            float ExponentialAttenuation;
        };

        struct EntityData
        {
            DirectX::XMMATRIX WorldMatrix;
            DirectX::XMMATRIX InverseWorldMatrix;
        };

        enum class GPUTimerIndex
        {
            _Total,
            _GeometryPass,
            _DeferredLightingPass,
            _EnvironmentPass,
            _BackBufferCopy,
        };

    } // anonymous namespace

    // GeometryPass
    GeometryPass::GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
        , m_GPUTimer{ gpuTimer }
    {
        m_FrameResources.PassBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "GeometryPass_PassBuffer",
            .ElementSize = sizeof(GBufferPassData),
            .ElementCount = benzin::config::g_BackBufferCount,
            .Flags = benzin::BufferFlag::ConstantBuffer,
        });

        for (uint32_t i = 0; i < benzin::config::g_BackBufferCount; ++i)
        {
            BenzinAssert(m_FrameResources.PassBuffer->PushConstantBufferView({ .ElementIndex = i }) == i);
        }

        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "GeometryPass",
            .VertexShader{ "gbuffer.hlsl", "VS_Main" },
            .PixelShader{ "gbuffer.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .RasterizerState
            {
                .TriangleOrder = benzin::TriangleOrder::CounterClockwise,
            },
            .RenderTargetFormats
            {
                ms_Color0Format,
                ms_Color1Format,
                ms_Color2Format,
                ms_Color3Format,
            },
            .DepthStencilFormat = ms_DepthStencilFormat,
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void GeometryPass::OnUpdate(const benzin::Camera& camera)
    {
        const GBufferPassData passData
        {
            .ViewMatrix = camera.GetViewMatrix(),
            .ProjectionMatrix = camera.GetProjectionMatrix(),
            .ViewProjectionMatrix = camera.GetViewProjectionMatrix(),
            .WorldCameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),
        };

        benzin::MappedData passBuffer{ *m_FrameResources.PassBuffer };
        passBuffer.Write(passData, m_SwapChain.GetCurrentFrameIndex());
    }

    void GeometryPass::OnExecute(const entt::registry& registry, const benzin::Buffer& entityBuffer)
    {
        enum class RootConstant : uint32_t
        {
            PassBufferIndex,

            EntityBufferIndex,
            EntityIndex,

            VertexBufferIndex,
            IndexBufferIndex,
            SubMeshBufferIndex,

            DrawableMeshBufferIndex,
            DrawableMeshIndex,
            NodeBufferIndex,
            NodeIndex,
            MaterialBufferIndex,
        };

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        const auto DrawMesh = [&](const benzin::Model& model, uint32_t drawableMeshIndex)
        {
            const auto& drawableMesh = model.GetDrawableMeshes()[drawableMeshIndex];
            const auto& meshInfo = model.GetMeshCollection()->GetMeshInfos()[drawableMesh.MeshIndex];

            commandList.SetRootConstant(RootConstant::DrawableMeshIndex, drawableMeshIndex);
            commandList.SetPrimitiveTopology(meshInfo.PrimitiveTopology);
            commandList.DrawVertexed(meshInfo.IndexCount);
        };

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(*m_GBuffer.Albedo, benzin::ResourceState::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.WorldNormal, benzin::ResourceState::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.Emissive, benzin::ResourceState::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.RoughnessMetalness, benzin::ResourceState::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.DepthStencil, benzin::ResourceState::DepthWrite);

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

        {
            BenzinGPUTimerSlotScopeMeasurement(m_GPUTimer, commandList, GPUTimerIndex::_GeometryPass);

            commandList.SetPipelineState(*m_PipelineState);
            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, m_FrameResources.PassBuffer->GetConstantBufferView(m_SwapChain.GetCurrentFrameIndex()));
            commandList.SetRootShaderResource(RootConstant::EntityBufferIndex, entityBuffer.GetShaderResourceView(m_SwapChain.GetCurrentFrameIndex()));

            const auto view = registry.view<benzin::TransformComponent, benzin::ModelComponent>();

            for (const auto& [entity, tc, mc] : view.each())
            {
                commandList.SetRootConstant(RootConstant::EntityIndex, magic_enum::enum_integer(entity));

                const auto& model = mc.Model;
                const auto& meshCollection = model->GetMeshCollection();

                // MeshCollection
                {
                    commandList.SetRootShaderResource(RootConstant::SubMeshBufferIndex, meshCollection->GetMeshInfoBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::VertexBufferIndex, meshCollection->GetVertexBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::IndexBufferIndex, meshCollection->GetIndexBuffer()->GetShaderResourceView());
                }

                // Model
                {
                    commandList.SetRootShaderResource(RootConstant::DrawableMeshBufferIndex, model->GetDrawableMeshBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::NodeBufferIndex, model->GetNodeBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::MaterialBufferIndex, model->GetMaterialBuffer()->GetShaderResourceView());
                }

                // If there is no need to draw whole model
                if (mc.DrawableMeshIndex)
                {
                    DrawMesh(*model, *mc.DrawableMeshIndex);
                    continue;
                }

                for (size_t nodeIndex = 0; nodeIndex < model->GetNodes().size(); ++nodeIndex)
                {
                    const auto& node = model->GetNodes()[nodeIndex];

                    commandList.SetRootConstant(RootConstant::NodeIndex, (uint32_t)nodeIndex);

                    for (uint32_t drawableMeshIndex : node.DrawableMeshIndexRange)
                    {
                        DrawMesh(*model, drawableMeshIndex);
                    }
                }
            }
        }

        commandList.SetResourceBarrier(*m_GBuffer.Albedo, benzin::ResourceState::Present);
        commandList.SetResourceBarrier(*m_GBuffer.WorldNormal, benzin::ResourceState::Present);
        commandList.SetResourceBarrier(*m_GBuffer.Emissive, benzin::ResourceState::Present);
        commandList.SetResourceBarrier(*m_GBuffer.RoughnessMetalness, benzin::ResourceState::Present);
        commandList.SetResourceBarrier(*m_GBuffer.DepthStencil, benzin::ResourceState::Present);
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
                .IsNeedShaderResourceView = true,
                .IsNeedRenderTargetView = true,
            });
        };

        CreateGBufferRenderTarget(m_Device, ms_Color0Format, width, height, "GBuffer_Albedo", m_GBuffer.Albedo);
        CreateGBufferRenderTarget(m_Device, ms_Color1Format, width, height, "GBuffer_WorldNormal", m_GBuffer.WorldNormal);
        CreateGBufferRenderTarget(m_Device, ms_Color2Format, width, height, "GBuffer_Emissive", m_GBuffer.Emissive);
        CreateGBufferRenderTarget(m_Device, ms_Color3Format, width, height, "GBuffer_RoughnessMetalness", m_GBuffer.RoughnessMetalness);

        {
            m_GBuffer.DepthStencil = std::make_shared<benzin::Texture>(m_Device, benzin::TextureCreation
            {
                .DebugName = "GBuffer_DepthStencil",
                .Type = benzin::TextureType::Texture2D,
                .Format = ms_DepthStencilFormat,
                .Width = width,
                .Height = height,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowDepthStencil,
                .IsNeedDepthStencilView = true,
            });

            m_GBuffer.DepthStencil->PushShaderResourceView({ .Format{ benzin::GraphicsFormat::D24Unorm_X8Typeless } });
        }
    }

    // DeferredLightingPass
    DeferredLightingPass::DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
        , m_GPUTimer{ gpuTimer }
    {
        m_FrameResources.PassBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "DeferredLightingPass_PassBuffer",
            .ElementSize = sizeof(DeferredLightingPassData),
            .ElementCount = benzin::config::g_BackBufferCount,
            .Flags = benzin::BufferFlag::ConstantBuffer,
        });

        m_FrameResources.PointLightBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "DeferredLightingPass_PointLightBuffer",
            .ElementSize = sizeof(PointLight),
            .ElementCount = ms_MaxPointLightCount * benzin::config::g_BackBufferCount,
            .Flags = benzin::BufferFlag::UploadBuffer,
        });

        for (uint32_t i = 0; i < benzin::config::g_BackBufferCount; ++i)
        {
            BenzinAssert(m_FrameResources.PassBuffer->PushConstantBufferView({ .ElementIndex = i }) == i);
            BenzinAssert(m_FrameResources.PointLightBuffer->PushStructureBufferView({ .FirstElementIndex = ms_MaxPointLightCount * i, .ElementCount = ms_MaxPointLightCount }) == i);
        }

        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "DeferredLightingPass",
            .VertexShader{ "fullscreen_triangle.hlsl", "VS_Main" },
            .PixelShader{ "deferred_lighting.hlsl", "PS_Main" },
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .DepthState = benzin::DepthState::GetDisabled(),
            .RenderTargetFormats{ benzin::GraphicsFormat::RGBA8Unorm },
        });

        OnResize((uint32_t)m_SwapChain.GetViewport().Width, (uint32_t)m_SwapChain.GetViewport().Height);
    }

    void DeferredLightingPass::OnUpdate(const benzin::Camera& camera, const entt::registry& registry)
    {
        const auto pointLightView = registry.view<const benzin::TransformComponent, const benzin::PointLightComponent>();

        {
            const DeferredLightingPassData passData
            {
                .InverseViewMatrix = camera.GetInverseViewMatrix(),
                .InverseProjectionMatrix = camera.GetInverseProjectionMatrix(),
                .WorldCameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),
                .PointLightCount = (uint32_t)pointLightView.size_hint(),
                .SunColor = m_SunColor,
                .SunIntensity = m_SunIntensity,
                .SunDirection = m_SunDirection,
            };

            benzin::MappedData passBuffer{ *m_FrameResources.PassBuffer };
            passBuffer.Write(passData, m_SwapChain.GetCurrentFrameIndex());
        }

        {
            const uint32_t startElementIndex = ms_MaxPointLightCount * m_SwapChain.GetCurrentFrameIndex();
            benzin::MappedData pointLightBuffer{ *m_FrameResources.PointLightBuffer };

            size_t i = 0;
            for (auto&& [entity, tc, plc] : pointLightView.each())
            {
                // https://wiki.ogre3d.org/Light+Attenuation+Shortcut
                const PointLight pointLight
                {
                    .Color = plc.Color,
                    .Intensity = plc.Intensity,
                    .WorldPosition = tc.Translation,
                    .ConstantAttenuation = 1.0f,
                    .LinearAttenuation = 4.5f / plc.Range,
                    .ExponentialAttenuation = 75.0f / (plc.Range * plc.Range),
                };

                pointLightBuffer.Write(pointLight, startElementIndex + i++);
            }
        }
    }

    void DeferredLightingPass::OnExecute(const GeometryPass::GBuffer& gbuffer)
    {
        enum class RootConstant : uint32_t
        {
            PassBufferIndex,

            AlbedoTextureIndex,
            WorldNormalTextureIndex,
            EmissiveTextureIndex,
            RoughnessMetalnessTextureIndex,
            DepthTextureIndex,

            PointLightBufferIndex,

            OutputType,
        };

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(*m_OutputTexture, benzin::ResourceState::RenderTarget);

        commandList.SetRenderTargets({ m_OutputTexture->GetRenderTargetView() });
        commandList.ClearRenderTarget(m_OutputTexture->GetRenderTargetView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, m_FrameResources.PassBuffer->GetConstantBufferView(m_SwapChain.GetCurrentFrameIndex()));

            commandList.SetRootShaderResource(RootConstant::AlbedoTextureIndex, gbuffer.Albedo->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::WorldNormalTextureIndex, gbuffer.WorldNormal->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::EmissiveTextureIndex, gbuffer.Emissive->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::RoughnessMetalnessTextureIndex, gbuffer.RoughnessMetalness->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::DepthTextureIndex, gbuffer.DepthStencil->GetShaderResourceView());

            commandList.SetRootShaderResource(RootConstant::PointLightBufferIndex, m_FrameResources.PointLightBuffer->GetShaderResourceView(m_SwapChain.GetCurrentFrameIndex()));

            commandList.SetRootConstant(RootConstant::OutputType, magic_enum::enum_integer(m_OutputType));

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);

            BenzinGPUTimerSlotScopeMeasurement(m_GPUTimer, commandList, GPUTimerIndex::_DeferredLightingPass);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(*m_OutputTexture, benzin::ResourceState::Present);
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

            static constexpr auto names = magic_enum::enum_names<OutputType>();

            if (ImGui::BeginListBox("##listbox", ImVec2{ -FLT_MIN, 0.0f }))
            {
                for (uint32_t i = 0; i < magic_enum::enum_count<OutputType>(); ++i)
                {
                    const bool isSelected = magic_enum::enum_integer(m_OutputType) == i;

                    if (ImGui::Selectable(names[i].data(), isSelected))
                    {
                        m_OutputType = *magic_enum::enum_cast<OutputType>(i);
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
            .Format = benzin::config::g_BackBufferFormat,
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowRenderTarget,
            .IsNeedRenderTargetView = true,
        });
    }

    // EnvironmentPass
    EnvironmentPass::EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
        , m_GPUTimer{ gpuTimer }
    {
        m_FrameResources.PassBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "EnvironmentPass_PassBuffer",
            .ElementSize = sizeof(EnvironmentPassData),
            .ElementCount = benzin::config::g_BackBufferCount,
            .Flags = benzin::BufferFlag::ConstantBuffer,
        });

        for (uint32_t i = 0; i < benzin::config::g_BackBufferCount; ++i)
        {
            BenzinAssert(m_FrameResources.PassBuffer->PushConstantBufferView({ .ElementIndex = i }) == i);
        }

        m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "EnvironmentPass",
            .VertexShader{ "fullscreen_triangle.hlsl", "VS_MainDepth1" },
            .PixelShader{ "environment.hlsl", "PS_Main" },
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

            IBLTextureGenerator iblTextureGenerator{ m_Device };
            auto* irradianceTexture = iblTextureGenerator.GenerateIrradianceTexture(*m_CubeTexture);
            irradianceTexture->PushShaderResourceView();

            m_EnvironmentTexture = std::unique_ptr<benzin::Texture>{ irradianceTexture };
        }
    }

    void EnvironmentPass::OnUpdate(const benzin::Camera& camera)
    {
        const EnvironmentPassData passData
        {
            .InverseViewProjectionMatrix = camera.GetInverseViewDirectionProjectionMatrix(),
        };

        benzin::MappedData passBuffer{ *m_FrameResources.PassBuffer };
        passBuffer.Write(passData, m_SwapChain.GetCurrentFrameIndex());
    }

    void EnvironmentPass::OnExecute(benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil)
    {
        enum class RootConstant : uint32_t
        {
            PassBufferIndex,
            CubeTextureIndex,
        };

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::ResourceState::RenderTarget);
        commandList.SetResourceBarrier(gbufferDepthStecil, benzin::ResourceState::DepthWrite);

        commandList.SetRenderTargets({ deferredLightingOutputTexture.GetRenderTargetView() }, &gbufferDepthStecil.GetDepthStencilView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, m_FrameResources.PassBuffer->GetConstantBufferView(m_SwapChain.GetCurrentFrameIndex()));
            commandList.SetRootConstantBuffer(RootConstant::CubeTextureIndex, m_CubeTexture->GetShaderResourceView());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);

            BenzinGPUTimerSlotScopeMeasurement(m_GPUTimer, commandList, GPUTimerIndex::_EnvironmentPass);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::ResourceState::Present);
        commandList.SetResourceBarrier(gbufferDepthStecil, benzin::ResourceState::Present);
    }

    // MainLayer
    MainLayer::MainLayer(const benzin::GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
        , m_GPUTimer{ std::make_unique<benzin::GPUTimer>(m_Device, m_Device.GetGraphicsCommandQueue(), magic_enum::enum_count<GPUTimerIndex>()) }
        , m_GeometryPass{ m_Device, m_SwapChain, *m_GPUTimer }
        , m_DeferredLightingPass{ m_Device, m_SwapChain, *m_GPUTimer }
        , m_EnvironmentPass{ m_Device, m_SwapChain, *m_GPUTimer }
    {
        CreateResources();
        CreateEntities();
    }

    MainLayer::~MainLayer()
    {
        const auto view = m_Registry.view<benzin::ModelComponent>();

        for (const auto& [entity, mc] : view.each())
        {
            mc.Model.reset();
        }
    }

    void MainLayer::OnEndFrame()
    {
        m_GPUTimer->OnEndFrame(m_Device.GetGraphicsCommandQueue().GetCommandList());
    }

    void MainLayer::OnEvent(benzin::Event& event)
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

    void MainLayer::OnUpdate()
    {
        m_FlyCameraController.OnUpdate(s_FrameTimer.GetDeltaTime());

        const benzin::MilliSeconds dt = s_FrameTimer.GetDeltaTime();

        {
#if 1
            {
                auto& tc = m_Registry.get<benzin::TransformComponent>(m_BoomBoxEntity);
                tc.Rotation.x += 0.0001f * dt.count();
                tc.Rotation.z += 0.0002f * dt.count();
            }

            {
                auto& tc = m_Registry.get<benzin::TransformComponent>(m_DamagedHelmetEntity);
                tc.Rotation.x += 0.0001f * dt.count();
                tc.Rotation.y -= 0.00015f * dt.count();
            }
#endif

            const uint32_t startElementIndex = ms_MaxEntityCount * m_SwapChain.GetCurrentFrameIndex();
            benzin::MappedData entityDataBuffer{ *m_FrameResources.EntityDataBuffer };

            const auto view = m_Registry.view<benzin::TransformComponent>();
            for (const auto& [entity, tc] : view.each())
            {
                const auto& tc = view.get<benzin::TransformComponent>(entity);

                const EntityData entityData
                {
                    .WorldMatrix = tc.GetMatrix(),
                    .InverseWorldMatrix = tc.GetInverseMatrix(),
                };

                entityDataBuffer.Write(entityData, startElementIndex + magic_enum::enum_integer(entity));
            }
        }

        m_GeometryPass.OnUpdate(m_Camera);
        m_DeferredLightingPass.OnUpdate(m_Camera, m_Registry);
        m_EnvironmentPass.OnUpdate(m_Camera);
    }

    void MainLayer::OnRender()
    {
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinGPUTimerSlotScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex::_Total);

        m_GeometryPass.OnExecute(m_Registry, *m_FrameResources.EntityDataBuffer);
        m_DeferredLightingPass.OnExecute(m_GeometryPass.GetGBuffer());

        if (m_DeferredLightingPass.GetOutputType() == DeferredLightingPass::OutputType::Final)
        {
            m_EnvironmentPass.OnExecute(m_DeferredLightingPass.GetOutputTexture(), *m_GeometryPass.GetGBuffer().DepthStencil);
        }

        {
            auto& currentBackBuffer = *m_SwapChain.GetCurrentBackBuffer();
            auto& deferredLightingOutputTexture = m_DeferredLightingPass.GetOutputTexture();

            commandList.SetResourceBarrier(currentBackBuffer, benzin::ResourceState::CopyDestination);
            commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::ResourceState::CopySource);

            {
                BenzinGPUTimerSlotScopeMeasurement(*m_GPUTimer, commandList, GPUTimerIndex::_BackBufferCopy);
                commandList.CopyResource(*m_SwapChain.GetCurrentBackBuffer(), m_DeferredLightingPass.GetOutputTexture());
            }

            commandList.SetResourceBarrier(currentBackBuffer, benzin::ResourceState::Present);
            commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::ResourceState::Present);
        }
    }

    void MainLayer::OnImGuiRender()
    {
        m_FlyCameraController.OnImGuiRender();

        m_DeferredLightingPass.OnImGuiRender();
#if 0
        ImGui::Begin("GBuffer");
        {
            static constexpr auto displayTexture = [&](const benzin::Texture& texture)
            {
                const float widgetWidth = ImGui::GetContentRegionAvail().x;

                BenzinAssert(texture.HasShaderResourceView());

                const uint64_t gpuHandle = texture.GetShaderResourceView().GetGPUHandle();
                const float textureAspectRatio = static_cast<float>(texture.GetWidth()) / static_cast<float>(texture.GetHeight());

                const ImVec2 textureSize
                {
                    widgetWidth,
                    widgetWidth / textureAspectRatio
                };

                const ImVec2 uv0{ 0.0f, 0.0f };
                const ImVec2 uv1{ 1.0f, 1.0f };
                const ImVec4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
                const ImVec4 borderColor{ 1.0f, 1.0f, 1.0f, 0.3f };

                ImGui::Text("%s", texture.GetDebugName().c_str());
                ImGui::Image(reinterpret_cast<ImTextureID>(gpuHandle), textureSize, uv0, uv1, tintColor, borderColor);
            };

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

            const float widgetWidth = ImGui::GetContentRegionAvail().x;

            {
                //displayTexture(*m_HDRTexture);
                displayTexture(*m_GBuffer.Albedo);
                displayTexture(*m_GBuffer.WorldNormal);
                displayTexture(*m_GBuffer.Emissive);
                displayTexture(*m_GBuffer.RoughnessMetalness);
                displayTexture(*m_GBuffer.DepthStencil);
            }

            ImGui::PopStyleVar();
        }
        ImGui::End();
#endif

        ImGui::Begin("PointLight");
        {
            const auto view = m_Registry.view<benzin::TransformComponent, benzin::PointLightComponent>();

            for (auto&& [entity, transformComponent, pointLightComponent] : view.each())
            {
                ImGui::PushID(magic_enum::enum_integer(entity));

                ImGui::Text("Entity: %d", entity);
                ImGui::DragFloat3("Translation", reinterpret_cast<float*>(&transformComponent.Translation), 0.1f);
                ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&transformComponent.Scale), 0.1f);
                ImGui::Separator();
                ImGui::ColorEdit3("Color", reinterpret_cast<float*>(&pointLightComponent.Color));
                ImGui::DragFloat("Intensity", &pointLightComponent.Intensity, 0.1f);
                ImGui::DragFloat("Range", &pointLightComponent.Range, 0.1f);

                ImGui::PopID();
            }
        }
        ImGui::End();

        ImGui::Begin("MainLayer Stats");
        {
            static magic_enum::containers::array<GPUTimerIndex, float> times;

            if (s_FrameStats.IsReady())
            {
                for (const auto [index, time] : times | std::views::enumerate)
                {
                    time = m_GPUTimer->GetElapsedTime((uint32_t)index).count();
                }
            }

            for (const auto [index, time] : times | std::views::enumerate)
            {
                const auto indexName = magic_enum::enum_name(magic_enum::enum_value<GPUTimerIndex>(index)).substr(1);

                ImGui::Text(std::format("GPU {} Time: {:.4f} ms", indexName, time).c_str());
            }
        }
        ImGui::End();
    }

    void MainLayer::CreateResources()
    {
        m_FrameResources.EntityDataBuffer = std::make_unique<benzin::Buffer>(m_Device, benzin::BufferCreation
        {
            .DebugName = "MainLayer_EntityBuffer",
            .ElementSize = sizeof(EntityData),
            .ElementCount = ms_MaxEntityCount * benzin::config::g_BackBufferCount,
            .Flags = benzin::BufferFlag::UploadBuffer,
        });

        for (uint32_t i = 0; i < benzin::config::g_BackBufferCount; ++i)
        {
            BenzinAssert(m_FrameResources.EntityDataBuffer->PushStructureBufferView({ .FirstElementIndex = ms_MaxEntityCount * i, .ElementCount = ms_MaxEntityCount }) == i);
        }
    }

    void MainLayer::CreateEntities()
    {
        {
            const entt::entity entity = m_Registry.create();
            auto& tc = m_Registry.emplace<benzin::TransformComponent>(entity);
            tc.Rotation = { 0.0f, DirectX::XM_PI, 0.0f };

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(entity);
            mc.Model = std::make_shared<benzin::Model>(m_Device, "Sponza/glTF/Sponza.gltf");
        }

        {
            m_BoomBoxEntity = m_Registry.create();

            auto& tc = m_Registry.emplace<benzin::TransformComponent>(m_BoomBoxEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 30.0f, 30.0f, 30.0f };
            tc.Translation = { 0.0f, 0.6f, 0.0f };

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(m_BoomBoxEntity);
            mc.Model = std::make_shared<benzin::Model>(m_Device, "BoomBox/glTF-Binary/BoomBox.glb");
        }

        {
            m_DamagedHelmetEntity = m_Registry.create();

            auto& tc = m_Registry.emplace<benzin::TransformComponent>(m_DamagedHelmetEntity);
            tc.Rotation = { 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f };
            tc.Scale = { 0.4f, 0.4f, 0.4f };
            tc.Translation = { 1.0f, 0.5f, -0.5f };

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(m_DamagedHelmetEntity);
            mc.Model = std::make_shared<benzin::Model>(m_Device, "DamagedHelmet/glTF/DamagedHelmet.gltf");
        }

        {
            const auto sphereMesh = std::make_shared<benzin::MeshCollection>(m_Device, benzin::MeshCollectionCreation
            {
                .DebugName = "Geosphere",
                .Meshes{ benzin::GetDefaultGeosphere() }
            });

            const size_t rowCount = 20;
            const size_t columnCount = 6;
            const size_t count = rowCount * columnCount;

            std::vector<benzin::Material> materials;
            materials.reserve(count);

            std::vector<benzin::DrawableMesh> drawableMeshes;
            drawableMeshes.reserve(count);

            for (size_t i = 0; i < count; ++i)
            {
                materials.push_back(benzin::Material
                {
                    .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                    .EmissiveFactor
                    {
                        benzin::Random::GetNumber<float>(0.0f, 1.0f),
                        benzin::Random::GetNumber<float>(0.0f, 1.0f),
                        benzin::Random::GetNumber<float>(0.0f, 1.0f),
                    },
                });

                drawableMeshes.push_back(benzin::DrawableMesh
                {
                    .MeshIndex = 0,
                    .MaterialIndex = (uint32_t)i,
                });
            }

            const auto model = std::make_shared<benzin::Model>(m_Device, benzin::ModelCreation
            {
                .DebugName = "PointLightSpheres",
                .MeshCollection = sphereMesh,
                .DrawableMeshes = drawableMeshes,
                .Materials = materials,
            });

            for (size_t i = 0; i < rowCount; ++i)
            {
                for (size_t j = 0; j < columnCount; ++j)
                {
                    const size_t k = i * columnCount + j;

                    const entt::entity pointLightEntity = m_Registry.create();

                    const float x = ((float)rowCount / -2.0f + (float)i) * 0.5f;
                    const float y = 0.2f;
                    const float z = ((float)columnCount / -2.0f + (float)j) * 0.5f;

                    auto& tc = m_Registry.emplace<benzin::TransformComponent>(pointLightEntity);
                    tc.Scale = { 0.02f, 0.02f, 0.02f };
                    tc.Translation = { x, y, z };

                    const DirectX::XMVECTOR color = DirectX::XMLoadFloat3(&materials[k].EmissiveFactor);

                    auto& pointLight = m_Registry.emplace<benzin::PointLightComponent>(pointLightEntity);
                    DirectX::XMStoreFloat3(&pointLight.Color, color);
                    pointLight.Intensity = 2.0f;
                    pointLight.Range = 3.0f;

                    auto& modelComponent = m_Registry.emplace<benzin::ModelComponent>(pointLightEntity);
                    modelComponent.Model = model;
                    modelComponent.DrawableMeshIndex = (uint32_t)k;
                }
            }
        }
    }

} // namespace sandbox
