#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Box.h>

namespace BluePrint
{
struct BoxBlurNode final : Node
{
    BP_NODE_WITH_NAME(BoxBlurNode, "Box Blur", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Blur")
    BoxBlurNode(BP* blueprint): Node(blueprint) { m_Name = "Box Blur"; }

    ~BoxBlurNode()
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
        if (m_SizeIn.IsLinked()) m_Size = context.GetPinValue<float>(m_SizeIn);
        if (m_IterationIn.IsLinked()) m_iteration = context.GetPinValue<float>(m_IterationIn);
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
                m_filter = new ImGui::BoxBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_Size, m_Size);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            double node_time = 0;
            node_time += m_filter->filter(mat_in, im_RGB);
            for (int i = 1; i < m_iteration; i++)
            {
                node_time += m_filter->filter(im_RGB, im_RGB);
            }
            m_NodeTimeMs = node_time;
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_SizeIn.m_ID) m_SizeIn.SetValue(m_Size);
        if (receiver.m_ID == m_IterationIn.m_ID) m_IterationIn.SetValue(m_iteration);
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

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origi, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        int _Size = m_Size;
        int _iteration = m_iteration;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_SizeIn.IsLinked());
        ImGui::SliderInt("Size##Box", &_Size, 1, 20, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_size##Box")) { _Size = 3; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_size##Box", key, "size##Box", 1.f, 20.f, 3.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_IterationIn.IsLinked());
        ImGui::SliderInt("Iteration##Box", &_iteration, 1, 20, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_iteration##Box")) { _iteration = 1; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_iteration##Box", key, "iteration##Box", 1.f, 20.f, 1.f);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_Size != m_Size) { m_Size = _Size; changed = true; }
        if (_iteration != m_iteration) { m_iteration = _iteration; changed = true; }
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
        if (value.contains("size"))
        {
            auto& val = value["size"];
            if (val.is_number()) 
                m_Size = val.get<imgui_json::number>();
        }

        if (value.contains("iteration"))
        {
            auto& val = value["iteration"];
            if (val.is_number()) 
                m_iteration = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["size"] = imgui_json::number(m_Size);
        value["iteration"] = imgui_json::number(m_iteration);
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
        ImGui::Button((std::string(u8"\ue3ec") + "##" + std::to_string(m_ID)).c_str(), size);
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
    FloatPin  m_SizeIn  = { this, "Size"};
    FloatPin  m_IterationIn = { this, "Iteration"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter,&m_MatIn, &m_SizeIn, &m_IterationIn };
    Pin* m_OutputPins[2] = { &m_Exit,&m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_Size          {3};
    int m_iteration     {1};
    ImGui::BoxBlur_vulkan * m_filter   {nullptr};
};
} // namespace BluePrint
