#include "bootstrap.hpp"

#include "test_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/core/logger.hpp>

#include <spieler/system/event.hpp>
#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/blend_state.hpp>
#include <spieler/renderer/rasterizer_state.hpp>
#include <spieler/renderer/resource_barrier.hpp>
#include <spieler/renderer/depth_stencil_state.hpp>

#include <spieler/math/matrix.hpp>

#include <spieler/utility/random.hpp>

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
            alignas(16) spieler::renderer::LightConstants Lights[MAX_LIGHT_COUNT];

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

    TestLayer::TestLayer(spieler::Window& window)
        : m_Window{ window }
        , m_CameraController{ spieler::math::ToRadians(60.0f), m_Window.GetAspectRatio() }
        , m_BlurPass{ window.GetWidth(), window.GetHeight() }
        , m_SobelFilterPass{ window.GetWidth(), window.GetHeight() }
    {}

    bool TestLayer::OnAttach()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        spieler::renderer::UploadBuffer uploadBuffer(device, spieler::renderer::UploadBufferType::Default, spieler::MB(6));

        UpdateViewport();
        UpdateScissorRect();

        InitConstantBuffers();

        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures(uploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitMeshGeometries(uploadBuffer));
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));

        InitMaterials();
        SPIELER_RETURN_IF_FAILED(InitRootSignatures());
        SPIELER_RETURN_IF_FAILED(InitPipelineStates());

        InitRenderItems();
        InitLights();

        InitDepthStencil();

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

        const ProjectionCamera& camera{ m_CameraController.GetCamera() };
        
        // Update Pass Constants
        {
            const ProjectionCamera& camera{ m_CameraController.GetCamera() };

            m_PassConstants["direct"].View = camera.View;
            m_PassConstants["direct"].Projection = camera.Projection;
            DirectX::XMStoreFloat3(&m_PassConstants["direct"].CameraPosition, camera.EyePosition);

            // Reflect light
            {
                const DirectX::XMMATRIX reflectOperator{ DirectX::XMMatrixReflect(m_RenderItems["mirror"]->GetComponent<MirrorConstants>().Plane) };

                PassConstants& direct{ m_PassConstants["direct"] };
                PassConstants& reflected{ m_PassConstants["reflected"] };

                reflected = direct;

                for (uint32_t i = 0; i < MAX_LIGHT_COUNT; ++i)
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
                    .View = DirectX::XMMatrixTranspose(pass.View),
                    .Projection = DirectX::XMMatrixTranspose(pass.Projection),
                    .CameraPosition = pass.CameraPosition,
                    .AmbientLight = pass.AmbientLight,
                    .FogColor = pass.Fog.Color,
                    .ForStart = pass.Fog.Start,
                    .FogRange = pass.Fog.Range
                };

                for (uint32_t i = 0; i < MAX_LIGHT_COUNT; ++i)
                {
                    constants.Lights[i] = pass.Lights[i];
                }

                m_ConstantBuffers["pass"].GetSlice(&pass).Update(&constants, sizeof(constants) );
            }
        }

        // Update RenderItems
        {
            m_RenderItems["skull"]->Transform.Rotation.x += 1.0f * dt;
            m_RenderItems["skull"]->Transform.Rotation.y += 1.0f * dt;

            // ConstantBuffer
            for (const auto& [name, item] : m_RenderItems)
            {
                if (item->HasComponent<ColorConstants>())
                {
                    const cb::ColorObject constants
                    {
                        .World = DirectX::XMMatrixTranspose(item->Transform.GetMatrix()),
                        .Color = item->GetComponent<ColorConstants>().Color
                    };

                    m_ConstantBuffers["color_render_item"].GetSlice(item.get()).Update(&constants, sizeof(constants));
                }
                else
                {
                    cb::Object constants
                    {
                        .TextureTransform = DirectX::XMMatrixTranspose(item->TextureTransform.GetMatrix())
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

                    if (auto& constantBuffer{ m_ConstantBuffers["render_item"] }; constantBuffer.HasSlice(item.get()))
                    {
                        constantBuffer.GetSlice(item.get()).Update(&constants, sizeof(constants));
                    }
                }
            }
        }

        // Update Materials
        {
            for (const auto& [name, material] : m_Materials)
            {
                const cb::Material constants
                {
                    .DiffuseAlbedo = material.Constants.DiffuseAlbedo,
                    .FresnelR0 = material.Constants.FresnelR0,
                    .Roughness = material.Constants.Roughness,
                    .Transform = material.Transform.GetMatrix()
                };

                m_ConstantBuffers["material"].GetSlice(&material).Update(&constants, sizeof(constants));
            }
        }
    }

    void TestLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto& nativeCommandList{ context.GetNativeCommandList() };

        spieler::renderer::Texture2D& offScreenTexture{ m_Textures["off_screen"] };

        spieler::renderer::Texture2D& currentBuffer{ swapChain.GetCurrentBuffer() };
        spieler::renderer::Texture2DResource& currentBufferResource{ currentBuffer.GetTexture2DResource() };

        SPIELER_ASSERT(context.ResetCommandList());
        {
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &offScreenTexture.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::RenderTarget
            });
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &m_DepthStencil.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::DepthWrite
            });

            context.SetRenderTarget(offScreenTexture.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(offScreenTexture.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            Render(m_Lights, m_PipelineStates["light"], m_PassConstants["direct"], &m_ConstantBuffers["color_render_item"]);
            Render(m_OpaqueObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            context.SetStencilReferenceValue(1);
            Render(m_Mirrors, m_PipelineStates["mirror"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            Render(m_ReflectedObjects, m_PipelineStates["reflected"], m_PassConstants["reflected"], &m_ConstantBuffers["render_item"]);

            Render(m_AlphaTestedBillboards, m_PipelineStates["billboard"], m_PassConstants["direct"]);

            context.SetStencilReferenceValue(0);
            Render(m_TransparentObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            Render(m_Shadows, m_PipelineStates["shadow"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            m_BlurPass.Execute(offScreenTexture.GetTexture2DResource(), m_BlurPassExecuteProps);

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &offScreenTexture.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::CopySource,
                .To = spieler::renderer::ResourceState::Present
            });

            if (!m_EnableSobelFilter)
            {
                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &currentBufferResource,
                    .From = spieler::renderer::ResourceState::Present,
                    .To = spieler::renderer::ResourceState::CopyDestination
                });

                nativeCommandList->CopyResource(currentBufferResource.GetResource().Get(), m_BlurPass.GetOutput().GetTexture2DResource().GetResource().Get());

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &currentBufferResource,
                    .From = spieler::renderer::ResourceState::CopyDestination,
                    .To = spieler::renderer::ResourceState::Present
                });
            }
            else
            {
                m_SobelFilterPass.Execute(offScreenTexture);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &currentBufferResource,
                    .From = spieler::renderer::ResourceState::Present,
                    .To = spieler::renderer::ResourceState::RenderTarget
                });

                context.SetRenderTarget(currentBuffer.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::DepthStencilView>());

                context.ClearRenderTarget(currentBuffer.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });

                context.SetPipelineState(m_PipelineStates["composite"]);
                nativeCommandList->SetGraphicsRootSignature(static_cast<ID3D12RootSignature*>(m_RootSignatures["composite"]));
                nativeCommandList->SetGraphicsRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurPass.GetOutput().GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

                nativeCommandList->SetGraphicsRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_SobelFilterPass.GetOutputTexture().GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

                RenderFullscreenQuad();

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &currentBufferResource,
                    .From = spieler::renderer::ResourceState::RenderTarget,
                    .To = spieler::renderer::ResourceState::Present
                });
            }

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &m_DepthStencil.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::DepthWrite,
                .To = spieler::renderer::ResourceState::Present
            });
        }
        SPIELER_ASSERT(context.ExecuteCommandList(true));
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

        const spieler::renderer::BufferFlags constantBufferFlags{ spieler::renderer::BufferFlags::Dynamic | spieler::renderer::BufferFlags::ConstantBuffer };

        // Pass
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(PassConstants),
                .ElementCount = 2
            };

            spieler::renderer::ConstantBuffer& pass{ m_ConstantBuffers["pass"] };

            pass.SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));

            // Direct
            {
                PassConstants& direct{ m_PassConstants["direct"] };
                direct.AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
                direct.Fog.Color = DirectX::XMFLOAT4{ 0.1f, 0.1f, 0.1f, 1.0f };
                direct.Fog.Start = 10.0f;
                direct.Fog.Range = 50.0f;

                pass.SetSlice(&direct);
            }

            // Reflected
            {
                PassConstants& reflected{ m_PassConstants["reflected"] };
                reflected = m_PassConstants["direct"];

                pass.SetSlice(&reflected);
            }
        }

        // Color Objects
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::ColorObject),
                .ElementCount = 1
            };

            m_ConstantBuffers["color_render_item"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }

        // Objects
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::Object),
                .ElementCount = 10
            };

            m_ConstantBuffers["render_item"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }

        // Materials
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::Material),
                .ElementCount = 10
            };

            m_ConstantBuffers["material"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }
    }

    bool TestLayer::InitTextures(spieler::renderer::UploadBuffer& uploadBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        // White texture
        {
            spieler::renderer::Texture2D& whiteTexture{ m_Textures["white"] };

            SPIELER_ASSERT(whiteTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/white.dds", uploadBuffer));
            whiteTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Wood crate texture
        {
            spieler::renderer::Texture2D& woodCrateTexture{ m_Textures["wood_crate"] };

            SPIELER_ASSERT(woodCrateTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/wood_crate1.dds", uploadBuffer));
            woodCrateTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Wire fence texture
        {
            spieler::renderer::Texture2D& wireFenceTexture{ m_Textures["wire_fence"] };

            SPIELER_ASSERT(wireFenceTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/wire_fence.dds", uploadBuffer));
            wireFenceTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Tile texture
        {
            spieler::renderer::Texture2D& tileTexture{ m_Textures["tile"] };

            SPIELER_ASSERT(tileTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/tile.dds", uploadBuffer));
            tileTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Bricks texture
        {
            spieler::renderer::Texture2D& bricksTexture{ m_Textures["bricks"] };

            SPIELER_ASSERT(bricksTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/bricks3.dds", uploadBuffer));
            bricksTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }
        
        // Mirror texture
        {
            spieler::renderer::Texture2D& mirrorTexture{ m_Textures["mirror"] };

            SPIELER_ASSERT(mirrorTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/ice.dds", uploadBuffer));
            mirrorTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Tree atlas texture
        {
            spieler::renderer::Texture2D& treeAtlasTexture{ m_Textures["tree_atlas"] };

            SPIELER_ASSERT(treeAtlasTexture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/tree_array.dds", uploadBuffer));
            treeAtlasTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        }

        // Off-screen texture
        {
            const spieler::renderer::Texture2DConfig offScreenTextureConfig
            {
                .Width{ static_cast<uint64_t>(m_Window.GetWidth()) },
                .Height{ m_Window.GetHeight() },
                .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
                .Flags{ spieler::renderer::Texture2DFlags::RenderTarget }
            };

            const spieler::renderer::TextureClearValue offScreenTextureClearValue
            {
                .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
            };

            spieler::renderer::Texture2D& offScreenTexture{ m_Textures["off_screen"] };

            SPIELER_ASSERT(device.CreateTexture(offScreenTextureConfig, offScreenTextureClearValue, offScreenTexture.GetTexture2DResource()));

            offScreenTexture.SetView<spieler::renderer::ShaderResourceView>(device);
            offScreenTexture.SetView<spieler::renderer::RenderTargetView>(device);
        }

        return true;
    }

    void TestLayer::InitMaterials()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        spieler::renderer::ConstantBuffer& materialConstantBuffer{ m_ConstantBuffers["material"] };

        // Wood crate material
        {
            spieler::renderer::Material& wood{ m_Materials["wood_crate1"] };
            wood.DiffuseMap = m_Textures["wood_crate"].GetView<spieler::renderer::ShaderResourceView>();

            wood.Constants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            wood.Constants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
            wood.Constants.Roughness = 0.2f;

            materialConstantBuffer.SetSlice(&wood);
        }
        
        // Wire fence material
        {
            spieler::renderer::Material& fence{ m_Materials["wire_fence"] };
            fence.DiffuseMap = m_Textures["wire_fence"].GetView<spieler::renderer::ShaderResourceView>();

            fence.Constants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            fence.Constants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
            fence.Constants.Roughness = 0.2f;

            materialConstantBuffer.SetSlice(&fence);
        }

        // Tile material
        {
            spieler::renderer::Material& tile{ m_Materials["tile"] };
            tile.DiffuseMap = m_Textures["tile"].GetView<spieler::renderer::ShaderResourceView>();

            tile.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tile.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            tile.Constants.Roughness = 0.8f;

            materialConstantBuffer.SetSlice(&tile);
        }

        // Bricks material
        {
            spieler::renderer::Material& bricks{ m_Materials["bricks"] };
            bricks.DiffuseMap = m_Textures["bricks"].GetView<spieler::renderer::ShaderResourceView>();

            bricks.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            bricks.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            bricks.Constants.Roughness = 0.8f;

            materialConstantBuffer.SetSlice(&bricks);
        }

        // Skull material
        {
            spieler::renderer::Material& skull{ m_Materials["skull"] };
            skull.DiffuseMap = m_Textures["white"].GetView<spieler::renderer::ShaderResourceView>();

            skull.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            skull.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            skull.Constants.Roughness = 0.3f;

            materialConstantBuffer.SetSlice(&skull);
        }

        // Mirror material
        {
            spieler::renderer::Material& mirror{ m_Materials["mirror"] };
            mirror.DiffuseMap = m_Textures["mirror"].GetView<spieler::renderer::ShaderResourceView>();

            mirror.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f };
            mirror.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };
            mirror.Constants.Roughness = 0.5f;

            materialConstantBuffer.SetSlice(&mirror);
        }

        // Shadow material
        {
            spieler::renderer::Material& shadow{ m_Materials["shadow"] };
            shadow.DiffuseMap = m_Textures["white"].GetView<spieler::renderer::ShaderResourceView>();

            shadow.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.5f };
            shadow.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.001f, 0.001f, 0.001f };
            shadow.Constants.Roughness = 0.0f;

            materialConstantBuffer.SetSlice(&shadow);
        }

        // Tree material
        {
            spieler::renderer::Material& tree{ m_Materials["tree"] };
            tree.DiffuseMap = m_Textures["tree_atlas"].GetView<spieler::renderer::ShaderResourceView>();

            tree.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tree.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.01f, 0.01f, 0.01f };
            tree.Constants.Roughness = 0.125f;

            materialConstantBuffer.SetSlice(&tree);
        }
    }

    bool TestLayer::InitMeshGeometries(spieler::renderer::UploadBuffer& uploadBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        // Basic geometry
        {
            const spieler::renderer::BoxGeometryProps boxGeometryProps
            {
                .Width = 10.0f,
                .Height = 10.0f,
                .Depth = 10.0f
            };

            const spieler::renderer::GeosphereGeometryProps geosphereGeometryProps
            {
                .Radius = 10.0f,
                .SubdivisionCount = 2
            };

            const spieler::renderer::GridGeometryProps gridGeometryProps
            {
                .Width = 1.0f,
                .Depth = 1.0f,
                .RowCount = 2,
                .ColumnCount = 2
            };

            const auto& boxMeshData{ spieler::renderer::GeometryGenerator::GenerateBox<uint32_t>(boxGeometryProps) };
            const auto& geosphereMeshData{ spieler::renderer::GeometryGenerator::GenerateGeosphere<uint32_t>(geosphereGeometryProps) };
            const auto& gridMeshData{ spieler::renderer::GeometryGenerator::GenerateGrid<uint32_t>(gridGeometryProps) };

            spieler::renderer::MeshGeometry& basicMeshesGeometry{ m_MeshGeometries["basic"] };
            basicMeshesGeometry.GenerateFrom(
                {
                    { "box", boxMeshData },
                    { "geosphere", geosphereMeshData },
                    { "grid", gridMeshData },
                },
                uploadBuffer
            );
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
            for (UINT i = 0; i < tcount; ++i)
            {
                fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
            }

            fin.close();

            //
            // Pack the indices of all the meshes into one index buffer.
            //
            spieler::renderer::MeshGeometry& skullMeshGeometry{ m_MeshGeometries["skull"] };

            const spieler::renderer::BufferConfig vertexBufferConfig
            {
                .ElementSize = sizeof(spieler::renderer::Vertex),
                .ElementCount = static_cast<uint32_t>(vertices.size())
            };

            skullMeshGeometry.m_VertexBuffer.SetResource(device.CreateBuffer(vertexBufferConfig, spieler::renderer::BufferFlags::None));
            context.CopyBuffer(vertices.data(), sizeof(spieler::renderer::Vertex) * vertices.size(), uploadBuffer, *skullMeshGeometry.m_VertexBuffer.GetResource());

            const spieler::renderer::BufferConfig indexBufferConfig
            {
                .ElementSize = sizeof(uint32_t),
                .ElementCount = static_cast<uint32_t>(indices.size())
            };

            skullMeshGeometry.m_IndexBuffer.SetResource(device.CreateBuffer(indexBufferConfig, spieler::renderer::BufferFlags::None));
            context.CopyBuffer(indices.data(), sizeof(uint32_t) * indices.size(), uploadBuffer, *skullMeshGeometry.m_IndexBuffer.GetResource());

            spieler::renderer::SubmeshGeometry& submesh{ skullMeshGeometry.m_Submeshes["grid"] };
            submesh.IndexCount = static_cast<uint32_t>(indices.size());
            submesh.BaseVertexLocation = 0;
            submesh.StartIndexLocation = 0;
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
                const spieler::renderer::BufferConfig vertexBufferConfig
                {
                    .ElementSize = sizeof(TreeVertex),
                    .ElementCount = treeCount
                };

                const spieler::renderer::BufferFlags vertexBufferFlags{ spieler::renderer::BufferFlags::None };

                treeMeshGeometry.m_VertexBuffer.SetResource(device.CreateBuffer(vertexBufferConfig, vertexBufferFlags));
                context.CopyBuffer(vertices.data(), sizeof(TreeVertex)* vertices.size(), uploadBuffer, *treeMeshGeometry.m_VertexBuffer.GetResource());
            }

            // IndexBuffer
            {
                const spieler::renderer::BufferConfig indexBufferConfig
                {
                    .ElementSize = sizeof(uint32_t),
                    .ElementCount = treeCount
                };

                const spieler::renderer::BufferFlags indexBufferFlags{ spieler::renderer::BufferFlags::None };

                treeMeshGeometry.m_IndexBuffer.SetResource(device.CreateBuffer(indexBufferConfig, indexBufferFlags));
                context.CopyBuffer(indices.data(), sizeof(uint32_t)* indices.size(), uploadBuffer, *treeMeshGeometry.m_IndexBuffer.GetResource());
            }

            spieler::renderer::SubmeshGeometry& main{ treeMeshGeometry.CreateSubmesh("main") };
            main.IndexCount = static_cast<uint32_t>(indices.size());
            main.BaseVertexLocation = 0;
            main.StartIndexLocation = 0;
        }

        return true;
    }

    bool TestLayer::InitRootSignatures()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        // Default root signature
        {
            spieler::renderer::RootSignature::Config rootSignatureConfig;

            // Init root parameters
            {
                rootSignatureConfig.RootParameters.resize(4);

                rootSignatureConfig.RootParameters[0] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                rootSignatureConfig.RootParameters[1] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 1 }
                };

                rootSignatureConfig.RootParameters[2] = spieler::renderer::RootParameter::Descriptor
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 2 }
                };

                rootSignatureConfig.RootParameters[3] = spieler::renderer::RootParameter::SingleDescriptorTable
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
                rootSignatureConfig.StaticSamplers.resize(6);

                rootSignatureConfig.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Wrap, 0 };
                rootSignatureConfig.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Clamp, 1 };
                rootSignatureConfig.StaticSamplers[2] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 2 };
                rootSignatureConfig.StaticSamplers[3] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Clamp, 3 };
                rootSignatureConfig.StaticSamplers[4] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 4 };
                rootSignatureConfig.StaticSamplers[5] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Clamp, 5 };
            }

            m_RootSignatures["default"] = spieler::renderer::RootSignature{ device, rootSignatureConfig };
        }

        // Composite root signature
        {
            spieler::renderer::RootSignature::Config rootSignatureConfig;

            // Init root parameters
            {
                rootSignatureConfig.RootParameters.resize(2);

                rootSignatureConfig.RootParameters[0] = spieler::renderer::RootParameter::SingleDescriptorTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 1 },
                        .BaseShaderRegister{ 0 }
                    }
                };

                rootSignatureConfig.RootParameters[1] = spieler::renderer::RootParameter::SingleDescriptorTable
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
                rootSignatureConfig.StaticSamplers.resize(6);

                rootSignatureConfig.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Wrap, 0 };
                rootSignatureConfig.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Clamp, 1 };
                rootSignatureConfig.StaticSamplers[2] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 2 };
                rootSignatureConfig.StaticSamplers[3] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Clamp, 3 };
                rootSignatureConfig.StaticSamplers[4] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 4 };
                rootSignatureConfig.StaticSamplers[5] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Clamp, 5 };
            }

            m_RootSignatures["composite"] = spieler::renderer::RootSignature{ device, rootSignatureConfig };
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
                .FillMode = spieler::renderer::FillMode::Wireframe,
                .CullMode = spieler::renderer::CullMode::None
            };

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .RasterizerState = &rasterzerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat,
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
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
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

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
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
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Disabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Replace,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Always
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Replace,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Always
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
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
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Enabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Keep,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Keep,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
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
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Enabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Increase,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Increase,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    }
                }
            };

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
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
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::InputLayout inputLayout
            {
                spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Size", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) },
            };

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .GeometryShader = &geometryShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Point,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat
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
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::InputLayout inputLayout;

            const spieler::renderer::GraphicsPipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["composite"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = m_DepthStencilFormat
            };

            m_PipelineStates["composite"] = spieler::renderer::GraphicsPipelineState{ device, pipelineStateConfig };
        }

        return true;
    }

    void TestLayer::InitRenderItems()
    {
        spieler::renderer::MeshGeometry& basicMeshGeometry{ m_MeshGeometries["basic"] };

        spieler::renderer::ConstantBuffer& renderItemConstantBuffer{ m_ConstantBuffers["render_item"] };
        spieler::renderer::ConstantBuffer& colorRenderItemConstantBuffer{ m_ConstantBuffers["color_render_item"] };

        // Box
        {
            std::unique_ptr<spieler::renderer::RenderItem>& box{ m_RenderItems["box"] = std::make_unique<spieler::renderer::RenderItem>() };
            box->MeshGeometry = &basicMeshGeometry;
            box->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("box");
            box->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            box->Material = &m_Materials["wire_fence"];

            box->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 20.0f };

            renderItemConstantBuffer.SetSlice(box.get());

            m_OpaqueObjects.push_back(box.get());
        }

        // Sun
        {
            std::unique_ptr<spieler::renderer::RenderItem>& sun{ m_RenderItems["sun"] = std::make_unique<spieler::renderer::RenderItem>() };
            sun->MeshGeometry = &basicMeshGeometry;
            sun->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("geosphere");
            sun->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

            sun->GetComponent<ColorConstants>().Color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 0.2f, 1.0f };

            colorRenderItemConstantBuffer.SetSlice(sun.get());

            m_Lights.push_back(sun.get());
        }

        // Floor
        {
            std::unique_ptr<spieler::renderer::RenderItem>& floor{ m_RenderItems["floor"] = std::make_unique<spieler::renderer::RenderItem>() };
            floor->MeshGeometry = &basicMeshGeometry;
            floor->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            floor->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            floor->Material = &m_Materials["tile"];

            floor->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 20.0f };
            floor->Transform.Rotation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            floor->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };

            floor->TextureTransform.Scale = DirectX::XMFLOAT3{ 10.0f, 10.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(floor.get());

            m_OpaqueObjects.push_back(floor.get());
        }

        // Wall 1
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall1"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ 7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 2
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall2"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ DirectX::XM_PIDIV2, DirectX::XM_PIDIV2, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -10.0f, 5.0f, 0.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 4.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 3
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall3"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            skull->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            skull->Material = &m_Materials["skull"];

            skull->Transform.Scale = DirectX::XMFLOAT3{ 0.5f, 0.5f, 0.5f };
            skull->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 6.0f, 0.0f };

            renderItemConstantBuffer.SetSlice(skull.get());

            m_OpaqueObjects.push_back(skull.get());
        }

        // Reflected skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["reflected_skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            skull->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            skull->Material = &m_Materials["skull"];

            renderItemConstantBuffer.SetSlice(skull.get());

            m_ReflectedObjects.push_back(skull.get());
        }

        // Mirror
        {
            std::unique_ptr<spieler::renderer::RenderItem>& mirror{ m_RenderItems["mirror"] = std::make_unique<spieler::renderer::RenderItem>() };
            mirror->MeshGeometry = &basicMeshGeometry;
            mirror->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            mirror->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            mirror->Material = &m_Materials["mirror"];

            mirror->Transform.Scale = DirectX::XMFLOAT3{ 10.0f, 1.0f, 10.0f };
            mirror->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            mirror->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 5.0f, 10.0f };

            mirror->GetComponent<MirrorConstants>().Plane = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, -10.0f };

            renderItemConstantBuffer.SetSlice(mirror.get());

            m_Mirrors.push_back(mirror.get());
            m_TransparentObjects.push_back(mirror.get());
        }

        // Skull shadow
        {
            std::unique_ptr<spieler::renderer::RenderItem>& shadow{ m_RenderItems["skull_shadow"] = std::make_unique<spieler::renderer::RenderItem>() };
            shadow->MeshGeometry = &m_MeshGeometries["skull"];
            shadow->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            shadow->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            shadow->Material = &m_Materials["shadow"];

            renderItemConstantBuffer.SetSlice(shadow.get());

            m_Shadows.push_back(shadow.get());
        }

        // Billboards
        {
            std::unique_ptr<spieler::renderer::RenderItem>& trees{ m_RenderItems["trees"] = std::make_unique<spieler::renderer::RenderItem>() };
            trees->MeshGeometry = &m_MeshGeometries["trees"];
            trees->SubmeshGeometry = &m_MeshGeometries["trees"].GetSubmesh("main");
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
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::Texture2DConfig depthStencilConfig
        {
            .Width = m_Window.GetWidth(),
            .Height = m_Window.GetHeight(),
            .Format = m_DepthStencilFormat,
            .Flags = spieler::renderer::Texture2DFlags::DepthStencil
        };

        const spieler::renderer::DepthStencilClearValue depthStencilClearValue
        {
            .Depth = 1.0f,
            .Stencil = 0
        };

        SPIELER_ASSERT(device.CreateTexture(depthStencilConfig, depthStencilClearValue, m_DepthStencil.GetTexture2DResource()));
        m_DepthStencil.GetTexture2DResource().SetDebugName(L"DepthStencil");
        m_DepthStencil.SetView<spieler::renderer::DepthStencilView>(device);
    }

    void TestLayer::UpdateViewport()
    {
        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(m_Window.GetWidth());
        m_Viewport.Height = static_cast<float>(m_Window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TestLayer::UpdateScissorRect()
    {
        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(m_Window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(m_Window.GetHeight());
    }

    void TestLayer::RenderFullscreenQuad()
    {
        auto& context{ spieler::renderer::Renderer::GetInstance().GetContext() };
        auto& nativeCommandList{ context.GetNativeCommandList() };

        context.IASetVertexBuffer(nullptr);
        context.IASetIndexBuffer(nullptr);
        context.IASetPrimitiveTopology(spieler::renderer::PrimitiveTopology::TriangleList);

        nativeCommandList->DrawInstanced(6, 1, 0, 0);
    }

    void TestLayer::Render(
        std::vector<const spieler::renderer::RenderItem*> items,
        const spieler::renderer::PipelineState& pso,
        const PassConstants& passConstants,
        const spieler::renderer::ConstantBuffer* objectConstantBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };
        auto& descriptorManager{ device.GetDescriptorManager() };

        context.SetPipelineState(pso);
        context.SetGraphicsRootSignature(m_RootSignatures["default"]);

        m_ConstantBuffers["pass"].GetSlice(&passConstants).Bind(context, 0);

        for (const spieler::renderer::RenderItem* item : items)
        {
            context.IASetVertexBuffer(&item->MeshGeometry->GetVertexBuffer().GetView());
            context.IASetIndexBuffer(&item->MeshGeometry->GetIndexBuffer().GetView());
            context.IASetPrimitiveTopology(item->PrimitiveTopology);

            if (objectConstantBuffer)
            {
                objectConstantBuffer->GetSlice(item).Bind(context, 1);
            }

            // Material
            if (item->Material)
            {
                descriptorManager.Bind(context, spieler::renderer::DescriptorHeapType::SRV);

                m_ConstantBuffers["material"].GetSlice(item->Material).Bind(context, 2);
                item->Material->DiffuseMap.Bind(context, 3);
            }

            const spieler::renderer::SubmeshGeometry& submeshGeometry{ *item->SubmeshGeometry };
            context.GetNativeCommandList()->DrawIndexedInstanced(
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

        m_BlurPass.OnResize(event.GetWidth(), event.GetHeight());
        m_SobelFilterPass.OnResize(event.GetWidth(), event.GetHeight());

        // Off-screen texture
        {
            auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

            const spieler::renderer::Texture2DConfig offScreenTextureConfig
            {
                .Width{ static_cast<uint64_t>(event.GetWidth()) },
                .Height{ event.GetHeight() },
                .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
                .Flags{ spieler::renderer::Texture2DFlags::RenderTarget }
            };

            const spieler::renderer::TextureClearValue offScreenTextureClearValue
            {
                .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
            };

            spieler::renderer::Texture2D& offScreenTexture{ m_Textures["off_screen"] };

            SPIELER_ASSERT(device.CreateTexture(offScreenTextureConfig, offScreenTextureClearValue, offScreenTexture.GetTexture2DResource()));

            offScreenTexture.SetView<spieler::renderer::ShaderResourceView>(device);
            offScreenTexture.SetView<spieler::renderer::RenderTargetView>(device);
        }

        return true;
    }

} // namespace sandbox
