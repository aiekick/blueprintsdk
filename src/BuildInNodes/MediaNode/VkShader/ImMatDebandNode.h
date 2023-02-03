#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "DeBand_vulkan.h"

namespace BluePrint
{
struct DeBandNode final : Node
{
    BP_NODE_WITH_NAME(DeBandNode, "Deband", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Enhance")
    DeBandNode(BP* blueprint): Node(blueprint) { m_Name = "DeBand"; }
    ~DeBandNode()
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
        if (m_ThresholdIn.IsLinked()) m_threshold = context.GetPinValue<float>(m_ThresholdIn);
        if (m_RangeIn.IsLinked()) m_range = context.GetPinValue<float>(m_RangeIn);
        if (m_DirectionIn.IsLinked()) m_direction = context.GetPinValue<float>(m_DirectionIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device ||
                m_filter->in_width != mat_in.w || 
                m_filter->in_height != mat_in.h ||
                m_filter->in_channels != mat_in.c)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::DeBand_vulkan(mat_in.w, mat_in.h, mat_in.c, gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_range, m_direction * M_PI);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB, m_threshold, m_blur);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_ThresholdIn.m_ID) m_ThresholdIn.SetValue(m_threshold);
        if (receiver.m_ID == m_RangeIn.m_ID) m_RangeIn.SetValue(m_range);
        if (receiver.m_ID == m_DirectionIn.m_ID) m_DirectionIn.SetValue(m_direction);
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
        float _threshold = m_threshold;
        int _range = m_range;
        float _direction = m_direction;
        bool _blur = m_blur;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled || m_ThresholdIn.IsLinked());
        ImGui::SliderFloat("Threshold##DeBand", &_threshold, 0, 0.05f, "%.3f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_threshold##DeBand")) { _threshold = 0.05f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_threshold##DeBand", key, "threshold##DeBand", 0.f, 0.05f, 0.01f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_RangeIn.IsLinked());
        ImGui::SliderInt("Range##DeBand", &_range, 0, 64, "%.d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_range##DeBand")) { _range = 16.f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_range##DeBand", key, "range##DeBand", 0.f, 64.f, 16.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_DirectionIn.IsLinked());
        ImGui::SliderFloat("Direction##DeBand", &_direction, 0.f, 4.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_direction##DeBand")) { _direction = 2.f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_direction##DeBand", key, "direction##DeBand", 0.f, 4.f, 2.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::TextUnformatted("Blur:");ImGui::SameLine();
        ImGui::ToggleButton("##Blur##DeBand",&_blur);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_threshold != m_threshold) { m_threshold = _threshold; changed = true; }
        if (_range != m_range) { m_range = _range; changed = true; }
        if (_direction != m_direction) { m_direction = _direction; changed = true; }
        if (_blur != m_blur) { m_blur = _blur; changed = true; }
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
        if (value.contains("threshold"))
        {
            auto& val = value["threshold"];
            if (val.is_number()) 
                m_threshold = val.get<imgui_json::number>();
        }
        if (value.contains("range"))
        {
            auto& val = value["range"];
            if (val.is_number()) 
                m_range = val.get<imgui_json::number>();
        }
        if (value.contains("direction"))
        {
            auto& val = value["direction"];
            if (val.is_number()) 
                m_direction = val.get<imgui_json::number>();
        }
        if (value.contains("blur"))
        {
            auto& val = value["blur"];
            if (val.is_boolean()) 
                m_blur = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["threshold"] = imgui_json::number(m_threshold);
        value["range"] = imgui_json::number(m_range);
        value["direction"] = imgui_json::number(m_direction);
        value["blur"] = m_blur;
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
        ImGui::Button((std::string(u8"\uf75b") + "##" + std::to_string(m_ID)).c_str(), size);
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
    FloatPin  m_ThresholdIn = { this, "Threshold"};
    FloatPin  m_RangeIn = { this, "Range"};
    FloatPin  m_DirectionIn = { this, "Direction"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatIn, &m_ThresholdIn, &m_RangeIn, &m_DirectionIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device            {-1};
    float m_threshold       {0.01};
    int m_range             {16};
    float m_direction       {2};
    bool m_blur             {false};
    ImGui::DeBand_vulkan *  m_filter {nullptr};
};
} //namespace BluePrint
