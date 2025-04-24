#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_runner.hpp"

#include <benzin/core/logger.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/tools/render_settings_tool.hpp>
#include <benzin/tools/render_viewport_tool.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>
#include <benzin/utility/random.hpp>

#include <shaders/joint/mesh_types.hpp>

#include "sandbox/render_passes/deferred_lighting_pass.hpp"
#include "sandbox/render_passes/environment_pass.hpp"
#include "sandbox/render_passes/geometry_pass.hpp"
#include "sandbox/render_passes/global_consts_pass.hpp"
#include "sandbox/render_passes/procedural_grass_pass.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"
#include "sandbox/render_passes/tone_mapping_pass.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::ProceduralGrassConsts);

namespace sandbox
{

    enum class Mesh : uint32_t
    {
        Sponza,
        BoomBox,
        DamagedHelmet,
        OrientationTest,
        MilkTruck,
        GltfMeshCount,

        Cylinder = GltfMeshCount,
        UnitSphere,
    };
    BenzinEnableUnaryPlusForEnum(Mesh);

    static void LoadMeshes(std::span<benzin::MeshResource> outMeshResources)
    {
        BenzinLogTimeOnScopeExit("LoadMeshes");

        const auto createCylinderMesh = []
        {
            const benzin::Material material
            {
                .AlbedoFactor{ 0.7f, 0.7f, 0.7f, 1.0f },
            };

            const joint::MeshInstance meshInstance
            {
                .SubMeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshResource
            {
                .DebugName = "Cylinder",
                .SubMeshes{ benzin::GetDefaultCyliderMesh() },
                .SubMeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto createUnitSphereMesh = []
        {
            const benzin::Material material
            {
                .AlbedoFactor{ 0.0f, 0.0f, 0.0f, 0.0f },
                .EmissiveFactor{ 1.0f, 1.0f, 1.0f },
            };

            const joint::MeshInstance meshInstance
            {
                .SubMeshIndex = 0,
                .MaterialIndex = 0,
            };

            return benzin::MeshResource
            {
                .DebugName = "UnitSphere",
                .SubMeshes{ benzin::GetUnitGeoSphereMesh() },
                .SubMeshInstances{ meshInstance },
                .Materials{ material },
            };
        };

        const auto loadFromFile = [](std::string_view fileName)
        {
            if (fileName.empty())
            {
                return benzin::MeshResource{};
            }

            benzin::MeshResource resource;
            BenzinAssertExpr(benzin::LoadMeshFromGltfFile(fileName, resource));

            return resource;
        };

        std::array<std::string_view, +Mesh::GltfMeshCount> gltfFileNames;
        gltfFileNames[+Mesh::Sponza] = "Sponza/glTF/Sponza.gltf";
        gltfFileNames[+Mesh::BoomBox] = "BoomBox/glTF/BoomBox.gltf";
        gltfFileNames[+Mesh::DamagedHelmet] = "DamagedHelmet/glTF/DamagedHelmet.gltf";
        gltfFileNames[+Mesh::OrientationTest] = "OrientationTest/OrientationTest.gltf";
        gltfFileNames[+Mesh::MilkTruck] = "CesiumMilkTruck/glTF/CesiumMilkTruck.gltf";

        std::array<std::future<void>, +Mesh::GltfMeshCount> gltfFutures;
        for (const uint32_t i : std::views::iota(0u, +Mesh::GltfMeshCount))
        {
            gltfFutures[i] = std::async(std::launch::async, [&, i]
            {
                outMeshResources[i] = loadFromFile(gltfFileNames[i]);
            });
        }

        outMeshResources[+Mesh::Cylinder] = createCylinderMesh();
        outMeshResources[+Mesh::UnitSphere] = createUnitSphereMesh();

        for (auto& future : gltfFutures)
        {
            future.wait();
        }
    }

    static void DrawGBufferSettings(GBufferSettings& settings)
    {
        ImGui::Checkbox("Depth pre-pass", &settings.IsDepthPrePassEnabled);
        ImGui::Checkbox("CPU frustum culling", &settings.IsFrustumCullingEnabled);
    }

    static void DrawGBufferStats(GBufferStats& stats)
    {
        std::locale::global(benzin::Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        ImGui::Text(BenzinFormatData("Mesh count: {:L} (Max: {:L})", stats.RenderedMeshCount, stats.MeshCount));
        ImGui::Text(BenzinFormatData("Triangle count: {:L}", stats.RenderedTriangleCount));
    }

    static void DrawProceduralGrassSettings(ProceduralGrassSettings& settings)
    {
        ImGui::Checkbox("Enable ### ProceduralGrass", &settings.IsEnabled);

        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui_CollapsingHeaderWithIndent("Amplification", [&settings]
        {
            ImGui::Checkbox("GPU frustum culling", &settings.IsFrustumCullingEnabled);
            ImGui::DragFloat("Patch cull radius", &settings.GrassPatchCullRadius, 0.0001f);
        });

        ImGui_CollapsingHeaderWithIndent("Mesh", [&settings]
        {
            ImGui::DragFloat("Grass end distance", &settings.GrassEndDistance, 0.01f);

            if (ImGui::DragFloat("Spacing in patch (between blades)", &settings.SpacingInGrassPatch, 0.0001f))
            {
                settings.SpacingInGrassPatch = std::max(settings.SpacingInGrassPatch, 0.001f);
            }

            ImGui::DragFloat("Wind direction", &settings.WindDirection, 0.01f, 0.0f, DirectX::XM_2PI);
            ImGui::DragFloat("Blade width", &settings.BladeWidth, 0.0001f, std::numeric_limits<float>::min());
        });

        ImGui_CollapsingHeaderWithIndent("Pixel", [&settings]
        {
            ImGui::ColorEdit3("Base color", (float*)&settings.BaseColor);
        });
    }

    static void DrawProceduralGrassStats(ProceduralGrassStats& stats)
    {
        using enum joint::ProceduralGrassConsts;

        std::locale::global(benzin::Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        ImGui::Text(BenzinFormatData("Patch count: {:L} (Max: {:L})", stats.PatchCount, stats.MaxPatchCount));
        ImGui::Text(BenzinFormatData("Blade count: {:L} (Max: {:L})", stats.BladeCount, stats.MaxPatchCount * +MaxBladeCountPerPatch));
        ImGui::Text(BenzinFormatData("Vertex count: {:L}", stats.VertexCount));
        ImGui::Text(BenzinFormatData("Triangle count: {:L}", stats.TriangleCount));
    }

    static void DrawRayTracingShadowsSettings(RayTracing_ShadowSettings& settings)
    {
        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable###RayTracingShadows", &settings.IsEnabled);

        ImGui::Checkbox("Use blue noise", &settings.IsBlueNoiseUsed);
        ImGui::Checkbox("Animate noise", &settings.IsNoiseAnimated);

        ImGui::Checkbox("Freeze blue noise depth", &settings.IsBlueNoiseDepthFreezed);

        ImGui::BeginDisabled();
        auto tempBlueNoiseDepthIndex = (int)settings.BlueNoiseDepthIndex;
        ImGui::SliderInt("Blue noise depth index", &tempBlueNoiseDepthIndex, 0, settings.BlueNoiseDepth - 1);
        ImGui::EndDisabled();
    }

    static void DrawSigmaDenoiserSettings(SigmaDenoiserSettings& settings)
    {
        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### SigmaDenoiser", &settings.IsEnabled);
        ImGui::Checkbox("Clear pass", &settings.IsClearEnabled);

        ImGui_CollapsingHeaderWithIndent("Classification", [&settings]
        {
            ImGui::Checkbox("Tile smoothing", &settings.IsTileSmoothingEnabled);
        });

        ImGui_CollapsingHeaderWithIndent("Blur", [&settings]
        {
            ImGui::Checkbox("Post blur pass", &settings.IsPostBlurEnabled);

            ImGui::DragFloat("Plane distance sensitivity %", &settings.PlaneDistanceSensitivity, 0.0001f, 0.0f, 0.1f);
        });

        ImGui_CollapsingHeaderWithIndent("Temporal stabilization", [&settings]
        {
            ImGui::Checkbox("Temporal stabilization pass", &settings.IsTemporalStabilizationEnabled);

            ImGui::BeginDisabled(true);
            ImGui::SliderInt("History length", (int*)&settings.HistoryLength, 0, settings.MaxHistoryLength, "%d", ImGuiSliderFlags_NoInput);
            ImGui::DragFloat("Stabilization strength", &settings.StabilizationStrength);
            ImGui::EndDisabled();

            ImGui::DragFloat("Disocclusion threshold %", &settings.DisocclusionThreshold, 0.0001f, 0.0f, 0.2f);
        });
    }

    static void DrawToneMappingSettings(ToneMappingSettings& settings)
    {
        ImGui::PushItemWidth(120.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### ToneMapping", &settings.IsToneMappingEnabled);

        ImGui_CollapsingHeaderWithIndent("Luminance Histogram", [&settings]
        {
            auto& luminanceHistogram = settings.LuminanceHistogram;

            if (ImGui::DragFloat("Min log luminance", &luminanceHistogram.MinLogLuminance))
            {
                luminanceHistogram.MinLogLuminance = std::clamp(
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance
                );
            }

            if (ImGui::DragFloat("Max log luminance", &luminanceHistogram.MaxLogLuminance))
            {
                luminanceHistogram.MaxLogLuminance = std::clamp(
                    luminanceHistogram.MaxLogLuminance,
                    luminanceHistogram.MinLogLuminance,
                    luminanceHistogram.MaxLogLuminance
                );
            }

            ImGui::InputFloat("Tau", &luminanceHistogram.Tau);
        });

        ImGui_CollapsingHeaderWithIndent("PBR Camera", [&settings]
        {
            auto& pbrCamera = settings.PbrCamera;

            ImGui::Checkbox("Auto exposure", &settings.IsAutoExposureUsed);

            ImGui::DragFloat("Aperture (in f-stops)", &pbrCamera.Aperture, 0.001f);
            ImGui::DragFloat("Shutter speed (in sec)", &pbrCamera.ShutterSpeed, 0.001f);
            ImGui::DragFloat("Sensor sensitivity (in ISO)", &pbrCamera.Iso, 0.01f);
        });

        ImGui_CollapsingHeaderWithIndent("Tone Mapping", [&settings]
        {
            ImGui::Checkbox("Accurate gamma correction", &settings.IsAccurateGammaCorrectionUsed);

            static const auto toneReproductionTransformNames = magic_enum::enum_names<joint::ToneReproductionTransform>();

            ImGui::Combo(
                "Tone reproduction transform",
                (int*)&settings.ToneReproductionTransform,
                ImGui_SelectComboName<decltype(toneReproductionTransformNames)>,
                (void*)&toneReproductionTransformNames,
                (int)toneReproductionTransformNames.size()
            );
        });
    }

    //

    void SandboxRunner::InitRenderPasses()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitRenderPasses");

        // The order in which render passes are added is important
        BenzinAssert(m_RenderPasses.empty());
        m_RenderPasses.push_back(std::make_unique<TlasBuildingPass>());
        m_RenderPasses.push_back(std::make_unique<GlobalConstsPass>());
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
        BenzinAssert(m_RenderSettingsTool != nullptr);
        m_RenderSettingsTool->RegisterSectionDrawCallback<GBufferSettings>(DrawGBufferSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<GBufferStats>(DrawGBufferStats, ImGuiTreeNodeFlags_None);
        m_RenderSettingsTool->RegisterSectionDrawCallback<ProceduralGrassSettings>(DrawProceduralGrassSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<ProceduralGrassStats>(DrawProceduralGrassStats, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<RayTracing_ShadowSettings>(DrawRayTracingShadowsSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<SigmaDenoiserSettings>(DrawSigmaDenoiserSettings, ImGuiTreeNodeFlags_DefaultOpen);
        m_RenderSettingsTool->RegisterSectionDrawCallback<ToneMappingSettings>(DrawToneMappingSettings, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void SandboxRunner::InitScene()
    {
        InitCamera();
        InitSceneEntities();
    }

    void SandboxRunner::InitCamera()
    {
        auto& perspectiveProjection = m_Scene->GetPerspectiveProjection();
        perspectiveProjection.SetLens(DirectX::XMConvertToRadians(90.0f), 16.0f / 9.0f, 0.1f, 100.0f);

        auto& camera = m_Scene->GetCamera();
        camera.SetPosition({ -1.649f, 1.007f, -1.555f });
        camera.SetFrontDirection({ 0.769f, 0.129f, 0.627f });
    }

    void SandboxRunner::InitSceneEntities()
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::InitSceneEntities");

        std::array<benzin::MeshResource, magic_enum::enum_count<Mesh>()> meshResources{};
        LoadMeshes(meshResources);

        std::array<entt::entity, magic_enum::enum_count<Mesh>()> meshHandles;
        meshHandles.fill(benzin::g_BadEnum<entt::entity>);
        AddMeshesToScene(meshResources, meshHandles);

        AddStaticMeshEntities(meshHandles);
        AddDynamicMeshEntities(meshHandles);
        AddProceduralGrass();
        AddLightEntities(meshHandles);
    }

    void SandboxRunner::AddMeshesToScene(std::span<benzin::MeshResource> meshResources, std::span<entt::entity> outMeshHandles)
    {
        BenzinLogTimeOnScopeExit("SandboxRunner::AddMeshesToScene");

        for (auto&& [outMeshHandle, meshResource] : std::views::zip(outMeshHandles, meshResources))
        {
            outMeshHandle = !meshResource.SubMeshes.empty() ? m_Scene->AddMesh(std::move(meshResource)) : benzin::g_BadEnum<entt::entity>;
        }
    }

    void SandboxRunner::AddStaticMeshEntities(std::span<const entt::entity> meshHandles)
    {
        auto& entityRegistry = m_Scene->GetEntityRegistry();

        if (benzin::IsGoodEnum(meshHandles[+Mesh::Sponza]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::Sponza];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XM_PI, 0.0f });
            transform.SetTranslation({ 5.0f, 0.0f, 0.0f });
        }

        if (benzin::IsGoodEnum(meshHandles[+Mesh::OrientationTest]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::OrientationTest];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.05f, 0.05f, 0.05f });
            transform.SetTranslation({ 2.5f, 0.2f, -0.25f });
        }

        if (benzin::IsGoodEnum(meshHandles[+Mesh::MilkTruck]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::MilkTruck];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 0.1f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.2f, 0.5f });
        }

        if (benzin::IsGoodEnum(meshHandles[+Mesh::Cylinder]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::Cylinder];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetScale({ 0.1f, 1.5f, 0.1f });
            transform.SetTranslation({ -1.5f, 0.4f, -0.25f });
        }
    }

    void SandboxRunner::AddDynamicMeshEntities(std::span<const entt::entity> meshHandles)
    {
        auto& entityRegistry = m_Scene->GetEntityRegistry();

        if (benzin::IsGoodEnum(meshHandles[+Mesh::BoomBox]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::BoomBox];

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

        if (benzin::IsGoodEnum(meshHandles[+Mesh::DamagedHelmet]))
        {
            const auto entity = entityRegistry.create();

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::DamagedHelmet];

            auto& transform = entityRegistry.emplace<benzin::Transform>(entity);
            transform.SetRotation({ 0.0f, DirectX::XMConvertToRadians(45.0f), 0.0f });
            transform.SetScale({ 0.4f, 0.4f, 0.4f });
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
    }

    void SandboxRunner::AddProceduralGrass()
    {
        const int32_t xRadius = 160;
        const int32_t zRadius = 70;
        const int32_t totalCount = (xRadius * 2 + 1) * (zRadius * 2 + 1);

        std::vector<joint::GrassPatch> grassPatches;
        grassPatches.reserve(totalCount);

        auto& entityRegistry = m_Scene->GetEntityRegistry();

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

    void SandboxRunner::AddLightEntities(std::span<const entt::entity> meshHandles)
    {
        entt::registry& entityRegistry = m_Scene->GetEntityRegistry();

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

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

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

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

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

            auto& mc = entityRegistry.emplace<benzin::MeshComponent>(entity);
            mc.MeshHandle = meshHandles[+Mesh::UnitSphere];

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
    }

}
