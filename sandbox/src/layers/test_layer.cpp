#include "bootstrap.hpp"

#include "test_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/core/application.hpp>

#include <spieler/system/event.hpp>
#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/blend_state.hpp>
#include <spieler/renderer/rasterizer_state.hpp>
#include <spieler/renderer/resource_barrier.hpp>
#include <spieler/renderer/depth_stencil_state.hpp>
#include <spieler/renderer/mapped_data.hpp>

#include <spieler/utility/random.hpp>

#if 0

namespace sandbox
{

    namespace cb
    {

        struct Pass
        {
            alignas(16) DirectX::XMMATRIX View{};
            alignas(16) DirectX::XMMATRIX Projection{};
            alignas(16) DirectX::XMFLOAT3 CameraPosition{};
            alignas(16) DirectX::XMFLOAT4 AmbientLight{};
            alignas(16) spieler::renderer::LightConstants Lights[g_MaxLightCount];

            alignas(16) DirectX::XMFLOAT4 FogColor{};
            float ForStart{};
            float FogRange{};
        };

        struct ColorObject
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        };

        struct Object
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            DirectX::XMMATRIX TextureTransform{ DirectX::XMMatrixIdentity() };
        };

        struct Material
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
        };

    } // namespace cb

    struct ColorConstants
    {
        DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct MirrorConstants
    {
        DirectX::XMVECTOR Plane{ 0.0f, 0.0f, 0.0f, 0.0f };
    };

    bool TestLayer::OnAttach()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        UpdateViewport();
        UpdateScissorRect();

        InitConstantBuffers();

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            SPIELER_ASSERT(InitTextures());
            SPIELER_ASSERT(InitMeshGeometries());
        }

        InitMaterials();
        SPIELER_ASSERT(InitRootSignatures());
        SPIELER_ASSERT(InitPipelineStates());

        InitRenderItems();
        InitLights();

        InitDepthStencil();

        m_BlurPass = std::make_unique<BlurPass>(window.GetWidth(), window.GetHeight());
        m_SobelFilterPass = std::make_unique<SobelFilterPass>(window.GetWidth(), window.GetHeight());

        return true;
    }

    void TestLayer::OnEvent(spieler::Event& event)
    {
        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        m_CameraController.OnEvent(event);
    }

    void TestLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Update Pass Constants
        {
            spieler::renderer::MappedData bufferData{ m_ConstantBuffers["pass"], 0 };

            m_PassConstants["direct"].View = m_Camera.GetView();
            m_PassConstants["direct"].Projection = m_Camera.GetProjection();
            DirectX::XMStoreFloat3(&m_PassConstants["direct"].CameraPosition, m_Camera.GetPosition());

            // Reflect light
            {
                const DirectX::XMMATRIX reflectOperator{ DirectX::XMMatrixReflect(m_RenderItems["mirror"]->GetComponent<MirrorConstants>().Plane) };

                PassConstants& direct{ m_PassConstants["direct"] };
                PassConstants& reflected{ m_PassConstants["reflected"] };

                reflected = direct;

                for (uint32_t i = 0; i < g_MaxLightCount; ++i)
                {
                    const DirectX::XMVECTOR directlightDirection{ DirectX::XMLoadFloat3(&direct.Lights[i].Direction) };
                    const DirectX::XMVECTOR reflectedLightDirection{ DirectX::XMVector3TransformNormal(directlightDirection, reflectOperator) };

                    DirectX::XMStoreFloat3(&reflected.Lights[i].Direction, reflectedLightDirection);
                }
            }

            // ConstantBuffer
            for (const auto& [name, pass] : m_PassConstants)
            {
                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(pass.View) },
                    .Projection{ DirectX::XMMatrixTranspose(pass.Projection) },
                    .CameraPosition{ pass.CameraPosition },
                    .AmbientLight{ pass.AmbientLight },
                    .FogColor{ pass.Fog.Color },
                    .ForStart{ pass.Fog.Start },
                    .FogRange{ pass.Fog.Range }
                };

                for (uint32_t i = 0; i < g_MaxLightCount; ++i)
                {
                    constants.Lights[i] = pass.Lights[i];
                }

                bufferData.Write(&constants, sizeof(constants), pass.ConstantBufferIndex * m_ConstantBuffers["pass"]->GetConfig().ElementSize);
            }
        }

        // Update RenderItems
        {
            spieler::renderer::MappedData defaultBufferData{ m_ConstantBuffers["render_item"], 0 };
            spieler::renderer::MappedData colorBufferData{ m_ConstantBuffers["color_render_item"], 0 };

            m_RenderItems["skull"]->Transform.Rotation.x += 1.0f * dt;
            m_RenderItems["skull"]->Transform.Rotation.y += 1.0f * dt;

            // ConstantBuffer
            for (const auto& [name, item] : m_RenderItems)
            {
                if (item->HasComponent<ColorConstants>())
                {
                    const cb::ColorObject constants
                    {
                        .World{ DirectX::XMMatrixTranspose(item->Transform.GetMatrix()) },
                        .Color{ item->GetComponent<ColorConstants>().Color }
                    };

                    colorBufferData.Write(&constants, sizeof(constants), item->ConstantBufferIndex * m_ConstantBuffers["color_render_item"]->GetConfig().ElementSize);
                }
                else
                {
                    cb::Object constants
                    {
                        .TextureTransform{ DirectX::XMMatrixTranspose(item->TextureTransform.GetMatrix()) }
                    };

                    if (name == "skull_shadow")
                    {
                        const DirectX::XMFLOAT3 lightDirection{ m_PassConstants["direct"].Lights[0].Direction };

                        const DirectX::XMVECTOR shadowPlane{ 0.0f, 1.0f, 0.0f, 0.0f };
                        const DirectX::XMVECTOR toLight{ -lightDirection.x, -lightDirection.y, -lightDirection.z, 0.0f };
                        const DirectX::XMMATRIX shadowOperator{ DirectX::XMMatrixShadow(shadowPlane, toLight) };
                        const DirectX::XMMATRIX translation{ DirectX::XMMatrixTranslation(0.0f, 0.001f, 0.0f) };

                        constants.World = DirectX::XMMatrixTranspose(m_RenderItems["skull"]->Transform.GetMatrix() * shadowOperator * translation);
                    }
                    else if (name == "reflected_skull")
                    {
                        const DirectX::XMMATRIX reflectOperator{ DirectX::XMMatrixReflect(m_RenderItems["mirror"]->GetComponent<MirrorConstants>().Plane) };

                        constants.World = DirectX::XMMatrixTranspose(m_RenderItems["skull"]->Transform.GetMatrix() * reflectOperator);
                    }
                    else
                    {
                        constants.World = DirectX::XMMatrixTranspose(item->Transform.GetMatrix());
                    }

                    defaultBufferData.Write(&constants, sizeof(constants), item->ConstantBufferIndex * m_ConstantBuffers["render_item"]->GetConfig().ElementSize);
                }
            }
        }

        // Update Materials
        {
            spieler::renderer::MappedData bufferData{ m_ConstantBuffers["material"], 0 };

            for (const auto& [name, material] : m_Materials)
            {
                const cb::Material constants
                {
                    .DiffuseAlbedo{ material.Constants.DiffuseAlbedo },
                    .FresnelR0{ material.Constants.FresnelR0 },
                    .Roughness{ material.Constants.Roughness },
                    .Transform{ material.Transform.GetMatrix() }
                };

                bufferData.Write(&constants, sizeof(constants), material.ConstantBufferIndex * m_ConstantBuffers["material"]->GetConfig().ElementSize);
            }
        }
    }

    void TestLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        spieler::renderer::Texture& offScreenTexture{ m_Textures["off_screen"] };
        spieler::renderer::Texture& currentBuffer{ swapChain.GetCurrentBuffer() };

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ offScreenTexture.GetTextureResource().get() },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::RenderTarget }
            });
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ m_DepthStencil.GetTextureResource().get() },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::DepthWrite }
            });

            context.SetRenderTarget(offScreenTexture.GetView<spieler::renderer::TextureRenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::TextureDepthStencilView>());

            context.ClearRenderTarget(offScreenTexture.GetView<spieler::renderer::TextureRenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.GetView<spieler::renderer::TextureDepthStencilView>(), 1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            Render(m_Lights, m_PipelineStates["light"], m_PassConstants["direct"], m_ConstantBuffers["color_render_item"].get());
            Render(m_OpaqueObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], m_ConstantBuffers["render_item"].get());

            context.SetStencilReferenceValue(1);
            Render(m_Mirrors, m_PipelineStates["mirror"], m_PassConstants["direct"], m_ConstantBuffers["render_item"].get());

            Render(m_ReflectedObjects, m_PipelineStates["reflected"], m_PassConstants["reflected"], m_ConstantBuffers["render_item"].get());

            Render(m_AlphaTestedBillboards, m_PipelineStates["billboard"], m_PassConstants["direct"]);

            context.SetStencilReferenceValue(0);
            Render(m_TransparentObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], m_ConstantBuffers["render_item"].get());

            Render(m_Shadows, m_PipelineStates["shadow"], m_PassConstants["direct"], m_ConstantBuffers["render_item"].get());

            m_BlurPass->Execute(*offScreenTexture.GetTextureResource(), m_BlurPassExecuteProps);

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ offScreenTexture.GetTextureResource().get() },
                .From{ spieler::renderer::ResourceState::CopySource },
                .To{ spieler::renderer::ResourceState::Present }
            });

            if (!m_EnableSobelFilter)
            {
                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ currentBuffer.GetTextureResource().get() },
                    .From{ spieler::renderer::ResourceState::Present },
                    .To{ spieler::renderer::ResourceState::CopyDestination }
                });

                context.GetDX12GraphicsCommandList()->CopyResource(currentBuffer.GetTextureResource()->GetDX12Resource(), m_BlurPass->GetOutput().GetTextureResource()->GetDX12Resource());

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ currentBuffer.GetTextureResource().get() },
                    .From{ spieler::renderer::ResourceState::CopyDestination },
                    .To{ spieler::renderer::ResourceState::Present }
                });
            }
            else
            {
                m_SobelFilterPass->Execute(offScreenTexture);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ currentBuffer.GetTextureResource().get() },
                    .From{ spieler::renderer::ResourceState::Present },
                    .To{ spieler::renderer::ResourceState::RenderTarget }
                });

                context.SetRenderTarget(currentBuffer.GetView<spieler::renderer::TextureRenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::TextureDepthStencilView>());

                context.ClearRenderTarget(currentBuffer.GetView<spieler::renderer::TextureRenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });

                context.SetPipelineState(m_PipelineStates["composite"]);

                context.GetDX12GraphicsCommandList()->SetGraphicsRootSignature(m_RootSignatures["composite"].GetDX12RootSignature());
                context.GetDX12GraphicsCommandList()->SetGraphicsRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE
                { 
                    m_BlurPass->GetOutput().GetView<spieler::renderer::TextureShaderResourceView>().GetDescriptor().GPU
                });

                context.GetDX12GraphicsCommandList()->SetGraphicsRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE
                { 
                    m_SobelFilterPass->GetOutputTexture().GetView<spieler::renderer::TextureShaderResourceView>().GetDescriptor().GPU
                });

                RenderFullscreenQuad();

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ currentBuffer.GetTextureResource().get() },
                    .From{ spieler::renderer::ResourceState::RenderTarget },
                    .To{ spieler::renderer::ResourceState::Present }
                });
            }

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ m_DepthStencil.GetTextureResource().get() },
                .From{ spieler::renderer::ResourceState::DepthWrite },
                .To{ spieler::renderer::ResourceState::Present }
            });
        }
    }

    void TestLayer::OnImGuiRender(float dt)
    {
        m_CameraController.OnImGuiRender(dt);
        m_DirectionalLightController.OnImGuiRender();
        
        ImGui::Begin("Position Settings");
        {
            ImGui::DragFloat3("Skull Position", reinterpret_cast<float*>(&m_RenderItems["skull"]->Transform.Translation), 0.1f);

            // Mirror
            {
                std::unique_ptr<spieler::renderer::RenderItem>& mirror{ m_RenderItems["mirror"] };
                MirrorConstants& mirrorConstants{ mirror->GetComponent<MirrorConstants>() };

                if (ImGui::DragFloat3("Mirror Position", reinterpret_cast<float*>(&mirror->Transform.Translation), 0.1f))
                {
                    mirrorConstants.Plane = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, -mirror->Transform.Translation.z };
                }

                DirectX::XMFLOAT4 plane;
                DirectX::XMStoreFloat4(&plane, mirrorConstants.Plane);

                ImGui::Text("Mirror Plane { %.2f, %.2f, %.2f, %.2f }", plane.x, plane.y, plane.z, plane.w);
            }

        }
        ImGui::End();

        ImGui::Begin("Fog");
        {
            struct PassConstants::Fog& fog{ m_PassConstants["direct"].Fog };

            ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&fog.Color));
            ImGui::SliderFloat("Start", &fog.Start, 0.1f, 100.0f);
            ImGui::SliderFloat("Range", &fog.Range, 0.1f, 300.1f);
        }
        ImGui::End();

        ImGui::Begin("Blur Settings");
        {
            ImGui::SliderFloat("Horizontal Blur Sigma", &m_BlurPassExecuteProps.HorizontalBlurSigma, 0.1f, 2.5f, "%.3f");
            ImGui::SliderFloat("Vertical Blur Sigma", &m_BlurPassExecuteProps.VerticalBlurSigma, 0.1f, 2.5f, "%.3f");
            ImGui::SliderInt("Blur Count", reinterpret_cast<int*>(&m_BlurPassExecuteProps.BlurCount), 0, 20);

            ImGui::Text(fmt::format("Horizontal Blur Radius: {}", BlurPass::GetBlurRadius(m_BlurPassExecuteProps.HorizontalBlurSigma)).c_str());
            ImGui::Text(fmt::format("Vertical Blur Radius: {}", BlurPass::GetBlurRadius(m_BlurPassExecuteProps.VerticalBlurSigma)).c_str());
        }
        ImGui::End();

        ImGui::Begin("Sober Filter Settings");
        {
            ImGui::Checkbox("Enable", &m_EnableSobelFilter);
        }
        ImGui::End();
    }

    void TestLayer::InitConstantBuffers()
    {
        using namespace magic_enum::bitwise_operators;

        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        // Pass
        {
            const spieler::renderer::BufferResource::Config config
            {
                .ElementSize{ sizeof(PassConstants) },
                .ElementCount{ 2 },
                .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
            };

            auto& pass{ m_ConstantBuffers["pass"] };
            pass = spieler::renderer::BufferResource::Create(device, config);

            // Direct
            {
                PassConstants& direct{ m_PassConstants["direct"] };
                direct.AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
                direct.Fog.Color = DirectX::XMFLOAT4{ 0.1f, 0.1f, 0.1f, 1.0f };
                direct.Fog.Start = 10.0f;
                direct.Fog.Range = 50.0f;
                direct.ConstantBufferIndex = 0;
            }

            // Reflected
            {
                PassConstants& reflected{ m_PassConstants["reflected"] };
                reflected = m_PassConstants["direct"];
                reflected.ConstantBufferIndex = 1;
            }
        }

        // Color Objects
        {
            const spieler::renderer::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::ColorObject) },
                .ElementCount{ 1 },
                .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
            };

            m_ConstantBuffers["color_render_item"] = spieler::renderer::BufferResource::Create(device, config);
        }

        // Objects
        {
            const spieler::renderer::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Object) },
                .ElementCount{ 10 },
                .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
            };

            m_ConstantBuffers["render_item"] = spieler::renderer::BufferResource::Create(device, config);
        }

        // Materials
        {
            const spieler::renderer::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Material) },
                .ElementCount{ 10 },
                .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
            };

            m_ConstantBuffers["material"] = spieler::renderer::BufferResource::Create(device, config);
        }
    }

    bool TestLayer::InitTextures()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        // White texture
        {
            auto& texture{ m_Textures["white"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/white.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }

        // Wire fence texture
        {
            auto& texture{ m_Textures["wire_fence"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/wire_fence.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }

        // Tile texture
        {
            auto& texture{ m_Textures["tile"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/tile.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }

        // Bricks texture
        {
            auto& texture{ m_Textures["bricks"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/bricks3.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }
        
        // Mirror texture
        {
            auto& texture{ m_Textures["mirror"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/ice.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }

        // Tree atlas texture
        {
            auto& texture{ m_Textures["tree_atlas"] };
            texture.SetTextureResource(spieler::renderer::TextureResource::LoadFromDDSFile(device, context, L"assets/textures/tree_array.dds"));
            texture.PushView<spieler::renderer::TextureShaderResourceView>(device);
        }

        // Off-screen texture
        {
            const spieler::renderer::TextureResource::Config config
            {
                .Width{ window.GetWidth() },
                .Height{ window.GetHeight() },
                .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
                .UsageFlags{ spieler::renderer::TextureResource::UsageFlags::RenderTarget }
            };

            const spieler::renderer::TextureResource::ClearColor clearColor
            {
                .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
            };

            spieler::renderer::Texture& offScreenTexture{ m_Textures["off_screen"] };

            offScreenTexture.SetTextureResource(spieler::renderer::TextureResource::Create(device, config, clearColor));
            offScreenTexture.PushView<spieler::renderer::TextureShaderResourceView>(device);
            offScreenTexture.PushView<spieler::renderer::TextureRenderTargetView>(device);
        }

        return true;
    }

    void TestLayer::InitMaterials()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        // Wire fence material
        {
            spieler::renderer::Material& fence{ m_Materials["wire_fence"] };
            fence.DiffuseMap = m_Textures["wire_fence"].GetView<spieler::renderer::TextureShaderResourceView>();

            fence.Constants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            fence.Constants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
            fence.Constants.Roughness = 0.2f;

            fence.ConstantBufferIndex = 1;
        }

        // Tile material
        {
            spieler::renderer::Material& tile{ m_Materials["tile"] };
            tile.DiffuseMap = m_Textures["tile"].GetView<spieler::renderer::TextureShaderResourceView>();

            tile.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tile.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            tile.Constants.Roughness = 0.8f;

            tile.ConstantBufferIndex = 2;
        }

        // Bricks material
        {
            spieler::renderer::Material& bricks{ m_Materials["bricks"] };
            bricks.DiffuseMap = m_Textures["bricks"].GetView<spieler::renderer::TextureShaderResourceView>();

            bricks.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            bricks.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            bricks.Constants.Roughness = 0.8f;

            bricks.ConstantBufferIndex = 3;
        }

        // Skull material
        {
            spieler::renderer::Material& skull{ m_Materials["skull"] };
            skull.DiffuseMap = m_Textures["white"].GetView<spieler::renderer::TextureShaderResourceView>();

            skull.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            skull.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            skull.Constants.Roughness = 0.3f;

            skull.ConstantBufferIndex = 4;
        }

        // Mirror material
        {
            spieler::renderer::Material& mirror{ m_Materials["mirror"] };
            mirror.DiffuseMap = m_Textures["mirror"].GetView<spieler::renderer::TextureShaderResourceView>();

            mirror.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f };
            mirror.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };
            mirror.Constants.Roughness = 0.5f;

            mirror.ConstantBufferIndex = 5;
        }

        // Shadow material
        {
            spieler::renderer::Material& shadow{ m_Materials["shadow"] };
            shadow.DiffuseMap = m_Textures["white"].GetView<spieler::renderer::TextureShaderResourceView>();

            shadow.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.5f };
            shadow.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.001f, 0.001f, 0.001f };
            shadow.Constants.Roughness = 0.0f;

            shadow.ConstantBufferIndex = 6;
        }

        // Tree material
        {
            spieler::renderer::Material& tree{ m_Materials["tree"] };
            tree.DiffuseMap = m_Textures["tree_atlas"].GetView<spieler::renderer::TextureShaderResourceView>();

            tree.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tree.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.01f, 0.01f, 0.01f };
            tree.Constants.Roughness = 0.125f;

            tree.ConstantBufferIndex = 7;
        }
    }

    bool TestLayer::InitMeshGeometries()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        // Basic geometry
        {
            const spieler::renderer::BoxGeometryProps boxGeometryProps
            {
                .Width{ 10.0f },
                .Height{ 10.0f },
                .Depth{ 10.0f }
            };

            const spieler::renderer::GeosphereGeometryProps geosphereGeometryProps
            {
                .Radius{ 10.0f },
                .SubdivisionCount{ 2 }
            };

            const spieler::renderer::GridGeometryProps gridGeometryProps
            {
                .Width{ 1.0f },
                .Depth{ 1.0f },
                .RowCount{ 2 },
                .ColumnCount{ 2 }
            };

            const std::unordered_map<std::string, spieler::renderer::MeshData<uint32_t>> meshes
            {
                { "box", spieler::renderer::GeometryGenerator::GenerateBox<uint32_t>(boxGeometryProps) },
                { "geosphere", spieler::renderer::GeometryGenerator::GenerateGeosphere<uint32_t>(geosphereGeometryProps) },
                { "grid", spieler::renderer::GeometryGenerator::GenerateGrid<uint32_t>(gridGeometryProps) }
            };

            size_t vertexCount{ 0 };
            size_t indexCount{ 0 };

            for (const auto& [name, mesh] : meshes)
            {
                m_MeshGeometries["basic"].Submeshes[name] = spieler::renderer::SubmeshGeometry
                {
                    .IndexCount{ static_cast<uint32_t>(mesh.Indices.size()) },
                    .BaseVertexLocation{ static_cast<uint32_t>(vertexCount) },
                    .StartIndexLocation{ static_cast<uint32_t>(indexCount) }
                };

                vertexCount += mesh.Vertices.size();
                indexCount += mesh.Indices.size();
            }

            std::vector<spieler::renderer::Vertex> vertices;
            vertices.reserve(vertexCount);

            std::vector<uint32_t> indices;
            indices.reserve(indexCount);

            for (const auto& [name, mesh] : meshes)
            {
                vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
                indices.insert(indices.end(), mesh.Indices.begin(), mesh.Indices.end());
            }

            // VertexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(spieler::renderer::Vertex) },
                    .ElementCount{ static_cast<uint32_t>(vertexCount) }
                };

                m_MeshGeometries["basic"].VertexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*m_MeshGeometries["basic"].VertexBuffer.GetBufferResource(), vertices.data(), sizeof(spieler::renderer::Vertex) * vertexCount);
            }

            // IndexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(uint32_t) },
                    .ElementCount{ static_cast<uint32_t>(indexCount) }
                };

                m_MeshGeometries["basic"].IndexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*m_MeshGeometries["basic"].IndexBuffer.GetBufferResource(), indices.data(), sizeof(uint32_t) * indexCount);
            }
        }

        // Load skull model
        {
            std::ifstream fin("assets/models/skull.txt");

            SPIELER_ASSERT(fin);

            uint32_t vcount = 0;
            uint32_t tcount = 0;
            std::string ignore;

            fin >> ignore >> vcount;
            fin >> ignore >> tcount;
            fin >> ignore >> ignore >> ignore >> ignore;

            std::vector<spieler::renderer::Vertex> vertices(vcount);
            for (UINT i = 0; i < vcount; ++i)
            {
                fin >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
                fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

                // Model does not have texture coordinates, so just zero them out.
                vertices[i].TexCoord = { 0.0f, 0.0f };
            }

            fin >> ignore;
            fin >> ignore;
            fin >> ignore;

            std::vector<uint32_t> indices(3 * tcount);
            for (uint32_t i = 0; i < tcount; ++i)
            {
                fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
            }

            fin.close();

            spieler::renderer::MeshGeometry& skullMeshGeometry{ m_MeshGeometries["skull"] };

            // VertexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(spieler::renderer::Vertex) },
                    .ElementCount{ static_cast<uint32_t>(vertices.size()) }
                };

                skullMeshGeometry.VertexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*skullMeshGeometry.VertexBuffer.GetBufferResource(), vertices.data(), sizeof(spieler::renderer::Vertex) * vertices.size());
            }

            // IndexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(uint32_t) },
                    .ElementCount{ static_cast<uint32_t>(indices.size()) }
                };

                skullMeshGeometry.IndexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*skullMeshGeometry.IndexBuffer.GetBufferResource(), indices.data(), sizeof(uint32_t) * indices.size());
            }

            skullMeshGeometry.Submeshes["grid"] = spieler::renderer::SubmeshGeometry
            {
                .IndexCount{ static_cast<uint32_t>(indices.size()) },
                .BaseVertexLocation{ 0 },
                .StartIndexLocation{ 0 }
            };
        }

        // Trees (billboards)
        {
            static constexpr uint32_t treeCount{ 10 };

            struct TreeVertex
            {
                DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
                DirectX::XMFLOAT2 Size{ 0.0f, 0.0f };
            };

            // Vertices
            std::array<TreeVertex, treeCount> vertices;

            for (TreeVertex& vertex : vertices)
            {
                const float x{ spieler::Random::GetInstance().GetFloatingPoint(-10.0f, 10.0f) };
                const float z{ spieler::Random::GetInstance().GetFloatingPoint(-10.0f, 10.0f) };
                const float size{ spieler::Random::GetInstance().GetFloatingPoint(3.0f, 10.0f) };

                vertex.Position = DirectX::XMFLOAT3{ x, size / 2.0f - 0.4f, z };
                vertex.Size = DirectX::XMFLOAT2{ size, size };
            }

            // Indices
            std::array<uint32_t, treeCount> indices;
            std::iota(indices.begin(), indices.end(), 0);

            spieler::renderer::MeshGeometry& treeMeshGeometry{ m_MeshGeometries["trees"] };

            // VertexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(TreeVertex) },
                    .ElementCount{ static_cast<uint32_t>(vertices.size()) }
                };

                treeMeshGeometry.VertexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*treeMeshGeometry.VertexBuffer.GetBufferResource(), vertices.data(), sizeof(TreeVertex) * vertices.size());
            }

            // IndexBuffer
            {
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(uint32_t) },
                    .ElementCount{ static_cast<uint32_t>(indices.size()) }
                };

                treeMeshGeometry.IndexBuffer.SetBufferResource(spieler::renderer::BufferResource::Create(device, config));
                context.UploadToBuffer(*treeMeshGeometry.IndexBuffer.GetBufferResource(), indices.data(), sizeof(uint32_t) * indices.size());
            }

            treeMeshGeometry.Submeshes["main"] = spieler::renderer::SubmeshGeometry
            {
                .IndexCount{ static_cast<uint32_t>(indices.size()) },
                .BaseVertexLocation{ 0 },
                .StartIndexLocation{ 0 }
            };
        }

        return true;
    }

    bool TestLayer::InitRootSignatures()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        // Default root signature
        {
            spieler::renderer::RootSignature::Config config{ 4, 6 };

            // Init root parameters
            {
                config.RootParameters[0] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                config.RootParameters[1] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 1 }
                };

                config.RootParameters[2] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 2 }
                };

                config.RootParameters[3] = spieler::renderer::RootParameter::SingleDescriptorTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 1 },
                        .BaseShaderRegister{ 0 }
                    }
                };
            }

            // Init statis samplers
            {
                config.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Wrap, 0 };
                config.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Clamp, 1 };
                config.StaticSamplers[2] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 2 };
                config.StaticSamplers[3] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Clamp, 3 };
                config.StaticSamplers[4] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 4 };
                config.StaticSamplers[5] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Clamp, 5 };
            }

            m_RootSignatures["default"] = spieler::renderer::RootSignature{ device, config };
        }

        // Composite root signature
        {
            spieler::renderer::RootSignature::Config config{ 2, 6 };

            // Init root parameters
            {
                config.RootParameters[0] = spieler::renderer::RootParameter::SingleDescriptorTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 1 },
                        .BaseShaderRegister{ 0 }
                    }
                };

                config.RootParameters[1] = spieler::renderer::RootParameter::SingleDescriptorTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 1 },
                        .BaseShaderRegister{ 1 }
                    }
                };
            }

            // Init statis samplers
            {
                config.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Wrap, 0 };
                config.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Clamp, 1 };
                config.StaticSamplers[2] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 2 };
                config.StaticSamplers[3] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Clamp, 3 };
                config.StaticSamplers[4] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 4 };
                config.StaticSamplers[5] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Clamp, 5 };
            }

            m_RootSignatures["composite"] = spieler::renderer::RootSignature{ device, config };
        }

        return true;
    }

    bool TestLayer::InitPipelineStates()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };

        const spieler::renderer::InputLayout inputLayout
        {
            spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "Normal", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "Tangent", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "TexCoord", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
        };

        // PSO for lights
        {
            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderPermutation<per::Color>{}) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderPermutation<per::Color>{}) };

            const spieler::renderer::RasterizerState rasterzerState
            {
                .FillMode{ spieler::renderer::FillMode::Wireframe },
                .CullMode{ spieler::renderer::CullMode::None }
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature{ &m_RootSignatures["default"] },
                .VertexShader{ &vertexShader },
                .PixelShader{ &pixelShader },
                .RasterizerState{ &rasterzerState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                .RTVFormat{ renderer.GetSwapChain().GetBufferFormat() },
                .DSVFormat{ m_DepthStencilFormat },
            };

            m_PipelineStates["light"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for opaque objects
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode{ spieler::renderer::FillMode::Solid },
                .CullMode{ spieler::renderer::CullMode::None },
            };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates
                {
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .State{ spieler::renderer::BlendState::RenderTargetState::State::Enabled },
                        .ColorEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::ColorFactor::SourceAlpha },
                            .DestinationFactor{ spieler::renderer::BlendState::ColorFactor::InverseSourceAlpha },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .AlphaEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::AlphaFactor::One },
                            .DestinationFactor{ spieler::renderer::BlendState::AlphaFactor::Zero },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .Channels{ spieler::renderer::BlendState::Channels::All }
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat,
            };

            m_PipelineStates["opaque"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for marking mirros
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates 
                { 
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .Channels{ spieler::renderer::BlendState::Channels::None }
                    }
                }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode{ spieler::renderer::FillMode::Solid },
                .CullMode{ spieler::renderer::CullMode::None },
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                    .WriteState{ spieler::renderer::DepthState::WriteState::Disabled },
                    .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ spieler::renderer::StencilState::TestState::Enabled },
                    .ReadMask{ 0xff },
                    .WriteMask{ 0xff },
                    .FrontFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Replace },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Always }
                    },
                    .BackFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Replace },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Always }
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat,
            };

            m_PipelineStates["mirror"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for stencil reflections
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates
                {
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .State{ spieler::renderer::BlendState::RenderTargetState::State::Enabled },
                        .ColorEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::ColorFactor::SourceAlpha },
                            .DestinationFactor{ spieler::renderer::BlendState::ColorFactor::InverseSourceAlpha },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .AlphaEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::AlphaFactor::One },
                            .DestinationFactor{ spieler::renderer::BlendState::AlphaFactor::Zero },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .Channels{ spieler::renderer::BlendState::Channels::All }
                    }
                }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
                .TriangleOrder = spieler::renderer::TriangleOrder::Counterclockwise
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                    .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ spieler::renderer::StencilState::TestState::Enabled },
                    .ReadMask{ 0xff },
                    .WriteMask{ 0xff },
                    .FrontFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Equal }
                    },
                    .BackFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Equal }
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat,
            };

            m_PipelineStates["reflected"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for planar shadows
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates
                {
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .State{ spieler::renderer::BlendState::RenderTargetState::State::Enabled },
                        .ColorEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::ColorFactor::SourceAlpha },
                            .DestinationFactor{ spieler::renderer::BlendState::ColorFactor::InverseSourceAlpha },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .AlphaEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::AlphaFactor::One },
                            .DestinationFactor{ spieler::renderer::BlendState::AlphaFactor::Zero },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .Channels{ spieler::renderer::BlendState::Channels::All }
                    }
                }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                    .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ spieler::renderer::StencilState::TestState::Enabled },
                    .ReadMask{ 0xff },
                    .WriteMask{ 0xff },
                    .FrontFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Increase },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Equal }
                    },
                    .BackFaceBehaviour
                    {
                        .StencilFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .DepthFailOperation{ spieler::renderer::StencilState::Operation::Keep },
                        .PassOperation{ spieler::renderer::StencilState::Operation::Increase },
                        .StencilFunction{ spieler::renderer::ComparisonFunction::Equal }
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat,
            };

            m_PipelineStates["shadow"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for billboard technique
        {
            spieler::renderer::ShaderPermutation<per::Billboard> permutation;
            permutation.Set<per::Billboard::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Billboard::USE_ALPHA_TEST>(true);
            permutation.Set<per::Billboard::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };
            const spieler::renderer::Shader& geometryShader{ GetShader(spieler::renderer::ShaderType::Geometry, permutation) };

            // Blend state
            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates
                {
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .State{ spieler::renderer::BlendState::RenderTargetState::State::Enabled },
                        .ColorEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::ColorFactor::SourceAlpha },
                            .DestinationFactor{ spieler::renderer::BlendState::ColorFactor::InverseSourceAlpha },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .AlphaEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::AlphaFactor::One },
                            .DestinationFactor{ spieler::renderer::BlendState::AlphaFactor::Zero },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .Channels{ spieler::renderer::BlendState::Channels::All }
                    }
                }
            };

            // Rasterizer state
            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode{ spieler::renderer::FillMode::Solid },
                .CullMode{ spieler::renderer::CullMode::None },
            };

            const spieler::renderer::InputLayout inputLayout
            {
                spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Size", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) },
            };

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature{ &m_RootSignatures["default"] },
                .VertexShader{ &vertexShader },
                .GeometryShader{ &geometryShader },
                .PixelShader{ &pixelShader },
                .BlendState{ &blendState },
                .RasterizerState{ &rasterizerState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Point },
                .RTVFormat{ renderer.GetSwapChain().GetBufferFormat() },
                .DSVFormat{ m_DepthStencilFormat }
            };

            m_PipelineStates["billboard"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        // PSO for composite pass
        {
            const spieler::renderer::ShaderPermutation<per::Composite> permutation;
            const spieler::renderer::Shader& vertexShader{ GetShader(spieler::renderer::ShaderType::Vertex, permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader(spieler::renderer::ShaderType::Pixel, permutation) };

            // Blend state
            const spieler::renderer::BlendState blendState
            {
                .RenderTargetStates
                {
                    spieler::renderer::BlendState::RenderTargetState
                    {
                        .State{ spieler::renderer::BlendState::RenderTargetState::State::Enabled },
                        .ColorEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::ColorFactor::SourceAlpha },
                            .DestinationFactor{ spieler::renderer::BlendState::ColorFactor::InverseSourceAlpha },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .AlphaEquation
                        {
                            .SourceFactor{ spieler::renderer::BlendState::AlphaFactor::One },
                            .DestinationFactor{ spieler::renderer::BlendState::AlphaFactor::Zero },
                            .Operation{ spieler::renderer::BlendState::Operation::Add }
                        },
                        .Channels{ spieler::renderer::BlendState::Channels::All }
                    }
                }
            };

            // Rasterizer state
            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode{ spieler::renderer::FillMode::Solid },
                .CullMode{ spieler::renderer::CullMode::None },
            };

            const spieler::renderer::InputLayout inputLayout;

            const spieler::renderer::GraphicsPipelineState::Config pipelineStateConfig
            {
                .RootSignature{ &m_RootSignatures["composite"] },
                .VertexShader{ &vertexShader },
                .PixelShader{ &pixelShader },
                .BlendState{ &blendState },
                .RasterizerState{ &rasterizerState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                .RTVFormat{ renderer.GetSwapChain().GetBufferFormat() },
                .DSVFormat{ m_DepthStencilFormat }
            };

            m_PipelineStates["composite"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        return true;
    }

    void TestLayer::InitRenderItems()
    {
        spieler::renderer::MeshGeometry& basicMeshGeometry{ m_MeshGeometries["basic"] };

        // Box
        {
            std::unique_ptr<spieler::renderer::RenderItem>& box{ m_RenderItems["box"] = std::make_unique<spieler::renderer::RenderItem>() };
            box->MeshGeometry = &basicMeshGeometry;
            box->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("box");
            box->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            box->Material = &m_Materials["wire_fence"];

            box->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 20.0f };

            box->ConstantBufferIndex = 0;

            m_OpaqueObjects.push_back(box.get());
        }

        // Sun
        {
            std::unique_ptr<spieler::renderer::RenderItem>& sun{ m_RenderItems["sun"] = std::make_unique<spieler::renderer::RenderItem>() };
            sun->MeshGeometry = &basicMeshGeometry;
            sun->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("geosphere");
            sun->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

            sun->GetComponent<ColorConstants>().Color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 0.2f, 1.0f };

            sun->ConstantBufferIndex = 0;

            m_Lights.push_back(sun.get());
        }

        // Floor
        {
            std::unique_ptr<spieler::renderer::RenderItem>& floor{ m_RenderItems["floor"] = std::make_unique<spieler::renderer::RenderItem>() };
            floor->MeshGeometry = &basicMeshGeometry;
            floor->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("grid");
            floor->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            floor->Material = &m_Materials["tile"];

            floor->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 20.0f };
            floor->Transform.Rotation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            floor->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };

            floor->TextureTransform.Scale = DirectX::XMFLOAT3{ 10.0f, 10.0f, 1.0f };

            floor->ConstantBufferIndex = 1;

            m_OpaqueObjects.push_back(floor.get());
        }

        // Wall 1
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall1"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ 7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            wall->ConstantBufferIndex = 2;

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 2
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall2"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ DirectX::XM_PIDIV2, DirectX::XM_PIDIV2, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -10.0f, 5.0f, 0.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 4.0f, 2.0f, 1.0f };

            wall->ConstantBufferIndex = 3;

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 3
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall3"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            wall->ConstantBufferIndex = 4;

            m_OpaqueObjects.push_back(wall.get());
        }

        // Skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].Submeshes.at("grid");
            skull->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            skull->Material = &m_Materials["skull"];

            skull->Transform.Scale = DirectX::XMFLOAT3{ 0.5f, 0.5f, 0.5f };
            skull->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 6.0f, 0.0f };

            skull->ConstantBufferIndex = 5;

            m_OpaqueObjects.push_back(skull.get());
        }

        // Reflected skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["reflected_skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].Submeshes.at("grid");
            skull->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            skull->Material = &m_Materials["skull"];

            skull->ConstantBufferIndex = 6;

            m_ReflectedObjects.push_back(skull.get());
        }

        // Mirror
        {
            std::unique_ptr<spieler::renderer::RenderItem>& mirror{ m_RenderItems["mirror"] = std::make_unique<spieler::renderer::RenderItem>() };
            mirror->MeshGeometry = &basicMeshGeometry;
            mirror->SubmeshGeometry = &basicMeshGeometry.Submeshes.at("grid");
            mirror->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            mirror->Material = &m_Materials["mirror"];

            mirror->Transform.Scale = DirectX::XMFLOAT3{ 10.0f, 1.0f, 10.0f };
            mirror->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            mirror->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 5.0f, 10.0f };

            mirror->GetComponent<MirrorConstants>().Plane = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, -10.0f };

            mirror->ConstantBufferIndex = 7;

            m_Mirrors.push_back(mirror.get());
            m_TransparentObjects.push_back(mirror.get());
        }

        // Skull shadow
        {
            std::unique_ptr<spieler::renderer::RenderItem>& shadow{ m_RenderItems["skull_shadow"] = std::make_unique<spieler::renderer::RenderItem>() };
            shadow->MeshGeometry = &m_MeshGeometries["skull"];
            shadow->SubmeshGeometry = &m_MeshGeometries["skull"].Submeshes.at("grid");
            shadow->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            shadow->Material = &m_Materials["shadow"];

            shadow->ConstantBufferIndex = 8;

            m_Shadows.push_back(shadow.get());
        }

        // Billboards
        {
            std::unique_ptr<spieler::renderer::RenderItem>& trees{ m_RenderItems["trees"] = std::make_unique<spieler::renderer::RenderItem>() };
            trees->MeshGeometry = &m_MeshGeometries["trees"];
            trees->SubmeshGeometry = &m_MeshGeometries["trees"].Submeshes.at("main");
            trees->PrimitiveTopology = spieler::renderer::PrimitiveTopology::PointList;
            trees->Material = &m_Materials["tree"];

            m_AlphaTestedBillboards.push_back(trees.get());
        }
    }

    void TestLayer::InitLights()
    {
        spieler::renderer::LightConstants constants;
        constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };

        m_DirectionalLightController.SetConstants(&m_PassConstants["direct"].Lights[0]);
        m_DirectionalLightController.SetShape(m_RenderItems["sun"].get());
        m_DirectionalLightController.Init(constants, DirectX::XM_PIDIV2 * 1.25f, DirectX::XM_PIDIV4);
    }

    void TestLayer::InitDepthStencil()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::TextureResource::Config config
        {
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Format{ m_DepthStencilFormat },
            .UsageFlags{ spieler::renderer::TextureResource::UsageFlags::DepthStencil }
        };

        const spieler::renderer::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        m_DepthStencil.SetTextureResource(spieler::renderer::TextureResource::Create(device, config, clearDepthStencil));
        m_DepthStencil.PushView<spieler::renderer::TextureDepthStencilView>(device);
    }

    void TestLayer::UpdateViewport()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(window.GetWidth());
        m_Viewport.Height = static_cast<float>(window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TestLayer::UpdateScissorRect()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(window.GetHeight());
    }

    void TestLayer::RenderFullscreenQuad()
    {
        auto& context{ spieler::renderer::Renderer::GetInstance().GetContext() };

        context.IASetVertexBuffer(nullptr);
        context.IASetIndexBuffer(nullptr);
        context.IASetPrimitiveTopology(spieler::renderer::PrimitiveTopology::TriangleList);

        context.GetDX12GraphicsCommandList()->DrawInstanced(6, 1, 0, 0);
    }

    void TestLayer::Render(
        std::vector<const spieler::renderer::RenderItem*> items,
        const spieler::renderer::PipelineState& pso,
        const PassConstants& passConstants,
        const spieler::renderer::BufferResource* objectConstantBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };
        auto& descriptorManager{ device.GetDescriptorManager() };

        context.SetPipelineState(pso);
        context.SetGraphicsRootSignature(m_RootSignatures["default"]);

        context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
            0, 
            m_ConstantBuffers["pass"]->GetGPUVirtualAddress() + passConstants.ConstantBufferIndex * m_ConstantBuffers["pass"]->GetConfig().ElementSize
        );

        for (const spieler::renderer::RenderItem* item : items)
        {
            context.IASetVertexBuffer(item->MeshGeometry->VertexBuffer.GetBufferResource().get());
            context.IASetIndexBuffer(item->MeshGeometry->IndexBuffer.GetBufferResource().get());
            context.IASetPrimitiveTopology(item->PrimitiveTopology);

            if (objectConstantBuffer)
            {
                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    1,
                    objectConstantBuffer->GetGPUVirtualAddress() + item->ConstantBufferIndex * objectConstantBuffer->GetConfig().ElementSize
                );
            }

            // Material
            if (item->Material)
            {
                context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::renderer::DescriptorHeap::Type::SRV));

                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    2,
                    m_ConstantBuffers["material"]->GetGPUVirtualAddress() + item->Material->ConstantBufferIndex * m_ConstantBuffers["material"]->GetConfig().ElementSize
                );

                context.GetDX12GraphicsCommandList()->SetGraphicsRootDescriptorTable(3, D3D12_GPU_DESCRIPTOR_HANDLE{ item->Material->DiffuseMap.GetDescriptor().GPU });
            }

            const spieler::renderer::SubmeshGeometry& submeshGeometry{ *item->SubmeshGeometry };
            context.GetDX12GraphicsCommandList()->DrawIndexedInstanced(
                submeshGeometry.IndexCount,
                1,
                submeshGeometry.StartIndexLocation,
                submeshGeometry.BaseVertexLocation,
                0
            );
        }
    }

    bool TestLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        UpdateViewport();
        UpdateScissorRect();

        InitDepthStencil();

        m_BlurPass->OnResize(event.GetWidth(), event.GetHeight());
        m_SobelFilterPass->OnResize(event.GetWidth(), event.GetHeight());

        // Off-screen texture
        {
            auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

            const spieler::renderer::TextureResource::Config config
            {
                .Width{ event.GetWidth() },
                .Height{ event.GetHeight() },
                .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
                .UsageFlags{ spieler::renderer::TextureResource::UsageFlags::RenderTarget }
            };

            const spieler::renderer::TextureResource::ClearColor clearColor
            {
                .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
            };

            spieler::renderer::Texture& offScreenTexture{ m_Textures["off_screen"] };

            offScreenTexture.SetTextureResource(spieler::renderer::TextureResource::Create(device, config, clearColor));
            offScreenTexture.PushView<spieler::renderer::TextureShaderResourceView>(device);
            offScreenTexture.PushView<spieler::renderer::TextureRenderTargetView>(device);
        }

        return false;
    }

} // namespace sandbox

#endif
