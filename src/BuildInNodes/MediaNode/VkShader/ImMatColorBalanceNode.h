#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <ColorBalance_vulkan.h>

namespace BluePrint
{
struct ColorBalanceNode final : Node
{
    BP_NODE_WITH_NAME(ColorBalanceNode, "Color Balance", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Color")
    ColorBalanceNode(BP* blueprint): Node(blueprint) { m_Name = "Color Balance"; }

    ~ColorBalanceNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::ColorBalance_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_shadows, m_midtones, m_highlights, m_preserve_lightness);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        bool changed = false;
        ImVec4 _shadows = m_shadows;
        ImVec4 _midtones = m_midtones;
        ImVec4 _highlights = m_highlights;
        bool _preserve_lightness = m_preserve_lightness;
        bool _ganged = m_ganged;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        static ImVec2 offset = ImVec2(0, 0);
        if (ImGui::BalanceSelector("Shadow", ImVec2(100, 100), &_shadows, ImVec4(0, 0, 0, 0), m_ganged ? &offset : nullptr, zoom, 0.5))
        {
            if (m_ganged) _midtones = _highlights = ImVec4(0, 0, 0, 0);
        }
        ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
        if (ImGui::BalanceSelector("Midtones", ImVec2(100, 100), &_midtones, ImVec4(0, 0, 0, 0), m_ganged ? &offset : nullptr, zoom, 0.5))
        {
            if (m_ganged) _shadows = _highlights = ImVec4(0, 0, 0, 0);
        }
        ImGui::SameLine(); ImGui::Spacing(); ImGui::SameLine();
        if (ImGui::BalanceSelector("Highlights", ImVec2(100, 100), &_highlights, ImVec4(0, 0, 0, 0), m_ganged ? &offset : nullptr, zoom, 0.5))
        {
            if (m_ganged) _shadows = _midtones = ImVec4(0, 0, 0, 0);
        }
        ImGui::PopItemWidth();
        if (ImGui::Checkbox("Preserve Lightness", &_preserve_lightness)) {m_preserve_lightness = _preserve_lightness; changed = true; }
        if (ImGui::Checkbox("Color Ganged", &_ganged)) {m_ganged = _ganged; changed = true; }
        if (_shadows != m_shadows) { m_shadows = _shadows; changed = true; }
        if (_midtones != m_midtones) { m_midtones = _midtones; changed = true; }
        if (_highlights != m_highlights) { m_highlights = _highlights; changed = true; }
        ImGui::EndDisabled();
        return changed;
    }

    int Load(const imgui_json::value& value) override
    {
        int ret = BP_ERR_NONE;
        if ((ret = Node::Load(value)) != BP_ERR_NONE)
            return ret;

        if (value.contains("mat_type"))
        {
            auto& val = value["mat_type"];
            if (val.is_number()) 
                m_mat_data_type = (ImDataType)val.get<imgui_json::number>();
        }
        if (value.contains("shadows"))
        { 
            auto& val = value["shadows"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_shadows.x = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_shadows.y = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_shadows.z = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_shadows.w = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("midtones"))
        { 
            auto& val = value["midtones"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_midtones.x = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_midtones.y = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_midtones.z = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_midtones.w = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("highlights"))
        { 
            auto& val = value["highlights"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_highlights.x = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_highlights.y = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_highlights.z = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_highlights.w = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("preserve_lightness"))
        { 
            auto& val = value["preserve_lightness"];
            if (val.is_boolean())
                m_preserve_lightness = val.get<imgui_json::boolean>();
        }
        if (value.contains("ganged"))
        { 
            auto& val = value["ganged"];
            if (val.is_boolean())
                m_ganged = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        {
            imgui_json::value shadows;
            shadows["r"] = imgui_json::number(m_shadows.x);
            shadows["g"] = imgui_json::number(m_shadows.y);
            shadows["b"] = imgui_json::number(m_shadows.z);
            shadows["a"] = imgui_json::number(m_shadows.w);
            value["shadows"] = shadows;
        }
        {
            imgui_json::value midtones;
            midtones["r"] = imgui_json::number(m_midtones.x);
            midtones["g"] = imgui_json::number(m_midtones.y);
            midtones["b"] = imgui_json::number(m_midtones.z);
            midtones["a"] = imgui_json::number(m_midtones.w);
            value["midtones"] = midtones;
        }
        {
            imgui_json::value highlights;
            highlights["r"] = imgui_json::number(m_highlights.x);
            highlights["g"] = imgui_json::number(m_highlights.y);
            highlights["b"] = imgui_json::number(m_highlights.z);
            highlights["a"] = imgui_json::number(m_highlights.w);
            value["highlights"] = highlights;
        }
        value["preserve_lightness"] = imgui_json::boolean(m_preserve_lightness);
        value["ganged"] = imgui_json::boolean(m_ganged);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        float font_size = ImGui::GetFontSize();
        float size_min = size.x > size.y ? size.y : size.x;
        ImGui::SetWindowFontScale((size_min - 16) / font_size);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
#if IMGUI_ICONS
        ImGui::Button((std::string(u8"\uf53f") + "##" + std::to_string(m_ID)).c_str(), size);
#else
        ImGui::Button((std::string("F") + "##" + std::to_string(m_ID)).c_str(), size);
#endif
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device                {-1};
    ImGui::ColorBalance_vulkan * m_filter   {nullptr};
    ImVec4 m_shadows            {0, 0, 0, 0};
    ImVec4 m_midtones           {0, 0, 0, 0};
    ImVec4 m_highlights         {0, 0, 0, 0};
    bool m_preserve_lightness {false};
    bool m_ganged {false};
};
} // namespace BluePrint
