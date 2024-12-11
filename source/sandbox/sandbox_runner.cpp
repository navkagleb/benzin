#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_runner.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/core/logger.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/tools/render_settings_tool.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

#include <shaders/joint/structured_buffer_types.hpp>

#include "sandbox/render_passes/copy_to_back_buffer_pass.hpp"
#include "sandbox/render_passes/deferred_lighting_pass.hpp"
#include "sandbox/render_passes/environment_pass.hpp"
#include "sandbox/render_passes/full_screen_debug_pass.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"
#include "sandbox/render_passes/global_constants_pass.hpp"
#include "sandbox/render_passes/ray_tracing_shadows_pass.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    SandboxRunner::SandboxRunner()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::SandboxRunner");

        InitRenderPasses();
        InitTools();
        
        InitCamera();
        InitSceneEntities();

        m_1SecIntervalTimer.PushCallback([this]
        {
            for (const auto renderPass : magic_enum::enum_values<RenderPasses>())
            {
                m_CpuTimings[+renderPass] = m_RenderPasses[+renderPass]->GetCpuRenderTime();
            }

            m_TimingsTool->SetRunnerTimings(m_RunnerTimings);
            m_TimingsTool->SetCpuTimings(m_CpuTimings);

            m_CpuTimings = {}; // TODO: Reset it every frame
        });
    }

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        auto isRenderTextureFlippableCallback = [](uint32_t key)
        {
            const uint32_t maxKey = +magic_enum::enum_values<Texture>().back();
            const uint32_t previousTextureKey = key - 1;

            const bool isInBounds = previousTextureKey <= maxKey;
            const bool isGapExists = !magic_enum::enum_contains<Texture>(previousTextureKey); // There must be a gap between enum values, so the enum value must not exist

            return isInBounds && isGapExists;
        };

        m_RenderResources->SetMaxTextureCount(+Texture::Count);
        m_RenderResources->SetIsTextureFlippableCallback(std::move(isRenderTextureFlippableCallback));

        // The order in which render passes are added is important
        BenzinAssert(m_RenderPasses.empty());
        m_RenderPasses.resize(magic_enum::enum_count<RenderPasses>());

        m_RenderPasses[+RenderPasses::TlasBuilding] = std::make_unique<TlasBuildingPass>(*m_Device, *m_Scene);
        m_RenderPasses[+RenderPasses::GlobalConstants] = std::make_unique<GlobalConstantsPass>(*m_Device, *m_Scene);
        m_RenderPasses[+RenderPasses::Geometry] = std::make_unique<GeometryPass>(*m_Scene);
        m_RenderPasses[+RenderPasses::RayTracingShadows] = std::make_unique<RayTracingShadowsPass>(*m_Scene);
        m_RenderPasses[+RenderPasses::SigmaDenoiser] = std::make_unique<SigmaDenoiserPass>();
        m_RenderPasses[+RenderPasses::DeferredLighting] = std::make_unique<DeferredLightingPass>(*m_Scene);
        m_RenderPasses[+RenderPasses::Environment] = std::make_unique<EnvironmentPass>();
        m_RenderPasses[+RenderPasses::FullScreenDebug] = std::make_unique<FullScreenDebugPass>();
        m_RenderPasses[+RenderPasses::ImGui] = std::make_unique<benzin::ImGuiPass>(*m_ImGuiManager, +Texture::ImGui);
        m_RenderPasses[+RenderPasses::CopuToBackBuffer] = std::make_unique<CopyToBackBufferPass>();

        m_ImGuiPass = (benzin::ImGuiPass*)m_RenderPasses[+RenderPasses::ImGui].get();

#if BENZIN_IS_ASSERTS_ENABLED
        for (const auto renderPass : magic_enum::enum_values<RenderPasses>())
        {
            BenzinAssert(+renderPass == m_RenderPasses[+renderPass]->GetGpuTimerIndex());
        }
#endif
    }

    void SandboxRunner::InitTools()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitTools");

        m_RenderViewportTool->SetFinalTextureIndex(+Texture::Final);

        m_TextureViewerTool->SetTextureSelectorCallback([]
        {
            static const auto textureNames =
                magic_enum::enum_names<Texture>() |
                std::views::transform([](std::string_view name) { return name.data(); }) |
                std::ranges::to<std::vector>();

            static auto selectedTexture = Texture::SigmaShadow;

            ImGui::Combo("Texture", (int*)&selectedTexture, textureNames.data(), (int)textureNames.size());

            if (ImGui::Button("Reset"))
            {
                selectedTexture = (Texture)benzin::g_InvalidUnsigned<uint32_t>;
            }

            return +selectedTexture;
        });

        m_TimingsTool = m_ImGuiManager->PushTool<TimingsTool>(*m_Device);

        BenzinAssert(m_RenderSettingsTool != nullptr);

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<RayTracingShadowsSettings>("RayTracingShadows", true, [](RayTracingShadowsSettings& settings)
        {
            ImGui::Checkbox("IsEnabled###RayTracingShadows", &settings.IsEnabled);
            ImGui::SliderInt("RaysPerPixel", (int*)&settings.RaysPerPixel, 0, 100);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<SigmaDenoiserSettings>("SigmaDenoiser", true, [](SigmaDenoiserSettings& settings)
        {
            ImGui::Checkbox("IsEnabled###SigmaDenoiser", &settings.IsEnabled);
            ImGui::Checkbox("IsClearEnabled", &settings.IsClearEnabled);
            ImGui::Checkbox("IsPostBlurEnabled", &settings.IsPostBlurEnabled);
            ImGui::Checkbox("IsTemporalStabilizationEnabled", &settings.IsTemporalStabilizationEnabled);
            ImGui::Checkbox("IsBicubicSamplingUsedForHistory", &settings.IsBicubicSamplingUsedForHistory);
            ImGui::DragFloat("StabilizationStrength", &settings.StabilizationStrength, 0.001f, 0.0f, 1.0f);
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<DeferredLightingSettings>("DeferredLighting", true, [](DeferredLightingSettings& settings)
        {
            ImGui::DragFloat("SunIntensity", &settings.SunIntensity, 0.1f, 0.0f, 100.0f);
            ImGui::ColorEdit3("SunColor", reinterpret_cast<float*>(&settings.SunColor));

            ImGui::SliderAngle("SunAngularDiameter", &settings.SunAngularDiameterInRadians, 0.01f, 5.0f, "%.3f");
            ImGui::SliderAngle("SunAzimuth (Yaw)", &settings.SunAzimuthInRadians, -180.0f, 180.0f);
            ImGui::SliderAngle("SunElevation (Pitch)", &settings.SunElevationInRadians, 0.0f, 180.0f);

            const auto sunDirection = GetSunDirection(settings);
            ImGui::Text(BenzinFormatData("SunDirection: [{:.3f}, {:.3f}, {:.3f}]", sunDirection.x, sunDirection.y, sunDirection.z));
        });

        m_RenderSettingsTool->RegisterSectionImGuiSpawnCallback<FullScreenDebugSettings>("FullScreenDebug", true, [this](FullScreenDebugSettings& settings)
        {
            ImGui::SliderInt("ViewDepthMipIndex", (int*)&settings.ViewDepthMipIndex, 0, 4);
            ImGui::SliderFloat("MinViewDepth", &settings.MinViewDepth, 0.001f, 2.0f, "%.4f");
            ImGui::SliderFloat("MaxViewDepth", &settings.MaxViewDepth, 0.001f, 30.0f);

            static const auto debugOutputTypeNames = magic_enum::enum_names<joint::DebugOutputType>() |
                std::views::transform([](std::string_view name) { return name.substr("DebugOutputType_"sv.size()).data(); }) |
                std::ranges::to<std::vector>();

            ImGui::Combo("DebugOutputType", (int*)&settings.DebugOutputType, debugOutputTypeNames.data(), (int)debugOutputTypeNames.size());

            const auto spawnButton = [&](joint::DebugOutputType type)
            {
                const std::string_view buttonName = magic_enum::enum_name(type).substr("DebugOutputType_"sv.size());
                if (ImGui::Button(buttonName.data()))
                {
                    settings.DebugOutputType = type;
                }
            };

            spawnButton(joint::DebugOutputType_None);
            ImGui::SameLine();
            spawnButton(joint::DebugOutputType_SigmaSmoothTiles);
            ImGui::SameLine();
            spawnButton(joint::DebugOutputType_SigmaShadow);
        });
    }

    void SandboxRunner::InitCamera()
    {
        auto& perspectiveProjection = m_Scene->GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

#if 0
        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -3.0f, 2.0f, -0.25f });
        camera.SetFrontDirection({ 1.0f, 0.0f, 0.0f });
#elif 0
        // For denoising results

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -0.244f, 0.846f, -1.346f });
        camera.SetFrontDirection({ 0.439f, -0.413f, -0.798f });

        m_AnimationTimer.SetPaused(true);
#else
        // For global results

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });

        m_AnimationTimer.SetPaused(true);
#endif
    }

    void SandboxRunner::InitSceneEntities()
    {
        SceneMeshes sceneMeshes;
        LoadAndCreateMeshes(sceneMeshes);
        CreateEntities(sceneMeshes);
    }

    void SandboxRunner::LoadAndCreateMeshes(SceneMeshes& outSceneMeshes)
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::LoadAndCreateMeshes");

        const auto createCylinderMeshCollection = []
        {
            const benzin::Material material
            {
                .AlbedoFactor{ 0.7f, 0.7f, 0.7f, 1.0f },
            };

            const benzin::MeshInstance meshInstance
            {
                .MeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshCollectionResource
            {
                .DebugName = "Cylinder",
                .Meshes{ benzin::GetDefaultCyliderMesh() },
                .MeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto createSphereLightMeshCollection = []
        {
            const benzin::Material material
            {
                .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                .EmissiveFactor{ 1.0f, 1.0f, 1.0f },
            };

            const benzin::MeshInstance meshInstance
            {
                .MeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshCollectionResource
            {
                .DebugName = "Cylinder",
                .Meshes{ benzin::GetDefaultGeoSphereMesh() },
                .MeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto loadFromFile = [](std::string_view fileName)
        {
            // BenzinLogTimeOnScopeExit("Loading MeshCollection from {}", fileName);

            benzin::MeshCollectionResource resource;
            BenzinAssertExpr(benzin::LoadMeshCollectionFromGltfFile(fileName, resource));

            return resource;
        };

        benzin::EnumArray<benzin::MeshCollectionResource, SceneMesh> meshCollectionResources;

        const auto sponzaFuture = std::async(std::launch::async, [&]
        {
            meshCollectionResources[+SceneMesh::Sponza] = loadFromFile("Sponza/glTF/Sponza.gltf");
        });

        meshCollectionResources[+SceneMesh::BoomBox] = loadFromFile("BoomBox/glTF/BoomBox.gltf");
        meshCollectionResources[+SceneMesh::DamagedHelmet] = loadFromFile("DamagedHelmet/glTF/DamagedHelmet.gltf");
        meshCollectionResources[+SceneMesh::OrientationTest] = loadFromFile("OrientationTest/OrientationTest.gltf");
        meshCollectionResources[+SceneMesh::Cylinder] = createCylinderMeshCollection();
        meshCollectionResources[+SceneMesh::Sphere] = createSphereLightMeshCollection();

        sponzaFuture.wait();

        for (auto&& [outSceneMesh, meshCollectionResource] : std::views::zip(outSceneMeshes, meshCollectionResources))
        {
            outSceneMesh = m_Scene->PushMeshCollection(std::move(meshCollectionResource));
        }
    }

    void SandboxRunner::CreateEntities(const SceneMeshes& sceneMeshes)
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::CreateEntities");

        auto& entityRegistry = m_Scene->GetEntityRegistry();

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Sponza];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            tc.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::BoomBox];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 30.0f, 30.0f, 30.0f });
            tc.SetTranslation({ 0.0f, 0.6f, 0.0f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();

                tc.SetRotation(rotation);
            };
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::DamagedHelmet];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetRotation({ 0.0f, DirectX::XMConvertToRadians(-135.0f), 0.0f });
            tc.SetScale({ 0.4f, 0.4f, 0.4f });
            tc.SetTranslation({ 1.0f, 0.5f, -0.5f });

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                auto rotation = tc.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();

                tc.SetRotation(rotation);
            };
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::OrientationTest];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ 0.05f, 0.05f, 0.05f });
            tc.SetTranslation({ 2.5f, 0.4f, -0.25f });
        }

        {
            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Cylinder];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ 0.1f, 1.5f, 0.1f });
            tc.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }

        {
            const auto sunEntity = entityRegistry.create();

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(sunEntity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                BenzinUnused(entityRegistry);
                BenzinUnused(entityHandle);

                const auto animateSunAngle = [this](
                    float minAngleInRadians,
                    float maxAngleInRadians,
                    float& outAngleInRadians,
                    float& outDirection
                )
                {
                    const float speed = 0.01f;

                    outAngleInRadians += outDirection * m_AnimationTimer.GetDeltaTimeInSec() * speed;

                    if (!(minAngleInRadians <= outAngleInRadians && outAngleInRadians <= maxAngleInRadians))
                    {
                        outDirection *= -1.0f;
                    }
                };

                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& settings = m_RenderSettings->GetSection<DeferredLightingSettings>();

                static float elevationDirection = 1.0f;
                static float azimithDirection = 1.0f;

                animateSunAngle(DirectX::XMConvertToRadians(41.0f), DirectX::XMConvertToRadians(52.0f), settings.SunElevationInRadians, elevationDirection);
                animateSunAngle(DirectX::XMConvertToRadians(-2.0f), DirectX::XMConvertToRadians(3.0f), settings.SunAzimuthInRadians, azimithDirection);
            };
        }

#if 0
        {
            constexpr float sphereLightRadius = 0.02f;

            const auto entity = entityRegistry.create();

            auto& mic = entityRegistry.emplace<benzin::MeshInstanceComponent>(entity);
            mic.MeshUnionIndex = sceneMeshes[+SceneMesh::Sphere];

            auto& tc = entityRegistry.emplace<benzin::TransformComponent>(entity);
            tc.SetScale({ sphereLightRadius, sphereLightRadius, sphereLightRadius });
            tc.SetTranslation({ 0.5f, 2.0f, -0.25f });

            auto& plc = entityRegistry.emplace<benzin::PointLightComponent>(entity);
            plc.Color = { 1.0f, 1.0f, 1.0f };
            plc.Intensity = 0.0f;
            plc.Range = 30.0f;
            plc.GeometryRadius = sphereLightRadius;

            auto& uc = entityRegistry.emplace<benzin::UpdateComponent>(entity);
            uc.Callback = [this](entt::registry& entityRegistry, entt::entity entityHandle)
            {
                if (m_AnimationTimer.IsPaused())
                {
                    return;
                }

                auto& tc = entityRegistry.get<benzin::TransformComponent>(entityHandle);

                static constexpr float travelRadius = 1.0f;
                static constexpr float travelSpeed = 0.001f;

                static const float startX = tc.GetTranslation().x;
                static const float startZ = tc.GetTranslation().z;

                auto translation = tc.GetTranslation();
                translation.x = startX + travelRadius * std::cos(travelSpeed * m_AnimationTimer.GetElapsedTimeInMs());
                translation.z = startZ + travelRadius * std::sin(travelSpeed * m_AnimationTimer.GetElapsedTimeInMs());

                const auto& settings = m_RenderSettings->GetSection<DeferredLightingSettings>();
                tc.SetTranslation(GetSunDirection(settings));
            };
        }
#endif
    }

}
