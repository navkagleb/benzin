#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/imgui_helpers.hpp>

void ImGui_CollapsingHeaderWithIndent(std::string_view name, const ImGui_DrawCallback& callback, ImGuiTreeNodeFlags additionalFlags)
{
    ImGuiTreeNodeFlags treeNodeFlags = additionalFlags;
    treeNodeFlags |= ImGuiTreeNodeFlags_FramePadding;
    treeNodeFlags |= ImGuiTreeNodeFlags_Selected;

    if (ImGui::TreeNodeEx(name.data(), treeNodeFlags))
    {
        BenzinAssert(callback);
        callback();

        ImGui::TreePop();
    }
}
