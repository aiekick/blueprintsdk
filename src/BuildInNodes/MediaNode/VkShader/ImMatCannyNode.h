#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Canny_vulkan.h>

namespace BluePrint
{
struct CannyNode final : Node
{
    BP_NODE_WITH_NAME(CannyNode, "Canny Edge", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Edge")
    CannyNode(BP* blueprint): Node(blueprint) { m_Name = "Canny Edge"; }
    ~CannyNode()
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
        if (m_RadiusIn.IsLinked()) m_blurRadius = context.GetPinValue<float>(m_RadiusIn);
        if (m_MinIn.IsLinked()) m_minThreshold = context.GetPinValue<float>(m_MinIn);
        if (m_MaxIn.IsLinked()) m_maxThreshold = context.GetPinValue<float>(m_MaxIn);
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
                m_filter = new ImGui::Canny_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_blurRadius, m_minThreshold, m_maxThreshold);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_RadiusIn.m_ID) m_RadiusIn.SetValue(m_blurRadius);
        if (receiver.m_ID == m_MinIn.m_ID) m_MinIn.SetValue(m_minThreshold);
        if (receiver.m_ID == m_MinIn.m_ID) m_MaxIn.SetValue(m_maxThreshold);
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
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        int _blurRadius = m_blurRadius;
        float _minThreshold = m_minThreshold;
        float _maxThreshold = m_maxThreshold;
        ImGui::Dummy(ImVec2(160, 8));
        ImGui::PushItemWidth(160);
        ImGui::BeginDisabled(!m_Enabled || m_RadiusIn.IsLinked());
        ImGui::SliderInt("Blur Radius##Canny", &_blurRadius, 0, 10, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_radius##Canny")) { _blurRadius = 3; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_radius##Canny", key, "radius##Canny", 0.f, 10.f, 3.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_MinIn.IsLinked());
        ImGui::SliderFloat("Min Threshold##Canny", &_minThreshold, 0, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_min##Canny")) { _minThreshold = 0.1; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_min##Canny", key, "min##Canny", 0.f, 1.f, 0.1f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_MaxIn.IsLinked());
        ImGui::SliderFloat("Max Threshold##Canny", &_maxThreshold, _minThreshold, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_max##Canny")) { _maxThreshold = 0.45; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_max##Canny", key, "max##Canny", 0.f, 1.f, 0.45f);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (m_blurRadius != _blurRadius) { m_blurRadius = _blurRadius; changed = true; }
        if (m_minThreshold != _minThreshold) { m_minThreshold = _minThreshold; changed = true; }
        if (m_maxThreshold != _maxThreshold) { m_maxThreshold = _maxThreshold; changed= true; }
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
        if (value.contains("Radius"))
        {
            auto& val = value["Radius"];
            if (val.is_number()) 
                m_blurRadius = val.get<imgui_json::number>();
        }
        if (value.contains("minThreshold"))
        {
            auto& val = value["minThreshold"];
            if (val.is_number()) 
                m_minThreshold = val.get<imgui_json::number>();
        }
        if (value.contains("maxThreshold"))
        {
            auto& val = value["maxThreshold"];
            if (val.is_number()) 
                m_maxThreshold = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["Radius"] = imgui_json::number(m_blurRadius);
        value["minThreshold"] = imgui_json::number(m_minThreshold);
        value["maxThreshold"] = imgui_json::number(m_maxThreshold);
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
        ImGui::Button((std::string(u8"\ue155") + "##" + std::to_string(m_ID)).c_str(), size);
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
    FloatPin  m_RadiusIn = { this, "Radius" };
    FloatPin  m_MinIn = { this, "Min Threshold" };
    FloatPin  m_MaxIn = { this, "Max Threshold" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_RadiusIn, &m_MinIn, &m_MaxIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_minThreshold    {0.1};
    float m_maxThreshold    {0.45};
    int m_blurRadius        {3};
    ImGui::Canny_vulkan * m_filter {nullptr};
};
} //namespace BluePrint
