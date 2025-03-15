#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/imgui_helpers.hpp>

bool Imgui_MainCollapsingHeader(std::string_view name, ImGuiTreeNodeFlags flags)
{
    const ImVec4 headerColor{ 0.7f, 1.0f, 0.7f, 1.0f };
    const ImVec4 headerBackground{ 0.7f * 0.3f, 1.0f * 0.3f, 0.7f * 0.3f, 1.0f };

    ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
    ImGui::PushStyleColor(ImGuiCol_Header, headerBackground);
    BenzinExecuteOnScopeExit([] { ImGui::PopStyleColor(2); });

    flags |= ImGuiTreeNodeFlags_CollapsingHeader;
    return ImGui::CollapsingHeader(name.data(), flags);
}

void ImGui_CollapsingHeaderWithIndent(std::string_view name, const ImGui_DrawCallback& callback, ImGuiTreeNodeFlags flags)
{
    flags |= ImGuiTreeNodeFlags_FramePadding;
    flags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(name.data(), flags))
    {
        BenzinAssert(callback);
        callback();

        ImGui::TreePop();
    }
}
