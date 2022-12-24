#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <CopyTo_vulkan.h>

typedef enum Slider_Type : int32_t
{
    SLIDER_RIGHT = 0,
    SLIDER_LEFT,
    SLIDER_BOTTOM,
    SLIDER_TOP,
    SLIDER_RIGHT_BOTTOM,
    SLIDER_LEFT_TOP,
    SLIDER_RIGHT_TOP,
    SLIDER_LEFT_BOTTOM,
} Slider_Type;

namespace BluePrint
{
struct SliderFusionNode final : Node
{
    BP_NODE_WITH_NAME(SliderFusionNode, "Slider Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    SliderFusionNode(BP& blueprint): Node(blueprint) { m_Name = "Slider Transform"; }

    ~SliderFusionNode()
    {
        if (m_copy) { delete m_copy; m_copy = nullptr; }
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
        int x = 0, y = 0;
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float percentage = 1.0f - context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            switch (m_slider_type)
            {
                case SLIDER_RIGHT :
                    x = (1.0 - percentage) * mat_first.w;
                break;
                case SLIDER_LEFT:
                    x = - (1.0 - percentage) * mat_first.w;
                break;
                case SLIDER_BOTTOM:
                    y = (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_TOP:
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_RIGHT_BOTTOM:
                    x = (1.0 - percentage) * mat_first.w;
                    y = (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_LEFT_TOP:
                    x = - (1.0 - percentage) * mat_first.w;
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_RIGHT_TOP:
                    x = (1.0 - percentage) * mat_first.w;
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_LEFT_BOTTOM:
                    x = - (1.0 - percentage) * mat_first.w;
                    y = (1.0 - percentage) * mat_first.h;
                break;
                default: break;
            }
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_copy || m_device != gpu)
            {
                if (m_copy) { delete m_copy; m_copy = nullptr; }
                m_copy = new ImGui::CopyTo_vulkan(gpu);
            }
            if (!m_copy)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            double node_time = 0;
            node_time += m_copy->copyTo(mat_first, im_RGB, 0, 0);
            node_time += m_copy->copyTo(mat_second, im_RGB, x, y);
            m_NodeTimeMs = node_time;
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
        int type = m_slider_type;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        //ImGui::Combo("Type:", &type, items, IM_ARRAYSIZE(items));
        ImGui::RadioButton("Right In", &type, SLIDER_RIGHT);
        ImGui::RadioButton("Left In", &type, SLIDER_LEFT);
        ImGui::RadioButton("Bottom In", &type, SLIDER_BOTTOM);
        ImGui::RadioButton("Top In", &type, SLIDER_TOP);
        ImGui::RadioButton("Right Bottom In", &type, SLIDER_RIGHT_BOTTOM);
        ImGui::RadioButton("Left Top In", &type, SLIDER_LEFT_TOP);
        ImGui::RadioButton("Right Top In", &type, SLIDER_RIGHT_TOP);
        ImGui::RadioButton("Left Bottom In", &type, SLIDER_LEFT_BOTTOM);
        if (type != m_slider_type) { m_slider_type = type; changed = true; }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
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
        if (value.contains("slider_type"))
        { 
            auto& val = value["slider_type"];
            if (val.is_number())
                m_slider_type = (Slider_Type)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["slider_type"] = imgui_json::number(m_slider_type);
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
        ImGui::Button((std::string(u8"\ue882") + "##" + std::to_string(m_ID)).c_str(), size);
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

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatInFirst   = { this, "In 1" };
    MatPin    m_MatInSecond   = { this, "In 2" };
    FloatPin  m_Pos = { this, "Pos" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_slider_type   {SLIDER_RIGHT};
    ImGui::CopyTo_vulkan * m_copy   {nullptr};
};
} // namespace BluePrint
