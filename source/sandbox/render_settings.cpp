#include <sandbox/bootstrap.hpp>
#include <sandbox/render_settings.hpp>

#include <benzin/core/logger.hpp>

#include <shaders/joint/procedural_grass_resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::ProceduralGrassConsts);

namespace sandbox
{

    template <>
    void DrawSettings(GBufferSettings& settings)
    {
        const auto drawColoringType = [&settings]
        {
            static const auto names = magic_enum::enum_names<joint::DebugColoringType>();

            ImGui::SameLine();
            ImGui::Combo(
                "Coloring type",
                (int*)&settings.ColoringType,
                ImGui::SelectComboName<decltype(names)>,
                (void*)&names,
                (int)names.size()
            );
        };

        drawColoringType();
        ImGui::Checkbox("CPU frustum culling", &settings.IsCpuFrustumCullingEnabled);
        ImGui::Checkbox("Mesh pipeline", &settings.IsMeshPipelineUsed);

        ImGui::Indent();
        ImGui::BeginDisabled(!settings.IsMeshPipelineUsed);
        {
            ImGui::Checkbox("Amplification dispatch", &settings.IsAmplificationDispatchUsed);
            ImGui::Checkbox("GPU frustum culling", &settings.IsGpuFrustumCullingEnabled);
            ImGui::Checkbox("Back-face cone culling", &settings.IsBackfaceCullingEnabled);
            ImGui::Checkbox("Occlusion culling", &settings.IsOcclusionCullingEnabled);
        }
        ImGui::EndDisabled();
        ImGui::Unindent();
    }

    template <>
    void DrawSettings(GBufferStats& stats)
    {
        std::locale::global(benzin::Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        const auto drawRow = [](const char* name, uint32_t totalCount, uint32_t renderedCount)
        {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text(name);
            ImGui::TableNextColumn();
            ImGui::FmtText("{:L}", totalCount);
            ImGui::TableNextColumn();
            ImGui::FmtText("{:L}", renderedCount);

            const float renderedPercent = totalCount != 0 ? (float)renderedCount / totalCount : 0.0f;
            ImGui::TableNextColumn();
            ImGui::FmtText("{:.2f}", renderedPercent);
        };

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg;
        if (ImGui::BeginTable("GBufferStatsTable", 4, flags))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Total");
            ImGui::TableSetupColumn("Rendered");
            ImGui::TableSetupColumn("Rendered %");
            ImGui::TableHeadersRow();

            drawRow("Meshlets", stats.m_TotalMeshletCount, stats.m_MeshletCount);
            drawRow("Meshlet vertices", stats.m_TotalMeshletVertexCount, stats.m_MeshletVertexCount);
            drawRow("Meshlet triangles", stats.m_TotalMeshletTriangleCount, stats.m_MeshletTriangleCount);

            ImGui::EndTable();
        }

        ImGui::NewLine();
        ImGui::FmtText("VS invocations: {:L}", stats.m_VsInvocationCount);
        ImGui::FmtText("AS invocations: {:L}", stats.m_AsInvocationCount);
        ImGui::FmtText("MS invocations: {:L}", stats.m_MsInvocationCount);
        ImGui::FmtText("PS invocations: {:L}", stats.m_PsInvocationCount);

        ImGui::NewLine();
        ImGui::FmtText("Viewport pixels: {:L}", stats.m_ViewportPixelCount);
        ImGui::FmtText("Overdraw: {:.2f} %%", ((float)stats.m_PsInvocationCount / stats.m_ViewportPixelCount) * 100.0f);
    }

    template <>
    void DrawSettings(ProceduralGrassSettings& settings)
    {
        ImGui::PushItemWidth(150.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### ProceduralGrass", &settings.IsEnabled);

        ImGui::BeginDisabled(!settings.IsEnabled);
        BenzinExecuteOnScopeExit([] { ImGui::EndDisabled(); });

        ImGui::NewLine();
        ImGui::SeparatorText("AMPLIFICATION");
        {
            ImGui::Checkbox("GPU frustum culling", &settings.IsFrustumCullingEnabled);
            ImGui::DragFloat("Patch cull radius", &settings.GrassPatchCullRadius, 0.0001f);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("MESH");
        {
            ImGui::DragFloat("Grass end distance", &settings.GrassEndDistance, 0.01f);

            if (ImGui::DragFloat("Spacing in patch (between blades)", &settings.SpacingInGrassPatch, 0.0001f))
            {
                settings.SpacingInGrassPatch = std::max(settings.SpacingInGrassPatch, 0.001f);
            }

            ImGui::DragFloat("Wind direction", &settings.WindDirection, 0.01f, 0.0f, DirectX::XM_2PI);
            ImGui::DragFloat("Blade width", &settings.BladeWidth, 0.0001f, std::numeric_limits<float>::min());
        }

        ImGui::NewLine();
        ImGui::SeparatorText("PIXEL");
        {
            ImGui::ColorEdit3("Base color", (float*)&settings.BaseColor);
        }
    }

    template <>
    void DrawSettings(ProceduralGrassStats& stats)
    {
        using enum joint::ProceduralGrassConsts;

        std::locale::global(benzin::Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        ImGui::FmtText("Patch count: {:L} (Max: {:L})", stats.PatchCount, stats.MaxPatchCount);
        ImGui::FmtText("Blade count: {:L} (Max: {:L})", stats.BladeCount, stats.MaxPatchCount * +MaxBladeCountPerPatch);
        ImGui::FmtText("Vertex count: {:L}", stats.VertexCount);
        ImGui::FmtText("Triangle count: {:L}", stats.TriangleCount);
    }

    template <>
    void DrawSettings(RayTracing_ShadowSettings& settings)
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

    template <>
    void DrawSettings(SigmaDenoiserSettings& settings)
    {
        ImGui::Checkbox("Enable ### SigmaDenoiser", &settings.IsEnabled);

        ImGui::BeginDisabled(!settings.IsEnabled);
        BenzinExecuteOnScopeExit([] { ImGui::EndDisabled(); });

        ImGui::Checkbox("Clear pass", &settings.IsClearEnabled);

        ImGui::NewLine();
        ImGui::SeparatorText("CLASSIFICATION");
        {
            ImGui::Checkbox("Tile smoothing", &settings.IsTileSmoothingEnabled);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("BLUR");
        {
            ImGui::Checkbox("Post blur pass", &settings.IsPostBlurEnabled);
            ImGui::DragFloat("Plane distance sensitivity %", &settings.PlaneDistanceSensitivity, 0.0001f, 0.0f, 0.1f);
        }

        ImGui::NewLine();
        ImGui::SeparatorText("TEMPORAL STABILIZATION");
        {
            ImGui::Checkbox("Temporal stabilization pass", &settings.IsTemporalStabilizationEnabled);
            ImGui::DragFloat("Disocclusion threshold %", &settings.DisocclusionThreshold, 0.0001f, 0.0f, 0.2f);

            ImGui::BeginDisabled(true);
            ImGui::SliderInt("History length", (int*)&settings.HistoryLength, 0, settings.MaxHistoryLength, "%d", ImGuiSliderFlags_NoInput);
            ImGui::DragFloat("Stabilization strength", &settings.StabilizationStrength);
            ImGui::EndDisabled();
        }
    }

    template <>
    void DrawSettings(ToneMappingSettings& settings)
    {
        ImGui::PushItemWidth(120.0f);
        BenzinExecuteOnScopeExit([] { ImGui::PopItemWidth(); });

        ImGui::Checkbox("Enable ### ToneMapping", &settings.IsToneMappingEnabled);

        ImGui::CollapsingHeaderWithIndent("Luminance Histogram", [&settings]
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

        ImGui::CollapsingHeaderWithIndent("PBR Camera", [&settings]
        {
            auto& pbrCamera = settings.PbrCamera;

            ImGui::Checkbox("Auto exposure", &settings.IsAutoExposureUsed);

            ImGui::DragFloat("Aperture (in f-stops)", &pbrCamera.Aperture, 0.001f);
            ImGui::DragFloat("Shutter speed (in sec)", &pbrCamera.ShutterSpeed, 0.001f);
            ImGui::DragFloat("Sensor sensitivity (in ISO)", &pbrCamera.Iso, 0.01f);
        });

        ImGui::CollapsingHeaderWithIndent("Tone Mapping", [&settings]
        {
            ImGui::Checkbox("Accurate gamma correction", &settings.IsAccurateGammaCorrectionUsed);

            static const auto toneReproductionTransformNames = magic_enum::enum_names<joint::ToneReproductionTransform>();

            ImGui::Combo(
                "Tone reproduction transform",
                (int*)&settings.ToneReproductionTransform,
                ImGui::SelectComboName<decltype(toneReproductionTransformNames)>,
                (void*)&toneReproductionTransformNames,
                (int)toneReproductionTransformNames.size()
            );
        });
    }

}
