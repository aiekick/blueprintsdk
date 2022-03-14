#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <ColorBalance_vulkan.h>

#define ICON_RESET     "\uf0e2"

namespace BluePrint
{
struct ColorBalanceNode final : Node
{
    BP_NODE_WITH_NAME(ColorBalanceNode, "Color Balance", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Color")
    ColorBalanceNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Color Balance"; }

    ~ColorBalanceNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
    }

    void OnStop(Context& context) override
    {
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
            if (!m_bEnabled)
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
            m_filter->filter(mat_in, im_RGB, m_shadows, m_midtones, m_highlights, m_preserve_lightness);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
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

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin) override
    {
        ImGui::SetCurrentContext(ctx);
        ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        bool changed = false;
        bool check = m_bEnabled;
        std::vector<float> _shadows = m_shadows;
        std::vector<float> _midtones = m_midtones;
        std::vector<float> _highlights = m_highlights;
        bool _preserve_lightness = m_preserve_lightness;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::TextUnformatted("Enable"); ImGui::SameLine();
        if (ImGui::ToggleButton("##enable_filter_ColorBalance",&check)) { m_bEnabled = check; changed = true; }
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        ImGui::Bullet(); ImGui::TextUnformatted("Shadow");
        ImGui::TextUnformatted("R:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#RShadow", ImVec2(200, 20), &_shadows[0], 0.0f, zoom, 32, 1.0, false, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("G:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#GShadow", ImVec2(200, 20), &_shadows[1], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("B:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#BShadow", ImVec2(200, 20), &_shadows[2], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
        ImGui::Bullet(); ImGui::TextUnformatted("Midtones");
        ImGui::TextUnformatted("R:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#RMidtones", ImVec2(200, 20), &_midtones[0], 0.0f, zoom, 32, 1.0, false, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("G:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#GMidtones", ImVec2(200, 20), &_midtones[1], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("B:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#BMidtones", ImVec2(200, 20), &_midtones[2], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
        ImGui::Bullet(); ImGui::TextUnformatted("Highlights");
        ImGui::TextUnformatted("R:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#RHighlights", ImVec2(200, 20), &_highlights[0], 0.0f, zoom, 32, 1.0, false, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("G:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#GHighlights", ImVec2(200, 20), &_highlights[1], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::TextUnformatted("B:"); ImGui::SameLine(); ImGui::LumianceSelector("##color_balance#BHighlights", ImVec2(200, 20), &_highlights[2], 0.0f, zoom, 32, 1.0, false, ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
        ImGui::PopItemWidth();
        if (ImGui::Checkbox("Preserve Lightness", &_preserve_lightness)) {m_preserve_lightness = _preserve_lightness; changed = true; }
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
        if (value.contains("enabled"))
        { 
            auto& val = value["enabled"];
            if (val.is_boolean())
                m_bEnabled = val.get<imgui_json::boolean>();
        }
        if (value.contains("shadows"))
        { 
            auto& val = value["shadows"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_shadows[0] = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_shadows[1] = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_shadows[2] = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_shadows[3] = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("midtones"))
        { 
            auto& val = value["midtones"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_midtones[0] = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_midtones[1] = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_midtones[2] = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_midtones[3] = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("highlights"))
        { 
            auto& val = value["highlights"];
            if (val.contains("r")) {auto& fvalue = val["r"]; if (fvalue.is_number()) m_highlights[0] = fvalue.get<imgui_json::number>();}
            if (val.contains("g")) {auto& fvalue = val["g"]; if (fvalue.is_number()) m_highlights[1] = fvalue.get<imgui_json::number>();}
            if (val.contains("b")) {auto& fvalue = val["b"]; if (fvalue.is_number()) m_highlights[2] = fvalue.get<imgui_json::number>();}
            if (val.contains("a")) {auto& fvalue = val["a"]; if (fvalue.is_number()) m_highlights[3] = fvalue.get<imgui_json::number>();}
        }
        if (value.contains("preserve_lightness"))
        { 
            auto& val = value["preserve_lightness"];
            if (val.is_boolean())
                m_preserve_lightness = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        {
            imgui_json::value shadows;
            shadows["r"] = imgui_json::number(m_shadows[0]);
            shadows["g"] = imgui_json::number(m_shadows[1]);
            shadows["b"] = imgui_json::number(m_shadows[2]);
            shadows["a"] = imgui_json::number(m_shadows[3]);
            value["shadows"] = shadows;
        }
        {
            imgui_json::value midtones;
            midtones["r"] = imgui_json::number(m_midtones[0]);
            midtones["g"] = imgui_json::number(m_midtones[1]);
            midtones["b"] = imgui_json::number(m_midtones[2]);
            midtones["a"] = imgui_json::number(m_midtones[3]);
            value["midtones"] = midtones;
        }
        {
            imgui_json::value highlights;
            highlights["r"] = imgui_json::number(m_highlights[0]);
            highlights["g"] = imgui_json::number(m_highlights[1]);
            highlights["b"] = imgui_json::number(m_highlights[2]);
            highlights["a"] = imgui_json::number(m_highlights[3]);
            value["highlights"] = highlights;
        }
        value["preserve_lightness"] = imgui_json::boolean(m_preserve_lightness);
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
    bool m_bEnabled             {true};
    ImGui::ColorBalance_vulkan * m_filter   {nullptr};
    std::vector<float> m_shadows     {0, 0, 0, 0};
    std::vector<float> m_midtones    {0, 0, 0, 0};
    std::vector<float> m_highlights  {0, 0, 0, 0};
    bool m_preserve_lightness {false};
};
} // namespace BluePrint
