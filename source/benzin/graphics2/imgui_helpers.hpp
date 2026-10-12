#pragma once

namespace ImGui
{

    using DrawCallback = std::function<void()>;

    template <typename... Args>
    void FmtText(std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string text = std::format(fmt, std::forward<Args>(args)...);
        ::ImGui::Text(text.c_str());
    }

    template <typename... Args>
    void FmtBulletText(std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string text = std::format(fmt, std::forward<Args>(args)...);
        ::ImGui::BulletText(text.c_str());
    }

    template <typename... Args>
    void FmtSeparatorText(std::format_string<Args...> fmt, Args&&... args)
    {
        const std::string text = std::format(fmt, std::forward<Args>(args)...);
        ::ImGui::SeparatorText(text.c_str());
    }

    template <typename T>
    static bool SelectComboName(void* data, int index, const char** outName)
    {
        const auto& names = *(T*)data;

        if (index < 0 || index >= names.size())
        {
            return false;
        }

        *outName = names[index].data();
        return true;
    };

    void WarningBox(std::string_view text, std::string_view id);

    bool MainCollapsingHeader(std::string_view name, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None);
    void CollapsingHeaderWithIndent(std::string_view name, const DrawCallback& callback, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None);

}
