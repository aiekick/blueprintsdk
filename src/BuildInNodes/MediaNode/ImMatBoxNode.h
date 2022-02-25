#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#if IMGUI_VULKAN_SHADER
#include <imgui_logger.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>

#include <Box.h>

namespace BluePrint
{
struct BoxBlurNode final : Node
{
    BP_NODE_WITH_NAME(BoxBlurNode, "Box Blur", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Blur")
    BoxBlurNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Box Blur"; }

    ~BoxBlurNode()
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
                m_filter = new ImGui::BoxBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_Size, m_Size);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            if (mat_in.device == IM_DD_VULKAN)
            {
                ImGui::VkMat in_RGB = mat_in;
                m_filter->filter(in_RGB, im_RGB);
                for (int i = 1; i < m_iteration; i++)
                {
                    m_filter->filter(im_RGB, im_RGB);
                }
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                m_MatOut.SetValue(im_RGB);
            }
            else if (mat_in.device == IM_DD_CPU)
            {
                m_filter->filter(mat_in, im_RGB);
                for (int i = 1; i < m_iteration; i++)
                {
                    m_filter->filter(im_RGB, im_RGB);
                }
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                m_MatOut.SetValue(im_RGB);
            }
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
        bool changed = false;
        bool check = m_bEnabled;
        int _Size = m_Size;
        int _iteration = m_iteration;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        if (ImGui::Checkbox("##enable_filter_Box",&check)) { m_bEnabled = check; changed = true; }
        ImGui::SameLine(); ImGui::TextUnformatted("Box");
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        ImGui::SliderInt("Size##Box", &_Size, 1, 20, "%d", flags);
        ImGui::SliderInt("Iteration##Box", &_iteration, 1, 20, "%d", flags);
        ImGui::PopItemWidth();
        if (_Size != m_Size) { m_Size = _Size; changed = true; }
        if (_iteration != m_iteration) { m_iteration = _iteration; changed = true; }
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

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        value["size"] = imgui_json::number(m_Size);
        value["iteration"] = imgui_json::number(m_iteration);
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

    Pin* m_InputPins[2] = { &m_Enter,&m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit,&m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    bool m_bEnabled     {true};
    int m_Size          {3};
    int m_iteration     {1};
    ImGui::BoxBlur_vulkan * m_filter   {nullptr};
};
} // namespace BluePrint
#endif // IMGUI_VULKAN_SHADER