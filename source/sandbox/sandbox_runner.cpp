#include <sandbox/bootstrap.hpp>
#include <sandbox/sandbox_runner.hpp>

#include <sandbox/render_passes/deferred_lighting_pass.hpp>
#include <sandbox/render_passes/environment_pass.hpp>
#include <sandbox/render_passes/geometry_pass.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>
#include <sandbox/render_passes/procedural_grass_pass.hpp>
#include <sandbox/render_passes/ray_tracing_shadow_pass.hpp>
#include <sandbox/render_passes/sigma_denoiser_pass.hpp>
#include <sandbox/render_passes/tone_mapping_pass.hpp>
#include <sandbox/render_settings.hpp>

#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/utility/random.hpp>

#include <shaders/joint/mesh_types.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::ReadbackStat);

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
        BenzinTraceScopeTime("SandboxRunner::InitRenderPasses");

        auto readbackStatsCallback = [this](std::span<const uint32_t> readbackStats)
        {
            {
                auto& stats = m_RenderSettings->GetSection<GBufferStats>();
                stats.m_TotalMeshCount = readbackStats[*joint::ReadbackStat::Geometry_TotalMeshCount];
                stats.m_RenderedMeshCount = readbackStats[*joint::ReadbackStat::Geometry_RenderedMeshCount];
                stats.m_RenderedTriangleCount = readbackStats[*joint::ReadbackStat::Geometry_RenderedTriangleCount];
            }

            {
                auto& stats = m_RenderSettings->GetSection<ProceduralGrassStats>();
                stats.PatchCount = readbackStats[*joint::ReadbackStat::ProceduralGrass_PatchCount];
                stats.BladeCount = readbackStats[*joint::ReadbackStat::ProceduralGrass_BladeCount];
                stats.VertexCount = readbackStats[*joint::ReadbackStat::ProceduralGrass_VertexCount];
                stats.TriangleCount = readbackStats[*joint::ReadbackStat::ProceduralGrass_TriangleCount];
            }
        };

        // The order in which render passes are added is important
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
        BenzinTraceScopeTime("SandboxRunner::InitScene");

        benzin::PerspectiveCamera& camera = m_Scene.m_Camera;
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);

        benzin::SunLight& sunLight = m_Scene.m_SunLight;
        sunLight.m_Color = { 1.0f, 1.0f, 0.7f };
        sunLight.m_Intensity = 10.0f;

        m_UpdateCallbacks.push_back([this]
        {
            const auto animateSunAngle = [this](float minAngle, float maxAngle, float& angle, float& direction)
            {
                angle += direction * m_AnimationTimer.GetDeltaTimeInSec() * 0.01f;

                if (!(minAngle <= angle && angle <= maxAngle))
                {
                    direction *= -1.0f;
                }
            };

            static float elevationDirection = 1.0f;
            static float azimithDirection = 1.0f;

            float& elevation = m_Scene.m_SunLight.m_ElevationInRadians;
            float& azimuth = m_Scene.m_SunLight.m_AzimuthInRadians;

            animateSunAngle(DirectX::XMConvertToRadians(41.0f), DirectX::XMConvertToRadians(52.0f), elevation, elevationDirection);
            animateSunAngle(DirectX::XMConvertToRadians(-2.0f), DirectX::XMConvertToRadians(3.0f), azimuth, azimithDirection);
        });

        constexpr auto meshFileNames = std::to_array<std::string_view>(
        {
            "Sponza/glTF/Sponza.gltf",
            "BoomBox/glTF-Binary/BoomBox.glb",
            "DamagedHelmet/glTF/DamagedHelmet.gltf",
            "OrientationTest/OrientationTest.gltf",
            "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf",
        });

        std::unordered_map<std::string, benzin::Scene::MeshGeometryRange> geometries;
        std::mutex geometryMutex;

        std::for_each(
            std::execution::par,
            meshFileNames.begin(),
            meshFileNames.end(),
            [&geometries, &geometryMutex, this](const std::string_view fileName)
            {
                benzin::MeshGeometry geometry;
                std::vector<benzin::MeshDraw> draws;
                std::vector<benzin::Material> materials;
                std::vector<benzin::TextureImage> textures;
                BenzinAssertExpr(benzin::LoadMeshFromGltfFile(fileName, geometry, draws, materials, textures));

                const std::string debugName = benzin::CutExtension(fileName);

                std::scoped_lock lock{ geometryMutex };

                geometries[debugName] = m_Scene.AddMeshGeometry(
                    debugName,
                    std::move(geometry),
                    std::move(draws),
                    std::move(materials),
                    std::move(textures));
            });

        const auto addGeometryDraw = [this, &geometries](
            const std::string& name,
            const DirectX::XMFLOAT3& translation,
            float scale,
            const DirectX::XMFLOAT3& rotation = {})
        {
            if (!geometries.contains(name))
                return benzin::g_MaxU32;

            benzin::MeshGeometryDraw& draw = m_Scene.m_MeshGeometryDraws.emplace_back();
            draw.m_MeshDrawOffset = geometries.at(name).m_MeshDrawOffset;
            draw.m_MeshDrawCount = geometries.at(name).m_MeshDrawCount;
            draw.m_Translation = translation;
            draw.m_Scale = scale;
            draw.m_Rotation = rotation;

            return (uint32_t)m_Scene.m_MeshGeometryDraws.size() - 1;
        };

        addGeometryDraw("Sponza", { 5.0f, 0.0f, 0.0f }, 1.0f, { 0.0f, DirectX::XM_PI, 0.0f });
        addGeometryDraw("OrientationTest", { 2.5f, 0.2f, -0.25f }, 0.05f);
        addGeometryDraw("CesiumMilkTruck", { -1.5f, 0.2f, 0.5f }, 0.1f);
        addGeometryDraw("Cylinder", { -1.5f, 0.4f, -0.25f }, 0.1f); // actually { 0.1f, 1.5f, 0.1f }

        const uint32_t boomBooxDrawIndex = addGeometryDraw("BoomBox", { 0.0f, 0.6f, 0.0f }, 30.0f, { 0.0f, DirectX::XMConvertToRadians(45.0f), 0.0f });
        const uint32_t damagedHelmetDrawIndex = addGeometryDraw("DamagedHelmet", { 1.0f, 0.5f, -0.5f }, 0.4f, { 0.0f, DirectX::XMConvertToRadians(45.0f), 0.0f });

        if (boomBooxDrawIndex != benzin::g_MaxU32)
        {
            m_UpdateCallbacks.emplace_back([this, boomBooxDrawIndex]
            {
                benzin::MeshGeometryDraw& draw = m_Scene.m_MeshGeometryDraws[boomBooxDrawIndex];
                draw.m_Rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                draw.m_Rotation.z += 0.0002f * m_AnimationTimer.GetDeltaTimeInMs();
            });
        }

        if (damagedHelmetDrawIndex != benzin::g_MaxU32)
        {
            m_UpdateCallbacks.emplace_back([this, damagedHelmetDrawIndex]
            {
                benzin::MeshGeometryDraw& draw = m_Scene.m_MeshGeometryDraws[damagedHelmetDrawIndex];
                draw.m_Rotation.x += 0.0001f * m_AnimationTimer.GetDeltaTimeInMs();
                draw.m_Rotation.y -= 0.00015f * m_AnimationTimer.GetDeltaTimeInMs();
            });
        }

        // Procedural grass
        {
            const int32_t xRadius = 160;
            const int32_t zRadius = 70;
            const int32_t totalCount = (xRadius * 2 + 1) * (zRadius * 2 + 1);

            m_Scene.m_GrassPatches.reserve(totalCount);

            for (auto x = -xRadius; x <= xRadius; ++x)
            {
                for (auto z = -zRadius; z <= zRadius; ++z)
                {
                    const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVECTOR
                    {
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        1.0f,
                        benzin::Random::Get<float>(-0.1f, 0.1f),
                        0.0f,
                    });

                    joint::GrassPatch& grassPatch = m_Scene.m_GrassPatches.emplace_back();
                    grassPatch.Pos.x = (float)x * 0.07f + 6.0f;
                    grassPatch.Pos.z = (float)z * 0.07f - 0.3f;
                    grassPatch.Height = benzin::Random::Get<float>(0.07f, 0.13f);
                    DirectX::XMStoreFloat3(&grassPatch.Normal, normal);
                }
            }
        }
    }

    // StanfordDragonRunner

    void StanfordDragonRunner::InitScene()
    {
        m_RenderSettings->GetSection<RayTracing_ShadowSettings>().m_IsEnabled = false;
        m_RenderSettings->GetSection<SigmaDenoiserSettings>().m_IsEnabled = false;

        benzin::PerspectiveCamera& camera = m_Scene.m_Camera;
        camera.SetPosition({ -2.286f, 3.911f, -18.385f });
        camera.SetFrontDirection({ 0.149f, -0.185f, 0.972f });
        camera.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.05f);

        benzin::MeshGeometry dragon;
        std::vector<benzin::MeshDraw> draws;
        std::vector<benzin::Material> materials;
        std::vector<benzin::TextureImage> textures;
        BenzinAssertExpr(benzin::LoadMeshFromGltfFile("StanfordDragon/StanfordDragon.glb", dragon, draws, materials, textures));

        const auto [drawOffset, drawCount] = m_Scene.AddMeshGeometry(
            "StanfordDragon",
            std::move(dragon),
            std::move(draws),
            std::move(materials),
            std::move(textures));

        const int32_t radius = 3;

        for (auto x = -radius; x <= radius; ++x)
        {
            for (auto y = -radius; y <= radius; ++y)
            {
                for (auto z = -radius; z <= radius; ++z)
                {
                    benzin::MeshGeometryDraw& draw = m_Scene.m_MeshGeometryDraws.emplace_back();
                    draw.m_MeshDrawOffset = drawOffset;
                    draw.m_MeshDrawCount = drawCount;
                    draw.m_Translation.x = (float)x * 2.5f;
                    draw.m_Translation.y = (float)y * 2.5f;
                    draw.m_Translation.z = (float)z * 2.5f;
                    draw.m_Rotation.x = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    draw.m_Rotation.y = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    draw.m_Rotation.z = benzin::Random::Get<float>(0.0f, DirectX::XM_2PI);
                    draw.m_Scale = benzin::Random::Get<float>(0.03f, 0.15f);
                }
            }
        }
    }

}
