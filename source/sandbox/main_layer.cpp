#include "bootstrap.hpp"

#include "main_layer.hpp"

#include <benzin/engine/geometry_generator.hpp>
#include <benzin/graphics/api/command_list.hpp>
#include <benzin/graphics/api/command_queue.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/texture_loader.hpp>
#include <benzin/system/event_dispatcher.hpp>

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

    } // anonymous namespace

    // GeometryPass
    GeometryPass::GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        for (size_t i = 0; i < m_Resources.size(); ++i)
        {
            auto& resources = m_Resources[i];

            const benzin::BufferResource::Config config
            {
                .ElementSize{ sizeof(GBufferPassData) },
                .ElementCount{ 1 },
                .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
            };

            resources.PassBuffer = std::make_unique<benzin::BufferResource>(m_Device, config);
            resources.PassBuffer->SetDebugName(fmt::format("GeomtryPass_PassBuffer_{}", i));
            resources.PassBuffer->PushConstantBufferView();
        }

        {
            const benzin::PipelineState::GraphicsConfig config
            {
                .VertexShader{ "gbuffer.hlsl", "VS_Main" },
                .PixelShader{ "gbuffer.hlsl", "PS_Main" },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .DepthState{ benzin::DepthState::GetLess() },
                .RenderTargetViewFormats
                {
                    ms_Color0Format,
                    ms_Color1Format,
                    ms_Color2Format,
                    ms_Color3Format,
                },
                .DepthStencilViewFormat{ ms_DepthStencilFormat },
            };

            m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, config);
            m_PipelineState->SetDebugName("GeometryPass");
        }

        OnResize(width, height);
    }

    void GeometryPass::OnUpdate(const benzin::Camera& camera)
    {
        const GBufferPassData passData
        {
            .ViewMatrix{ camera.GetViewMatrix() },
            .ProjectionMatrix{ camera.GetProjectionMatrix() },
            .ViewProjectionMatrix{ camera.GetViewProjectionMatrix() },
            .WorldCameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()) },
        };

        benzin::MappedData passBuffer{ *m_Resources[m_SwapChain.GetCurrentBackBufferIndex()].PassBuffer };
        passBuffer.Write(passData);
    }

    void GeometryPass::OnExecute(const entt::registry& registry, const benzin::BufferResource& entityBuffer)
    {
        enum class RootConstant : uint32_t
        {
            PassBufferIndex,

            EntityBufferIndex,
            EntityIndex,

            VertexBufferIndex,
            IndexBufferIndex,
            MeshPrimitiveBufferIndex,

            DrawPrimitiveBufferIndex,
            DrawPrimitiveIndex,
            NodeBufferIndex,
            NodeIndex,
            MaterialBufferIndex,
        };

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
        auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(*m_GBuffer.Albedo, benzin::Resource::State::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.WorldNormal, benzin::Resource::State::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.Emissive, benzin::Resource::State::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.RoughnessMetalness, benzin::Resource::State::RenderTarget);
        commandList.SetResourceBarrier(*m_GBuffer.DepthStencil, benzin::Resource::State::DepthWrite);

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
            commandList.SetPipelineState(*m_PipelineState);
            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, resources.PassBuffer->GetConstantBufferView());
            commandList.SetRootShaderResource(RootConstant::EntityBufferIndex, entityBuffer.GetShaderResourceView());

            const auto view = registry.view<benzin::TransformComponent, benzin::ModelComponent>();

            for (const auto& [entity, tc, mc] : view.each())
            {
                commandList.SetRootConstant(RootConstant::EntityIndex, magic_enum::enum_integer(entity));

                const auto& model = mc.Model;
                const auto& mesh = model->GetMesh();

                // Mesh
                {
                    commandList.SetRootShaderResource(RootConstant::MeshPrimitiveBufferIndex, mesh->GetPrimitiveBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::VertexBufferIndex, mesh->GetVertexBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::IndexBufferIndex, mesh->GetIndexBuffer()->GetShaderResourceView());
                }

                // Model
                {
                    commandList.SetRootShaderResource(RootConstant::DrawPrimitiveBufferIndex, model->GetDrawPrimitiveBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::NodeBufferIndex, model->GetNodeBuffer()->GetShaderResourceView());
                    commandList.SetRootShaderResource(RootConstant::MaterialBufferIndex, model->GetMaterialBuffer()->GetShaderResourceView());
                }

                // If there is no need to draw whole model
                if (mc.DrawPrimitiveIndex)
                {
                    const benzin::Model::DrawPrimitive& drawPrimitive = model->GetDrawPrimitives()[*mc.DrawPrimitiveIndex];
                    const benzin::Mesh::Primitive& meshPrimitive = mesh->GetPrimitives()[drawPrimitive.MeshPrimitiveIndex];

                    commandList.SetRootConstant(RootConstant::DrawPrimitiveIndex, *mc.DrawPrimitiveIndex);
                    commandList.SetPrimitiveTopology(meshPrimitive.PrimitiveTopology);
                    commandList.DrawVertexed(meshPrimitive.IndexCount);

                    continue;
                }

                for (size_t nodeIndex = 0; nodeIndex < model->GetNodes().size(); ++nodeIndex)
                {
                    const benzin::Model::Node& node = model->GetNodes()[nodeIndex];

                    commandList.SetRootConstant(RootConstant::NodeIndex, static_cast<uint32_t>(nodeIndex));

                    for (uint32_t drawPrimitiveIndex : node.DrawPrimitiveRange)
                    {
                        const benzin::Model::DrawPrimitive& drawPrimitive = model->GetDrawPrimitives()[drawPrimitiveIndex];
                        const benzin::Mesh::Primitive& meshPrimitive = mesh->GetPrimitives()[drawPrimitive.MeshPrimitiveIndex];

                        commandList.SetRootConstant(RootConstant::DrawPrimitiveIndex, drawPrimitiveIndex);
                        commandList.SetPrimitiveTopology(meshPrimitive.PrimitiveTopology);
                        commandList.DrawVertexed(meshPrimitive.IndexCount);
                    }
                }
            }
        }

        commandList.SetResourceBarrier(*m_GBuffer.Albedo, benzin::Resource::State::Present);
        commandList.SetResourceBarrier(*m_GBuffer.WorldNormal, benzin::Resource::State::Present);
        commandList.SetResourceBarrier(*m_GBuffer.Emissive, benzin::Resource::State::Present);
        commandList.SetResourceBarrier(*m_GBuffer.RoughnessMetalness, benzin::Resource::State::Present);
        commandList.SetResourceBarrier(*m_GBuffer.DepthStencil, benzin::Resource::State::Present);
    }

    void GeometryPass::OnResize(uint32_t width, uint32_t height)
    {
        static const auto createGBufferRenderTarget = [this](
            benzin::GraphicsFormat format,
            uint32_t width,
            uint32_t height,
            std::string_view debugName,
            std::shared_ptr<benzin::TextureResource>& outRenderTarget
            )
        {
            const benzin::TextureResource::Config config
            {
                .Type{ benzin::TextureResource::Type::Texture2D },
                .Width{ width },
                .Height{ height },
                .MipCount{ 1 },
                .Format{ format },
                .Flags{ benzin::TextureResource::Flags::BindAsRenderTarget }
            };

            outRenderTarget = std::make_shared<benzin::TextureResource>(m_Device, config);
            outRenderTarget->SetDebugName(debugName);
            outRenderTarget->PushRenderTargetView();
            outRenderTarget->PushShaderResourceView();
        };

        createGBufferRenderTarget(ms_Color0Format, width, height, "GBuffer_Albedo", m_GBuffer.Albedo);
        createGBufferRenderTarget(ms_Color1Format, width, height, "GBuffer_WorldNormal", m_GBuffer.WorldNormal);
        createGBufferRenderTarget(ms_Color2Format, width, height, "GBuffer_Emissive", m_GBuffer.Emissive);
        createGBufferRenderTarget(ms_Color3Format, width, height, "GBuffer_RoughnessMetalness", m_GBuffer.RoughnessMetalness);

        {
            const benzin::TextureResource::Config config
            {
                .Type{ benzin::TextureResource::Type::Texture2D },
                .Width{ width },
                .Height{ height },
                .MipCount{ 1 },
                .Format{ ms_DepthStencilFormat },
                .Flags{ benzin::TextureResource::Flags::BindAsDepthStencil }
            };

            auto& depthStencil = m_GBuffer.DepthStencil;
            depthStencil = std::make_shared<benzin::TextureResource>(m_Device, config);
            depthStencil->SetDebugName("GBuffer_DepthStencil");
            depthStencil->PushDepthStencilView();
            depthStencil->PushShaderResourceView({ .Format{ benzin::GraphicsFormat::D24Unorm_X8Typeless } });
        }
    }

    // DeferredLightingPass
    DeferredLightingPass::DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        for (size_t i = 0; i < m_Resources.size(); ++i)
        {
            auto& resources = m_Resources[i];

            {
                const benzin::BufferResource::Config config
                {
                    .ElementSize{ sizeof(DeferredLightingPassData) },
                    .ElementCount{ 1 },
                    .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
                };

                resources.PassBuffer = std::make_unique<benzin::BufferResource>(m_Device, config);
                resources.PassBuffer->SetDebugName(fmt::format("DeferredLightingPass_PassBuffer_{}", i));
                resources.PassBuffer->PushConstantBufferView();
            }

            {
                const benzin::BufferResource::Config config
                {
                    .ElementSize{ sizeof(PointLight) },
                    .ElementCount{ 20 * 20 },
                    .Flags{ benzin::BufferResource::Flags::Dynamic }
                };

                resources.PointLightBuffer = std::make_unique<benzin::BufferResource>(m_Device, config);
                resources.PointLightBuffer->SetDebugName(fmt::format("DeferredLightingPass_PointLightBuffer_{}", i));
                resources.PointLightBuffer->PushShaderResourceView();
            }
        }

        {
            const benzin::PipelineState::GraphicsConfig config
            {
                .VertexShader{ "fullscreen_triangle.hlsl", "VS_Main" },
                .PixelShader{ "deferred_lighting.hlsl", "PS_Main" },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .DepthState{ benzin::DepthState::GetDisabled() },
                .RenderTargetViewFormats{ benzin::GraphicsFormat::RGBA8Unorm },
            };

            m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, config);
            m_PipelineState->SetDebugName("DeferredLightingPass");
        }

        OnResize(width, height);
    }

    void DeferredLightingPass::OnUpdate(const benzin::Camera& camera, const entt::registry& registry)
    {
        auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];

        const auto pointLightView = registry.view<const benzin::TransformComponent, const benzin::PointLightComponent>();

        {
            const DeferredLightingPassData passData
            {
                .InverseViewMatrix{ camera.GetInverseViewMatrix() },
                .InverseProjectionMatrix{ camera.GetInverseProjectionMatrix() },
                .WorldCameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()) },
                .PointLightCount{ static_cast<uint32_t>(pointLightView.size_hint()) },
                .SunColor{ m_SunColor },
                .SunIntensity{ m_SunIntensity },
                .SunDirection{ m_SunDirection },
            };

            benzin::MappedData passBuffer{ *resources.PassBuffer };
            passBuffer.Write(passData);
        }

        {
            benzin::MappedData pointLightBuffer{ *resources.PointLightBuffer };

            size_t i = 0;
            for (auto&& [entity, tc, plc] : pointLightView.each())
            {
                // https://wiki.ogre3d.org/Light+Attenuation+Shortcut
                const PointLight pointLight
                {
                    .Color{ plc.Color },
                    .Intensity{ plc.Intensity },
                    .WorldPosition{ tc.Translation },
                    .ConstantAttenuation{ 1.0f },
                    .LinearAttenuation{ 4.5f / plc.Range },
                    .ExponentialAttenuation{ 75.0f / (plc.Range * plc.Range) },
                };

                pointLightBuffer.Write(pointLight, i++);
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
        auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(*m_OutputTexture, benzin::Resource::State::RenderTarget);

        commandList.SetRenderTargets({ m_OutputTexture->GetRenderTargetView() });
        commandList.ClearRenderTarget(m_OutputTexture->GetRenderTargetView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, resources.PassBuffer->GetConstantBufferView());

            commandList.SetRootShaderResource(RootConstant::AlbedoTextureIndex, gbuffer.Albedo->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::WorldNormalTextureIndex, gbuffer.WorldNormal->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::EmissiveTextureIndex, gbuffer.Emissive->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::RoughnessMetalnessTextureIndex, gbuffer.RoughnessMetalness->GetShaderResourceView());
            commandList.SetRootShaderResource(RootConstant::DepthTextureIndex, gbuffer.DepthStencil->GetShaderResourceView());

            commandList.SetRootShaderResource(RootConstant::PointLightBufferIndex, resources.PointLightBuffer->GetShaderResourceView());

            commandList.SetRootConstant(RootConstant::OutputType, magic_enum::enum_integer(m_OutputType));

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(*m_OutputTexture, benzin::Resource::State::Present);
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
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .MipCount{ 1 },
            .Format{ benzin::config::GetBackBufferFormat() },
            .Flags{ benzin::TextureResource::Flags::BindAsRenderTarget }
        };

        m_OutputTexture = std::make_unique<benzin::TextureResource>(m_Device, config);
        m_OutputTexture->SetDebugName("DeferredLightingPass_OutputTexture");
        m_OutputTexture->PushRenderTargetView();
    }

    // EnvironmentPass
    EnvironmentPass::EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height)
        : m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        for (size_t i = 0; i < m_Resources.size(); ++i)
        {
            auto& resources = m_Resources[i];

            const benzin::BufferResource::Config config
            {
                .ElementSize{ sizeof(EnvironmentPassData) },
                .ElementCount{ 1 },
                .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
            };

            resources.PassBuffer = std::make_unique<benzin::BufferResource>(m_Device, config);
            resources.PassBuffer->SetDebugName(fmt::format("EnvironmentPass_PassBuffer_{}", i));
            resources.PassBuffer->PushConstantBufferView();
        }

        {
            const benzin::PipelineState::GraphicsConfig config
            {
                .VertexShader{ "fullscreen_triangle.hlsl", "VS_MainDepth1" },
                .PixelShader{ "environment.hlsl", "PS_Main" },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .DepthState
                {
                    .IsEnabled{ true },
                    .IsWriteEnabled{ false },
                    .ComparisonFunction{ benzin::ComparisonFunction::Equal },
                },
                .RenderTargetViewFormats{ benzin::GraphicsFormat::RGBA8Unorm },
                .DepthStencilViewFormat{ benzin::GraphicsFormat::D24Unorm_S8Uint },
            };

            m_PipelineState = std::make_unique<benzin::PipelineState>(m_Device, config);
            m_PipelineState->SetDebugName("EnvironmentPass");
        }

        {
            m_CubeTexture = std::unique_ptr<benzin::TextureResource>{ m_Device.GetTextureLoader().LoadCubeMapFromHDRFile("scythian_tombs_2_4k.hdr", 1024) };
            m_CubeTexture->PushShaderResourceView();

            IBLTextureGenerator iblTextureGenerator{ m_Device };
            auto* irradianceTexture = iblTextureGenerator.GenerateIrradianceTexture(*m_CubeTexture);
            irradianceTexture->PushShaderResourceView();

            m_EnvironmentTexture = std::unique_ptr<benzin::TextureResource>{ irradianceTexture };
        }
    }

    void EnvironmentPass::OnUpdate(const benzin::Camera& camera)
    {
        auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];

        {
            const EnvironmentPassData passData
            {
                .InverseViewProjectionMatrix{ camera.GetInverseViewDirectionProjectionMatrix() }
            };

            benzin::MappedData passBuffer{ *resources.PassBuffer };
            passBuffer.Write(passData);
        }
    }

    void EnvironmentPass::OnExecute(benzin::TextureResource& deferredLightingOutputTexture, benzin::TextureResource& gbufferDepthStecil)
    {
        enum class RootConstant : uint32_t
        {
            PassBufferIndex,
            CubeTextureIndex,
        };

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();
        auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];

        commandList.SetViewport(m_SwapChain.GetViewport());
        commandList.SetScissorRect(m_SwapChain.GetScissorRect());

        commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::Resource::State::RenderTarget);
        commandList.SetResourceBarrier(gbufferDepthStecil, benzin::Resource::State::DepthWrite);

        commandList.SetRenderTargets({ deferredLightingOutputTexture.GetRenderTargetView() }, &gbufferDepthStecil.GetDepthStencilView());

        {
            commandList.SetPipelineState(*m_PipelineState);

            commandList.SetRootConstantBuffer(RootConstant::PassBufferIndex, resources.PassBuffer->GetConstantBufferView());
            commandList.SetRootConstantBuffer(RootConstant::CubeTextureIndex, m_CubeTexture->GetShaderResourceView());

            commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
            commandList.DrawVertexed(3);
        }

        commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::Resource::State::Present);
        commandList.SetResourceBarrier(gbufferDepthStecil, benzin::Resource::State::Present);
    }

    // MainLayer
    MainLayer::MainLayer(benzin::Window& window, benzin::Device& device, benzin::SwapChain& swapChain)
        : m_Window{ window }
        , m_Device{ device }
        , m_SwapChain{ swapChain }
        , m_GeometryPass{ device, swapChain, window.GetWidth(), window.GetHeight() }
        , m_DeferredLightingPass{ device, swapChain, window.GetWidth(), window.GetHeight() }
        , m_EnvironmentPass{ device, swapChain, window.GetWidth(), window.GetHeight() }
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

    bool MainLayer::OnAttach()
    {
        return true;
    }

    bool MainLayer::OnDetach()
    {
        return true;
    }

    void MainLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };

        dispatcher.Dispatch<benzin::WindowResizedEvent>([&](benzin::WindowResizedEvent& event)
        {
            m_GeometryPass.OnResize(event.GetWidth(), event.GetHeight());
            m_DeferredLightingPass.OnResize(event.GetWidth(), event.GetHeight());

            return false;
        });
    }

    void MainLayer::OnUpdate(float dt)
    {
        m_FlyCameraController.OnUpdate(dt);

        m_GeometryPass.OnUpdate(m_Camera);
        m_DeferredLightingPass.OnUpdate(m_Camera, m_Registry);
        m_EnvironmentPass.OnUpdate(m_Camera);

        {
            auto& resources = m_Resources[m_SwapChain.GetCurrentBackBufferIndex()];
            benzin::MappedData entityDataBuffer{ *resources.EntityDataBuffer };

            const auto view = m_Registry.view<benzin::TransformComponent>();

            for (const auto& [entity, tc] : view.each())
            {
                const auto& tc = view.get<benzin::TransformComponent>(entity);

                const EntityData entityData
                {
                    .WorldMatrix{ tc.GetMatrix() },
                    .InverseWorldMatrix{ tc.GetInverseMatrix() }
                };

                entityDataBuffer.Write(entityData, magic_enum::enum_integer(entity));
            }
        }
    }

    void MainLayer::OnRender(float dt)
    {
        m_GeometryPass.OnExecute(m_Registry, *m_Resources[m_SwapChain.GetCurrentBackBufferIndex()].EntityDataBuffer);
        m_DeferredLightingPass.OnExecute(m_GeometryPass.GetGBuffer());

        if (m_DeferredLightingPass.GetOutputType() == DeferredLightingPass::OutputType::Final)
        {
            m_EnvironmentPass.OnExecute(m_DeferredLightingPass.GetOutputTexture(), *m_GeometryPass.GetGBuffer().DepthStencil);
        }

        {
            auto& currentBackBuffer = *m_SwapChain.GetCurrentBackBuffer();
            auto& deferredLightingOutputTexture = m_DeferredLightingPass.GetOutputTexture();

            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            commandList.SetResourceBarrier(currentBackBuffer, benzin::Resource::State::CopyDestination);
            commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::Resource::State::CopySource);

            commandList.CopyResource(*m_SwapChain.GetCurrentBackBuffer(), m_DeferredLightingPass.GetOutputTexture());

            commandList.SetResourceBarrier(currentBackBuffer, benzin::Resource::State::Present);
            commandList.SetResourceBarrier(deferredLightingOutputTexture, benzin::Resource::State::Present);
        }
    }

    void MainLayer::OnImGuiRender(float dt)
    {
        m_FlyCameraController.OnImGuiRender(dt);

        m_DeferredLightingPass.OnImGuiRender();
#if 0
        ImGui::Begin("GBuffer");
        {
            static constexpr auto displayTexture = [&](const benzin::TextureResource& texture)
            {
                const float widgetWidth = ImGui::GetContentRegionAvail().x;

                BENZIN_ASSERT(texture.HasShaderResourceView());

                const uint64_t gpuHandle = texture.GetShaderResourceView().GetGPUHandle();
                const float textureAspectRatio = static_cast<float>(texture.GetConfig().Width) / static_cast<float>(texture.GetConfig().Height);

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
    }

    void MainLayer::CreateResources()
    {
        for (size_t i = 0; i < m_Resources.size(); ++i)
        {
            auto& frameResource = m_Resources[i];

            {
                const benzin::BufferResource::Config config
                {
                    .ElementSize{ sizeof(EntityData) },
                    .ElementCount{ 20 * 20 + 1 },
                    .Flags{ benzin::BufferResource::Flags::Dynamic }
                };

                frameResource.EntityDataBuffer = std::make_unique<benzin::BufferResource>(m_Device, config);
                frameResource.EntityDataBuffer->SetDebugName(fmt::format("MainLayer_EntityBuffer_{}", i));
                frameResource.EntityDataBuffer->PushShaderResourceView();
            }
        }
    }

    void MainLayer::CreateEntities()
    {
        {
            const entt::entity entity = m_Registry.create();
            m_Registry.emplace<benzin::TransformComponent>(entity);

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(entity);
            mc.Model = std::make_shared<benzin::Model>(m_Device);
            //BENZIN_ASSERT(mc.Model->LoadFromGLTFFile("DamagedHelmet/glTF/DamagedHelmet.gltf"));
            //BENZIN_ASSERT(mc.Model->LoadFromGLTFFile("CesiumMilkTruck/glTF/CesiumMilkTruck.gltf"));
            BENZIN_ASSERT(mc.Model->LoadFromGLTFFile("Sponza/glTF/Sponza.gltf"));
        }

        {
            const entt::entity entity = m_Registry.create();
            auto& tc = m_Registry.emplace<benzin::TransformComponent>(entity);
            tc.Scale = { 30.0f, 30.0f, 30.0f };
            tc.Translation = { 0.0f, 0.5f, 0.0f };

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(entity);
            mc.Model = std::make_shared<benzin::Model>(m_Device);
            BENZIN_ASSERT(mc.Model->LoadFromGLTFFile("BoomBox/glTF-Binary/BoomBox.glb"));
        }

        {
            const entt::entity entity = m_Registry.create();
            auto& tc = m_Registry.emplace<benzin::TransformComponent>(entity);
            tc.Scale = { 0.4f, 0.4f, 0.4f };
            tc.Translation = { 1.0f, 0.6f, 0.0f };

            auto& mc = m_Registry.emplace<benzin::ModelComponent>(entity);
            mc.Model = std::make_shared<benzin::Model>(m_Device);
            BENZIN_ASSERT(mc.Model->LoadFromGLTFFile("DamagedHelmet/glTF/DamagedHelmet.gltf"));
        }

        {
            const benzin::geometry_generator::GeosphereConfig sphereConfig
            {
                .Radius{ 1.0f },
                .SubdivisionCount{ 0 },
            };

            const auto sphereMesh = std::make_shared<benzin::Mesh>(
                m_Device,
                std::vector{ benzin::geometry_generator::GenerateGeosphere(sphereConfig) },
                "Sphere"
                );

            const size_t rowCount = 32;
            const size_t columnCount = 6;
            const size_t count = rowCount * columnCount;

            std::vector<benzin::MaterialData> materialsData;
            materialsData.reserve(count);

            std::vector<benzin::Model::DrawPrimitive> drawPrimitives;
            drawPrimitives.reserve(count);

            for (size_t i = 0; i < rowCount; ++i)
            {
                for (size_t j = 0; j < columnCount; ++j)
                {
                    const size_t k = i * columnCount + j;

                    benzin::MaterialData& materialData = materialsData.emplace_back();
                    materialData.AlbedoFactor = { 0.0f, 0.0f, 0.0f, 0.0f };
                    materialData.EmissiveFactor =
                    {
                        static_cast<float>((k * 37) % 256) / 255.0f,
                        static_cast<float>((k * 71) % 256) / 255.0f,
                        static_cast<float>((k * 97) % 256) / 255.0f,
                    };

                    benzin::Model::DrawPrimitive& drawPrimitive = drawPrimitives.emplace_back();
                    drawPrimitive.MeshPrimitiveIndex = 0;
                    drawPrimitive.MaterialIndex = static_cast<uint32_t>(k);
                }
            }

            const auto model = std::make_shared<benzin::Model>(
                m_Device,
                sphereMesh,
                drawPrimitives,
                materialsData,
                "PointLightSpheres"
                );

            for (size_t i = 0; i < rowCount; ++i)
            {
                for (size_t j = 0; j < columnCount; ++j)
                {
                    const size_t k = i * columnCount + j;

                    const entt::entity pointLightEntity = m_Registry.create();

                    auto& tc = m_Registry.emplace<benzin::TransformComponent>(pointLightEntity);
                    tc.Scale = { 0.02f, 0.02f, 0.02f };
                    tc.Translation = {
                        (static_cast<float>(rowCount) / -2.0f + static_cast<float>(i)) * 0.5f,
                        0.2f,
                        (static_cast<float>(columnCount) / -2.0f + static_cast<float>(j)) * 0.5f
                    };

                    auto& pointLight = m_Registry.emplace<benzin::PointLightComponent>(pointLightEntity);
                    pointLight.Color = materialsData[k].EmissiveFactor;
                    pointLight.Intensity = 4.0f;
                    pointLight.Range = 4.0f;

                    auto& modelComponent = m_Registry.emplace<benzin::ModelComponent>(pointLightEntity);
                    modelComponent.Model = model;
                    modelComponent.DrawPrimitiveIndex = static_cast<uint32_t>(k);
                }
            }
        }
    }

} // namespace sandbox
