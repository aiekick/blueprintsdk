#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#if IMGUI_VULKAN_SHADER
#include <ImVulkanShader.h>
#endif
#include "Lut3D.h"
#if IMGUI_ICONS
#include <icons.h>
#endif
#define USE_BOOKMARK
#include <ImGuiFileDialog.h>

namespace BluePrint
{
struct Lut3DNode final : Node
{
    BP_NODE(Lut3DNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter")
    Lut3DNode(BP& blueprint): Node(blueprint) { m_Name = "Lut3D"; }
    ~Lut3DNode()
    {
        if (m_filter) { delete m_filter; m_filter = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_filter && m_setting_changed) { delete m_filter; m_filter = nullptr; }
        m_setting_changed = false;
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        if (entryPoint.m_ID == m_IReset.m_ID)
        {
            Reset(context);
            return m_OReset;
        }
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            if (!m_bEnabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || m_setting_changed)
            {
#if IMGUI_VULKAN_SHADER
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
#else
                int gpu = -1;
#endif
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                if (m_lut_mode != NO_DEFAULT)
                    m_filter = new ImGui::LUT3D_vulkan(m_lut_mode, m_interpolation_mode, gpu);
                else if (!m_path.empty())
                    m_filter = new ImGui::LUT3D_vulkan(m_path, m_interpolation_mode, gpu);
                m_setting_changed = false;
                return m_OReset;
            }
            if (!m_filter)
            {
                return {};
            }
            bool is_hdr_pq = m_bEnabled && ((m_lut_mode == SDR709_HDRPQ) || (m_lut_mode == HDRHLG_HDRPQ));
            bool is_hdr_hlg = m_bEnabled && ((m_lut_mode == SDR709_HDRHLG) || (m_lut_mode == HDRPQ_HDRHLG));
#if IMGUI_VULKAN_SHADER
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            if (mat_in.device == IM_DD_VULKAN)
            {
                ImGui::VkMat in_RGB = mat_in;
                m_filter->filter(in_RGB, im_RGB);
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                if (is_hdr_pq) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_PQ;
                if (is_hdr_hlg) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_HLG;
                m_MatOut.SetValue(im_RGB);
            }
            else if (mat_in.device == IM_DD_CPU)
            {
                m_filter->filter(mat_in, im_RGB);
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                if (is_hdr_pq) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_PQ;
                if (is_hdr_hlg) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_HLG;
                m_MatOut.SetValue(im_RGB);
            }
#else
            ImGui::ImMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            if (mat_in.device == IM_DD_VULKAN)
            {
                return {};
            }
            else if (mat_in.device == IM_DD_CPU)
            {
                m_filter->filter(mat_in, im_RGB);
                im_RGB.time_stamp = mat_in.time_stamp;
                im_RGB.rate = mat_in.rate;
                im_RGB.flags = mat_in.flags;
                if (is_hdr_pq) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_PQ;
                if (is_hdr_hlg) im_RGB.flags |= IM_MAT_FLAGS_VIDEO_HDR_HLG;
                m_MatOut.SetValue(im_RGB);
            }
#endif
        }
        return m_Exit;
    }

    void DrawSettingLayout(ImGuiContext * ctx) override
    {
        // Draw Setting
        int lut_mode = m_lut_mode;
        Node::DrawSettingLayout(ctx);
        ImGui::Separator();
        ImGui::RadioButton("SDR->HLG",  (int *)&lut_mode, SDR709_HDRHLG); ImGui::SameLine();
        ImGui::RadioButton("SDR->PQ",   (int *)&lut_mode, SDR709_HDRPQ); ImGui::SameLine();
        ImGui::RadioButton("HLG->SDR",  (int *)&lut_mode, HDRHLG_SDR709); ImGui::SameLine();
        ImGui::RadioButton("PQ->SDR",   (int *)&lut_mode, HDRPQ_SDR709); ImGui::SameLine();
        ImGui::RadioButton("HLG->PQ",   (int *)&lut_mode, HDRHLG_HDRPQ); ImGui::SameLine();
        ImGui::RadioButton("PQ->HLG",   (int *)&lut_mode, HDRPQ_HDRHLG); ImGui::SameLine();
        ImGui::RadioButton("File",      (int *)&lut_mode, NO_DEFAULT);
        ImGui::Separator();
        ImGui::RadioButton("Nearest",       (int *)&m_interpolation_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        ImGui::RadioButton("Trilinear",     (int *)&m_interpolation_mode, IM_INTERPOLATE_TRILINEAR); ImGui::SameLine();
        ImGui::RadioButton("Teteahedral",   (int *)&m_interpolation_mode, IM_INTERPOLATE_TETRAHEDRAL);
        ImGui::Separator();
        ImGui::TextUnformatted("Mat Type:"); ImGui::SameLine();
        ImGui::RadioButton("AsInput", (int *)&m_mat_data_type, (int)IM_DT_UNDEFINED); ImGui::SameLine();
        ImGui::RadioButton("Int8", (int *)&m_mat_data_type, (int)IM_DT_INT8); ImGui::SameLine();
        ImGui::RadioButton("Int16", (int *)&m_mat_data_type, (int)IM_DT_INT16); ImGui::SameLine();
        ImGui::RadioButton("Float16", (int *)&m_mat_data_type, (int)IM_DT_FLOAT16); ImGui::SameLine();
        ImGui::RadioButton("Float32", (int *)&m_mat_data_type, (int)IM_DT_FLOAT32);
        ImGui::Separator();
        if (m_lut_mode != lut_mode)
        {
            m_lut_mode = lut_mode;
            m_setting_changed = true;
        }
        // open from file
        if (m_lut_mode == NO_DEFAULT) ImGui::BeginDisabled(false);  else ImGui::BeginDisabled(true);
        static string filters = ".cube";
        ImGuiFileDialogFlags vflags = 0;
        vflags |= ImGuiFileDialogFlags_DontShowHiddenFiles;
        if (ImGui::Button(ICON_IGFD_FOLDER_OPEN " Choose File ## "))
            ImGuiFileDialog::Instance()->OpenModal("##NodeChooseLutFileDlgKey", "Choose File", 
                                                    filters.c_str(), 
                                                    m_path.empty() ? "." : m_path,
                                                    1, this, vflags);
        if (ImGuiFileDialog::Instance()->Display("##NodeChooseLutFileDlgKey"))
        {
	        // action if OK
            if (ImGuiFileDialog::Instance()->IsOk() == true)
            {
                m_path = ImGuiFileDialog::Instance()->GetFilePathName();
                m_file_name = ImGuiFileDialog::Instance()->GetCurrentFileName();
                m_setting_changed = true;
            }
            // close
            ImGuiFileDialog::Instance()->Close();
        }
        if (m_lut_mode != NO_DEFAULT)
        {
            m_path.clear();
            m_file_name.clear();
        }
        ImGui::SameLine(0);
        ImGui::TextUnformatted(m_file_name.c_str());
        ImGui::EndDisabled();
    }

    bool CustomLayout() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin) override
    {
        ImGui::SetCurrentContext(ctx);
        // Draw custom layout
        bool changed = false;
        bool check = m_bEnabled;
        int lut_mode = m_lut_mode;
        if (ImGui::Checkbox("##enable_filter",&check)) { m_bEnabled = check; changed = true; }
        ImGui::SameLine(0); ImGui::SameLine(); ImGui::TextUnformatted("Lut3D");
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);

        if (!m_file_name.empty())
            ImGui::Text("Lut from file: %s", m_file_name.c_str());
        else
        {
            switch (m_lut_mode)
            {
                case SDR709_HDRHLG: ImGui::TextUnformatted("SDR to HLG"); break;
                case SDR709_HDRPQ: ImGui::TextUnformatted("SDR to PQ"); break;
                case HDRHLG_SDR709: ImGui::TextUnformatted("HLG to SDR"); break;
                case HDRPQ_SDR709: ImGui::TextUnformatted("PQ to SDR"); break;
                case HDRHLG_HDRPQ: ImGui::TextUnformatted("HLG to PQ"); break;
                case HDRPQ_HDRHLG: ImGui::TextUnformatted("PQ to HDR"); break;
                default: ImGui::TextUnformatted("Unknown Lut"); break;
            }
        }

        switch (m_interpolation_mode)
        {
            case IM_INTERPOLATE_NEAREST: ImGui::TextUnformatted("Interpolate Nearest"); break;
            case IM_INTERPOLATE_TRILINEAR: ImGui::TextUnformatted("Interpolate Trilinear"); break;
            case IM_INTERPOLATE_TETRAHEDRAL: ImGui::TextUnformatted("Interpolate Tetrahedral"); break;
            default: ImGui::TextUnformatted("Unknown Interpolate"); break;
        }   

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
        if (value.contains("lut_mode"))
        {
            auto& val = value["lut_mode"];
            if (val.is_number()) 
                m_lut_mode = val.get<imgui_json::number>();
        }
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = val.get<imgui_json::number>();
        }
        if (value.contains("lut_file_path"))
        {
            auto& val = value["lut_file_path"];
            if (val.is_string())
            {
                m_path = val.get<imgui_json::string>();
            }
        }
        if (value.contains("lut_file_name"))
        {
            auto& val = value["lut_file_name"];
            if (val.is_string())
            {
                m_file_name = val.get<imgui_json::string>();
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        value["lut_mode"] = imgui_json::number(m_lut_mode);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
        value["lut_file_path"] = m_path;
        value["lut_file_name"] = m_file_name;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_Enter   = { this, "Enter" };
    
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    string  m_path;
    string m_file_name;
    bool m_needReload {false};
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    bool m_bEnabled      {true};
    int m_lut_mode {SDR709_HDRHLG};
    int m_interpolation_mode {IM_INTERPOLATE_TRILINEAR};

    ImGui::LUT3D_vulkan * m_filter {nullptr};
    bool  m_setting_changed {false};
};
} // namespace BluePrint
