#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <DoomScreen_vulkan.h>

namespace BluePrint
{
struct DoomScreenFusionNode final : Node
{
    BP_NODE_WITH_NAME(DoomScreenFusionNode, "DoomScreen Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    DoomScreenFusionNode(BP& blueprint): Node(blueprint) { m_Name = "DoomScreen Transform"; }

    ~DoomScreenFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
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
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float progress = context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_fusion || m_device != gpu)
            {
                if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
                m_fusion = new ImGui::DoomScreen_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_amplitude, m_noise, m_frequency, m_dripScale, m_bars);
            im_RGB.time_stamp = mat_first.time_stamp;
            im_RGB.rate = mat_first.rate;
            im_RGB.flags = mat_first.flags;
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
        bool changed = false;
        float _amplitude = m_amplitude;
        float _noise = m_noise;
        float _frequency = m_frequency;
        float _dripScale = m_dripScale;
        int _bars = m_bars;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Amplitude##DoomScreen", &_amplitude, 0.1, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amplitude##DoomScreen")) { _amplitude = 2.f; }
        ImGui::SliderFloat("Noise##DoomScreen", &_noise, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_noise##DoomScreen")) { _noise = 0.1f; }
        ImGui::SliderFloat("Frequency##DoomScreen", &_frequency, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_frequency##DoomScreen")) { _frequency = 0.5f; }
        ImGui::SliderFloat("DripScale##DoomScreen", &_dripScale, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_dripScale##DoomScreen")) { _dripScale = 0.5f; }
        ImGui::SliderInt("Bars##DoomScreen", &_bars, 1, 100, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_bars##DoomScreen")) { _bars = 30; }
        ImGui::PopItemWidth();
        if (_amplitude != m_amplitude) { m_amplitude = _amplitude; changed = true; }
        if (_noise != m_noise) { m_noise = _noise; changed = true; }
        if (_frequency != m_frequency) { m_frequency = _frequency; changed = true; }
        if (_dripScale != m_dripScale) { m_dripScale = _dripScale; changed = true; }
        if (_bars != m_bars) { m_bars = _bars; changed = true; }
        return m_Enabled ? changed : false;
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
        if (value.contains("amplitude"))
        {
            auto& val = value["amplitude"];
            if (val.is_number()) 
                m_amplitude = val.get<imgui_json::number>();
        }
        if (value.contains("noise"))
        {
            auto& val = value["noise"];
            if (val.is_number()) 
                m_noise = val.get<imgui_json::number>();
        }
        if (value.contains("frequency"))
        {
            auto& val = value["frequency"];
            if (val.is_number()) 
                m_frequency = val.get<imgui_json::number>();
        }
        if (value.contains("dripScale"))
        {
            auto& val = value["dripScale"];
            if (val.is_number()) 
                m_dripScale = val.get<imgui_json::number>();
        }
        if (value.contains("bars"))
        {
            auto& val = value["bars"];
            if (val.is_number()) 
                m_bars = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["amplitude"] = imgui_json::number(m_amplitude);
        value["noise"] = imgui_json::number(m_noise);
        value["frequency"] = imgui_json::number(m_frequency);
        value["dripScale"] = imgui_json::number(m_dripScale);
        value["bars"] = imgui_json::number(m_bars);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        float font_size = ImGui::GetFontSize();
        ImGui::SetWindowFontScale((size.x - 16) / font_size);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
#if IMGUI_ICONS
        ImGui::Button((std::string(u8"\ue077") + "##" + std::to_string(m_ID)).c_str(), size);
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
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 2" };
    FloatPin  m_Pos         = { this, "Pos" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_amplitude   {2.f};
    float m_noise       {0.1f};
    float m_frequency   {0.5f};
    float m_dripScale   {0.5f};
    int m_bars          {30};
    ImGui::DoomScreen_vulkan * m_fusion   {nullptr};
};
} // namespace BluePrint
