#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#if IMGUI_VULKAN_SHADER
#include <imgui_logger.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <GaussianBlur.h>

namespace BluePrint
{
struct GaussianBlurNode final : Node
{
    BP_NODE(GaussianBlurNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter")
    GaussianBlurNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Image Gaussian Blur"; }

    ~GaussianBlurNode()
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
            if (!m_bEnabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter)
            {
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::GaussianBlur_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_filter->SetParam(m_blurRadius, m_sigma);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            if (mat_in.device == IM_DD_VULKAN)
            {
                ImGui::VkMat in_RGB = mat_in;
                m_filter->filter(in_RGB, im_RGB);
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                m_MatOut.SetValue(im_RGB);
            }
            else if (mat_in.device == IM_DD_CPU)
            {
                m_filter->filter(mat_in, im_RGB);
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
        float _sigma = m_sigma;
        int _blurRadius = m_blurRadius;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(300, 8));
        ImGui::PushItemWidth(300);
        if (ImGui::Checkbox("##enable_filter",&check)) { m_bEnabled = check; changed = true; }
        ImGui::SameLine(); ImGui::TextUnformatted("GaussianBlur");
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        ImGui::SliderFloat("Sigma", &_sigma, 0.0, 10.f, "%.1f", flags);
        ImGui::SliderInt("Radius", &_blurRadius, 0, 20, "%d", flags);
        ImGui::PopItemWidth();
        if (_sigma != m_sigma) { m_sigma = _sigma; changed = true; }
        if (_blurRadius != m_blurRadius) { m_blurRadius = _blurRadius; changed = true; }
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
        if (value.contains("radius"))
        {
            auto& val = value["radius"];
            if (val.is_number()) 
                m_blurRadius = val.get<imgui_json::number>();
        }

        if (value.contains("sigma"))
        {
            auto& val = value["sigma"];
            if (val.is_number()) 
                m_sigma = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        value["radius"] = imgui_json::number(m_blurRadius);
        value["sigma"] = imgui_json::number(m_sigma);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    bool m_bEnabled      {true};
    ImGui::GaussianBlur_vulkan * m_filter   {nullptr};
    int m_blurRadius {3};
    float m_sigma {0.0f};
};
} // namespace BluePrint
#endif // IMGUI_VULKAN_SHADER
