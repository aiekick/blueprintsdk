#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#include "HQDN3D_vulkan.h"

namespace BluePrint
{
struct HQDN3DNode final : Node
{
    BP_NODE(HQDN3DNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter")
    HQDN3DNode(BP& blueprint): Node(blueprint) { m_Name = "Mat HQDN3D"; }
    ~HQDN3DNode()
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
            if (!m_filter || 
                m_filter->in_width != mat_in.w || 
                m_filter->in_height != mat_in.h ||
                m_filter->in_channels != mat_in.c)
            {
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::HQDN3D_vulkan(mat_in.w, mat_in.h, mat_in.c, gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_filter->SetParam(m_lum_spac, m_chrom_spac, m_lum_tmp, m_chrom_tmp);
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
        float _lum_spac = m_lum_spac;
        float _chrom_spac = m_chrom_spac;
        float _lum_tmp = m_lum_tmp;
        float _chrom_tmp = m_chrom_tmp;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(300, 8));
        ImGui::PushItemWidth(300);
        if (ImGui::Checkbox("##enable_filter",&check)) { m_bEnabled = check; changed = true; }
        ImGui::SameLine(); ImGui::TextUnformatted("HQDN3D");
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        ImGui::SliderFloat("Luma Spatial", &_lum_spac, 0, 50.f, "%.1f", flags);
        ImGui::SliderFloat("Chroma Spatial", &_chrom_spac, 0, 50.f, "%.1f", flags);
        ImGui::SliderFloat("Luma Temporal", &_lum_tmp, 0, 50.f, "%.1f", flags);
        ImGui::SliderFloat("Chroma Temporal", &_chrom_tmp, 0, 50.f, "%.1f", flags);
        ImGui::PopItemWidth();
        if (_lum_spac != m_lum_spac) { m_lum_spac = _lum_spac; changed = true; }
        if (_chrom_spac != m_chrom_spac) { m_chrom_spac = _chrom_spac; changed = true; }
        if (_lum_tmp != m_lum_tmp) { m_lum_tmp = _lum_tmp; changed = true; }
        if (_chrom_tmp != m_chrom_tmp) { m_chrom_tmp = _chrom_tmp; changed = true; }
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
        if (value.contains("lum_spac"))
        {
            auto& val = value["lum_spac"];
            if (val.is_number()) 
                m_lum_spac = val.get<imgui_json::number>();
        }
        if (value.contains("chrom_spac"))
        {
            auto& val = value["chrom_spac"];
            if (val.is_number()) 
                m_chrom_spac = val.get<imgui_json::number>();
        }
        if (value.contains("lum_tmp"))
        {
            auto& val = value["lum_tmp"];
            if (val.is_number()) 
                m_lum_tmp = val.get<imgui_json::number>();
        }
        if (value.contains("chrom_tmp"))
        {
            auto& val = value["chrom_tmp"];
            if (val.is_number()) 
                m_chrom_tmp = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        value["lum_spac"] = imgui_json::number(m_lum_spac);
        value["chrom_spac"] = imgui_json::number(m_chrom_spac);
        value["lum_tmp"] = imgui_json::number(m_lum_tmp);
        value["chrom_tmp"] = imgui_json::number(m_chrom_tmp);
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
    float m_lum_spac    {6.0};
    float m_chrom_spac  {4.0};
    float m_lum_tmp     {4.5};
    float m_chrom_tmp   {3.375};
    ImGui::HQDN3D_vulkan * m_filter   {nullptr};
};
} //namespace BluePrint
#endif // IMGUI_VULKAN_SHADER