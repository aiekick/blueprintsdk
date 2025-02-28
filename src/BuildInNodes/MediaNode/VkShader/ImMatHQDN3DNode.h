#include <UI.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include "HQDN3D_vulkan.h"

namespace BluePrint
{
struct HQDN3DNode final : Node
{
    BP_NODE_WITH_NAME(HQDN3DNode, "HQDN3D Denoise", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Denoise")
    HQDN3DNode(BP* blueprint): Node(blueprint) { m_Name = "HQDN3D Denoise"; }
    ~HQDN3DNode()
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
        if (m_LumSpatialIn.IsLinked()) m_lum_spac = context.GetPinValue<float>(m_LumSpatialIn);
        if (m_ChromaSpatialIn.IsLinked()) m_chrom_spac = context.GetPinValue<float>(m_ChromaSpatialIn);
        if (m_LumTemporalIn.IsLinked()) m_lum_tmp = context.GetPinValue<float>(m_LumTemporalIn);
        if (m_ChromaTemporalIn.IsLinked()) m_chrom_tmp = context.GetPinValue<float>(m_ChromaTemporalIn);
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
                m_filter = new ImGui::HQDN3D_vulkan(mat_in.w, mat_in.h, mat_in.c, gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            m_filter->SetParam(m_lum_spac, m_chrom_spac, m_lum_tmp, m_chrom_tmp);
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_NodeTimeMs = m_filter->filter(mat_in, im_RGB);
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    void WasUnlinked(const Pin& receiver, const Pin& provider) override
    {
        if (receiver.m_ID == m_LumSpatialIn.m_ID) m_LumSpatialIn.SetValue(m_lum_spac);
        if (receiver.m_ID == m_ChromaSpatialIn.m_ID) m_ChromaSpatialIn.SetValue(m_chrom_spac);
        if (receiver.m_ID == m_LumTemporalIn.m_ID) m_LumTemporalIn.SetValue(m_lum_tmp);
        if (receiver.m_ID == m_ChromaTemporalIn.m_ID) m_ChromaTemporalIn.SetValue(m_chrom_tmp);
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
        float _lum_spac = m_lum_spac;
        float _chrom_spac = m_chrom_spac;
        float _lum_tmp = m_lum_tmp;
        float _chrom_tmp = m_chrom_tmp;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(160, 8));
        ImGui::PushItemWidth(160);
        ImGui::BeginDisabled(!m_Enabled || m_LumSpatialIn.IsLinked());
        ImGui::SliderFloat("Luma Spatial##HQDN3D", &_lum_spac, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_luma_spatial##HQDN3D")) { _lum_spac = 6.f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_luma_spatial##HQDN3D", key, "luma spatial##HQDN3D", 0.f, 50.f, 6.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_ChromaSpatialIn.IsLinked());
        ImGui::SliderFloat("Chroma Spatial##HQDN3D", &_chrom_spac, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_chroma_spatial##HQDN3D")) { _chrom_spac = 4.f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_chroma_spatial##HQDN3D", key, "chroma spatial##HQDN3D", 0.f, 50.f, 4.f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_LumTemporalIn.IsLinked());
        ImGui::SliderFloat("Luma Temporal##HQDN3D", &_lum_tmp, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_luma_temporal##HQDN3D")) { _lum_tmp = 4.5f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_luma_temporal##HQDN3D", key, "luma temporal##HQDN3D", 0.f, 50.f, 4.5f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!m_Enabled || m_ChromaTemporalIn.IsLinked());
        ImGui::SliderFloat("Chroma Temporal##HQDN3D", &_chrom_tmp, 0, 50.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_chroma_temporal##HQDN3D")) { _chrom_tmp = 3.375f; changed = true; }
        if (key) ImGui::ImCurveEditKey("##add_curve_chroma_temporal##HQDN3D", key, "chroma temporal##HQDN3D", 0.f, 50.f, 3.375f);
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_lum_spac != m_lum_spac) { m_lum_spac = _lum_spac; changed = true; }
        if (_chrom_spac != m_chrom_spac) { m_chrom_spac = _chrom_spac; changed = true; }
        if (_lum_tmp != m_lum_tmp) { m_lum_tmp = _lum_tmp; changed = true; }
        if (_chrom_tmp != m_chrom_tmp) { m_chrom_tmp = _chrom_tmp; changed = true; }
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

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["lum_spac"] = imgui_json::number(m_lum_spac);
        value["chrom_spac"] = imgui_json::number(m_chrom_spac);
        value["lum_tmp"] = imgui_json::number(m_lum_tmp);
        value["chrom_tmp"] = imgui_json::number(m_chrom_tmp);
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
        ImGui::Button((std::string(u8"\ue3a4") + "##" + std::to_string(m_ID)).c_str(), size);
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
    FloatPin  m_LumSpatialIn = { this, "Lum spatial"};
    FloatPin  m_ChromaSpatialIn = { this, "Chroma spatial"};
    FloatPin  m_LumTemporalIn = { this, "Lum temporal"};
    FloatPin  m_ChromaTemporalIn = { this, "Chroma temporal"};
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[6] = { &m_Enter, &m_MatIn, &m_LumSpatialIn, &m_ChromaSpatialIn, &m_LumTemporalIn, &m_ChromaTemporalIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_lum_spac    {6.0};
    float m_chrom_spac  {4.0};
    float m_lum_tmp     {4.5};
    float m_chrom_tmp   {3.375};
    ImGui::HQDN3D_vulkan * m_filter   {nullptr};
};
} //namespace BluePrint
