#include <sandbox/bootstrap.hpp>
#include <sandbox/sandbox_runner.hpp>

#include <benzin/core/logger.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>
#include <benzin/utility/random.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <sandbox/render_passes/deferred_lighting_pass.hpp>
#include <sandbox/render_passes/environment_pass.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>
#include <sandbox/render_passes/ray_tracing_shadow_pass.hpp>
#include <sandbox/render_passes/sigma_denoiser_pass.hpp>
#include <sandbox/render_passes/tlas_building_pass.hpp>
#include <sandbox/render_passes/tone_mapping_pass.hpp>
#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::ReadbackStat);

namespace sandbox
{

    // SandboxRunner

    SandboxRunner::~SandboxRunner()
    {
        m_ImGuiManager->UnregisterTool<SettingsTool<GBufferSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<GBufferStats>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ProceduralGrassSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ProceduralGrassStats>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<RayTracing_ShadowSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<SigmaDenoiserSettings>>();
        m_ImGuiManager->UnregisterTool<SettingsTool<ToneMappingSettings>>();
    }

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        auto readbackStatsCallback = [this](std::span<const uint32_t> readbackStats)
        {
            {
                auto& stats = m_RenderSettings->GetSection<GBufferStats>();

                stats.m_TotalMeshletCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletCount];
                stats.m_TotalMeshletVertexCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletVertexCount];
                stats.m_TotalMeshletTriangleCount = readbackStats[+joint::ReadbackStat::Geometry_TotalMeshletTriangleCount];

                stats.m_MeshletCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletCount];
                stats.m_MeshletVertexCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletVertexCount];
                stats.m_MeshletTriangleCount = readbackStats[+joint::ReadbackStat::Geometry_MeshletTriangleCount];

                stats.m_VsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_VsInvocationCount];
                stats.m_AsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_AsInvocationCount];  
                stats.m_MsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_MsInvocationCount];
                stats.m_PsInvocationCount = readbackStats[+joint::ReadbackStat::Geometry_PsInvocationCount];
            }

            {
                auto& stats = m_RenderSettings->GetSection<ProceduralGrassStats>();
                stats.PatchCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_PatchCount];
                stats.BladeCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_BladeCount];
                stats.VertexCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_VertexCount];
                stats.TriangleCount = readbackStats[+joint::ReadbackStat::ProceduralGrass_TriangleCount];
            }
        };

        // The order in which render passes are added is important
        m_RenderPasses.push_back(std::make_unique<TlasBuildingPass>());
        m_RenderPasses.push_back(std::make_unique<GlobalConstsPass>(std::move(readbackStatsCallback)));
        m_RenderPasses.push_back(std::make_unique<GeometryPass>());
        m_RenderPasses.push_back(std::make_unique<ProceduralGrassPass>());
        m_RenderPasses.push_back(std::make_unique<RayTracing_ShadowPass>());
        m_RenderPasses.push_back(std::make_unique<SigmaDenoiserPass>());
        m_RenderPasses.push_back(std::make_unique<DeferredLightingPass>());
        m_RenderPasses.push_back(std::make_unique<EnvironmentPass>());
        m_RenderPasses.push_back(std::make_unique<ToneMappingPass>());
    }

    void SandboxRunner::InitTools()
    {
        m_ImGuiManager->RegisterTool<SettingsTool<GBufferSettings>>("Settings/GBuffer", m_RenderSettings->GetSection<GBufferSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<GBufferStats>>("Settings/GBufferStats", m_RenderSettings->GetSection<GBufferStats>());
        m_ImGuiManager->RegisterTool<SettingsTool<ProceduralGrassSettings>>("Settings/ProceduralGrass", m_RenderSettings->GetSection<ProceduralGrassSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<ProceduralGrassStats>>("Settings/ProceduralGrassStats", m_RenderSettings->GetSection<ProceduralGrassStats>());
        m_ImGuiManager->RegisterTool<SettingsTool<RayTracing_ShadowSettings>>("Settings/RayTracing_Shadow", m_RenderSettings->GetSection<RayTracing_ShadowSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<SigmaDenoiserSettings>>("Settings/SigmaDenoiser", m_RenderSettings->GetSection<SigmaDenoiserSettings>());
        m_ImGuiManager->RegisterTool<SettingsTool<ToneMappingSettings>>("Settings/ToneMapping", m_RenderSettings->GetSection<ToneMappingSettings>());
    }

    // SponzaRunner

    void SponzaRunner::InitScene()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitScene");

        std::unordered_map<std::string, entt::entity> meshHandles;

        constexpr auto meshFileNames = std::to_array<std::string_view>(
        {
            "Sponza/glTF/Sponza.gltf",
            "BoomBox/glTF-Binary/BoomBox.glb",
            "DamagedHelmet/glTF/DamagedHelmet.gltf",
            "OrientationTest/OrientationTest.gltf",
            "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf",
        });

        for (const std::string_view fileName : meshFileNames)
        {
            benzin::MeshResource mesh;
            std::vector<benzin::MaterialResource> materials;
            std::vector<benzin::TextureImage> textures;
            BenzinAssertExpr(benzin::LoadMeshFromGltfFile(fileName, mesh, materials, textures));

            std::string debugName = benzin::CutExtension(fileName);

            const entt::entity meshHandle = m_Scene->AddMesh(debugName, std::move(mesh), std::move(materials), std::move(textures));
            meshHandles[debugName] = meshHandle;
        }

        auto& entityRegistry = m_Scene->GetEntityRegistry();

        if (meshHandles.contains("Sponza"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("Sponza"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            transform.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        if (meshHandles.contains("OrientationTest"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("OrientationTest"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.05f, 0.05f, 0.05f });
            transform.SetTranslation({ 2.5f, 0.2f, -0.25f });
        }

        if (meshHandles.contains("MilkTruck"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("MilkTruck"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 0.1f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.2f, 0.5f });
        }

        if (meshHandles.contains("Cylinder"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("Cylinder"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 1.5f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }

        if (meshHandles.contains("BoomBox"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("BoomBox"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XMConvertToRadians(45.0f), 0.0f });
            transform.SetScale({ 30.0f, 30.0f, 30.0f });
            transform.SetTranslation({ 0.0f, 0.6f, 0.0f });

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                auto& transform = entityRegistry.get<benzin::Transform>(entity);

                auto rotation = transform.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();

                transform.SetRotation(rotation);
            });
        }

        if (meshHandles.contains("DamagedHelmet"))
        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, meshHandles.at("DamagedHelmet"));

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XMConvertToRadians(45.0f), 0.0f });
            // transform.SetScale({ 0.4f, 0.4f, 0.4f });
            transform.SetTranslation({ 1.0f, 0.5f, -0.5f });

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                auto& transform = entityRegistry.get<benzin::Transform>(entity);

                auto rotation = transform.GetRotation();
                rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();

                transform.SetRotation(rotation);
            });
        }

        // Procedural grass
        {
            const int32_t xRadius = 160;
            const int32_t zRadius = 70;
            const int32_t totalCount = (xRadius * 2 + 1) * (zRadius * 2 + 1);

            std::vector<joint::GrassPatch> grassPatches;
            grassPatches.reserve(totalCount);

            for (auto x = -xRadius; x <= xRadius; ++x)
            {
                for (auto z = -zRadius; z <= zRadius; ++z)
                {
                    auto& grassPatch = entityRegistry.emplace<joint::GrassPatch>(entityRegistry.create());

                    grassPatch.Pos.x = (float)x * 0.07f + 6.0f;
                    grassPatch.Pos.z = (float)z * 0.07f - 0.3f;

                    const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVECTOR
                    {
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        1.0f,
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        0.0f
                    });
                    DirectX::XMStoreFloat3(&grassPatch.Normal, normal);

                    grassPatch.Height = benzin::Random::Get<float>(0.07f, 0.13f);
                }
            }
        }

        // Sun
        {
            const auto entity = m_Scene->GetSunEntity();

            auto& light = entityRegistry.get_or_emplace<benzin::SunLight>(entity);
            light.SetColor({ 1.0f, 1.0f, 0.7f });
            light.SetIntensity(10.0f);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                const auto animateSunAngle = [this](
                    float minAngleInRadians,
                    float maxAngleInRadians,
                    float& outAngleInRadians,
                    float& outDirection
                )
                {
                    constexpr float speed = 0.01f;

                    outAngleInRadians += outDirection * m_AnimationTimer.GetDeltaTimeInSec() * speed;

                    if (!(minAngleInRadians <= outAngleInRadians && outAngleInRadians <= maxAngleInRadians))
                    {
                        outDirection *= -1.0f;
                    }
                };

                static float elevationDirection = 1.0f;
                static float azimithDirection = 1.0f;

                auto& light = entityRegistry.get<benzin::SunLight>(entity);

                float elevation = light.GetElevationInRadians();;
                animateSunAngle(DirectX::XMConvertToRadians(41.0f), DirectX::XMConvertToRadians(52.0f), elevation, elevationDirection);
                light.SetElevationInRadians(elevation);

                float azimuth = light.GetAzimuthInRadians();
                animateSunAngle(DirectX::XMConvertToRadians(-2.0f), DirectX::XMConvertToRadians(3.0f), azimuth, azimithDirection);
                light.SetAzimuthInRadians(azimuth);
            });
        }

        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, m_Scene->GetUnitSphereMeshHandle());

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetIntensity(5.0f);
            light.SetPosition({ 0.5f, 2.0f, -0.25f });
            light.SetRadius(0.03f);
            light.SetRange(10.0f);
            light.SetEnabled(false);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                constexpr float travelRadius = 1.0f;
                constexpr float travelSpeed = 0.5f;

                auto& light = entityRegistry.get<benzin::SphericalLight>(entity);

                static const float startX = light.GetPosition().x;
                static const float startZ = light.GetPosition().z;

                auto position = light.GetPosition();
                position.x = startX + travelRadius * std::cos(travelSpeed * m_AnimationTimer.GetElapsedTimeInSec());
                position.z = startZ + travelRadius * std::sin(travelSpeed * m_AnimationTimer.GetElapsedTimeInSec());

                light.SetPosition(position);
            });
        }

        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, m_Scene->GetUnitSphereMeshHandle());

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetColor({ 0.7f, 0.8f, 0.3f });
            light.SetIntensity(10.0f);
            light.SetPosition({ 0.0f, 3.0f, 1.25f });
            light.SetRadius(0.01f);
            light.SetRange(30.0f);
            light.SetEnabled(false);
        }

        {
            const auto entity = entityRegistry.create();

            entityRegistry.emplace<benzin::MeshComponent>(entity, m_Scene->GetUnitSphereMeshHandle());

            auto& light = entityRegistry.emplace<benzin::SphericalLight>(entity);
            light.SetColor({ 0.9f, 0.7f, 0.8f });
            light.SetIntensity(15.0f);
            light.SetPosition({ 1.5f, 4.0f, -1.0f });
            light.SetRadius(0.02f);
            light.SetRange(20.0f);
            light.SetEnabled(false);

            entityRegistry.emplace<benzin::EntityUpdateCallback>(entity, [this, &entityRegistry, entity]
            {
                constexpr float speed = 1.5f;
                constexpr float min = -1.0f;
                constexpr float max = 12.0f;

                static float direction = 1.0f;

                auto& light = entityRegistry.get<benzin::SphericalLight>(entity);

                auto position = light.GetPosition();
                position.x += direction * speed * m_AnimationTimer.GetDeltaTimeInSec();

                if (position.x < min || position.x > max)
                {
                    position.x = std::clamp(position.x, min, max);
                    direction *= -1.0f;
                }

                light.SetPosition(position);
            });
        }

        benzin::PerspectiveCamera& camera = m_Scene->GetCamera();
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);
    }

    // StanfordDragonRunner

    void StanfordDragonRunner::InitScene()
    {
        benzin::PerspectiveCamera& camera = m_Scene->GetCamera();
        camera.SetPosition({ -2.286f, 3.911f, -18.385f });
        camera.SetFrontDirection({ 0.149f, -0.185f, 0.972f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);

        benzin::MeshResource dragon;
        std::vector<benzin::MaterialResource> materials;
        std::vector<benzin::TextureImage> textures;
        BenzinAssertExpr(benzin::LoadMeshFromGltfFile("StanfordDragon/StanfordDragon.glb", dragon, materials, textures));

        const entt::entity dragonMeshHandle = m_Scene->AddMesh("StanfordDragon", std::move(dragon), std::move(materials), std::move(textures));
        const int32_t radius = 3;

        for (auto x = -radius; x <= radius; ++x)
        {
            for (auto y = -radius; y <= radius; ++y)
            {
                for (auto z = -radius; z <= radius; ++z)
                {
                    const auto entity = m_Scene->GetEntityRegistry().create();

                    m_Scene->GetEntityRegistry().emplace<benzin::MeshComponent>(entity, dragonMeshHandle);

                    DirectX::XMFLOAT3 translation{};
                    translation.x = (float)x * 2.5f;
                    translation.y = (float)y * 2.5f;
                    translation.z = (float)z * 2.5f;

                    DirectX::XMFLOAT3 rotation{};
                    rotation.x = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    rotation.y = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    rotation.z = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);

                    const float scaleFactor = benzin::Random::Get<float>(0.05f, 0.1f);

                    auto& transform = m_Scene->GetEntityRegistry().emplace<benzin::Transform>(entity);
                    transform.SetTranslation(translation);
                    transform.SetRotation(rotation);
                    transform.SetScale({ scaleFactor, scaleFactor, scaleFactor });
                }
            }
        }

        const auto entity = m_Scene->GetSunEntity();

        auto& light = m_Scene->GetEntityRegistry().get_or_emplace<benzin::SunLight>(entity);
        light.SetColor({ 1.0f, 1.0f, 0.7f });
        light.SetIntensity(10.0f);
    }

}
