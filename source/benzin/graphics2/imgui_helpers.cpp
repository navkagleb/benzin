#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/imgui_helpers.hpp>

bool ImGui_MainCollapsingHeader(std::string_view name, ImGuiTreeNodeFlags flags)
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
    flags |= ImGuiTreeNodeFlags_SpanFullWidth;

    if (ImGui::TreeNodeEx(name.data(), flags))
    {
        BenzinAssert(callback);
        callback();

        ImGui::TreePop();
    }
}

void ImGui_WarningBox(std::string_view text, std::string_view id)
{
    constexpr ImVec4 warningColor{ 1.0f, 0.5f, 0.0f, 1.0f };

    ImGui::BeginChild(id.data(), ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_None);

    ImGui::PushStyleColor(ImGuiCol_Text, warningColor);
    ImGui::PushTextWrapPos(0.0f); // Wrap at the right edge of the child window
    ImGui::Text(text.data());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    ImGui::EndChild();
}
