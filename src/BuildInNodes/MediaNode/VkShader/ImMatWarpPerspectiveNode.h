#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <warpAffine_vulkan.h>
#include <warpPerspective_vulkan.h>

namespace BluePrint
{
struct MatWarpPerspectiveNode final : Node
{
    BP_NODE_WITH_NAME(MatWarpPerspectiveNode, "Mat Warp Perspective", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Media")
    MatWarpPerspectiveNode(BP* blueprint): Node(blueprint) {
        m_Name = "Mat Warp Perspective";
        m_matrix.create_type(3, 3, IM_DT_FLOAT32);
    }

    ~MatWarpPerspectiveNode()
    {
        if (m_transform) { delete m_transform; m_transform = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_transform) { delete m_transform; m_transform = nullptr; }
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
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_transform)
            {
                int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
                m_transform = new ImGui::warpPerspective_vulkan(gpu);
                if (!m_transform)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            ImVec2 src_corners[4];
            ImVec2 dst_corners[4];
            dst_corners[0] = ImVec2(0, 0);
            dst_corners[1] = ImVec2(mat_in.w, 0);
            dst_corners[2] = ImVec2(mat_in.w, mat_in.h);
            dst_corners[3] = ImVec2(0, mat_in.h);
            src_corners[0] = m_warp_tl * ImVec2(mat_in.w, mat_in.h);
            src_corners[1] = m_warp_tr * ImVec2(mat_in.w, mat_in.h);
            src_corners[2] = m_warp_br * ImVec2(mat_in.w, mat_in.h);
            src_corners[3] = m_warp_bl * ImVec2(mat_in.w, mat_in.h);
            m_matrix = ImGui::getPerspectiveTransform(src_corners, dst_corners);
            m_NodeTimeMs = m_transform->warp(mat_in, im_RGB, m_matrix, m_interpolation_mode, ImPixel(0, 0, 0, 0));
            m_MatOut.SetValue(im_RGB);
        }
        return m_Exit;
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        ImVec2 _warp_tl = ImVec2(m_warp_tl.x, 1 - m_warp_tl.y);
        ImVec2 _warp_tr = ImVec2(m_warp_tr.x, 1 - m_warp_tr.y);
        ImVec2 _warp_br = ImVec2(m_warp_br.x, 1 - m_warp_br.y);
        ImVec2 _warp_bl = ImVec2(m_warp_bl.x, 1 - m_warp_bl.y);
        ImInterpolateMode _mode = m_interpolation_mode;
        const ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        const ImVec2 v_size = ImVec2(18, 100);
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(120);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderFloat("##TL x", &_warp_tl.x, 0.f, 1.f, "", flags);
        ImGui::SameLine();
        ImGui::SliderFloat("##TR x", &_warp_tr.x, 0.f, 1.f, "", flags);
        ImGui::VSliderFloat("##TL y", v_size, &_warp_tl.y, 0.f, 1.f, "", flags);
        ImGui::SameLine(230);
        ImGui::VSliderFloat("##TR y", v_size, &_warp_tr.y, 0.f, 1.f, "", flags);
        ImGui::VSliderFloat("##BL y", v_size, &_warp_bl.y, 0.f, 1.f, "", flags);
        ImGui::SameLine(230);
        ImGui::VSliderFloat("##BR y", v_size, &_warp_br.y, 0.f, 1.f, "", flags);
        ImGui::SliderFloat("##BL x", &_warp_bl.x, 0.f, 1.f, "", flags);
        ImGui::SameLine();
        ImGui::SliderFloat("##BR x", &_warp_br.x, 0.f, 1.f, "", flags);
        if (ImGui::Button(ICON_RESET "##reset_bly##Warp"))
        { 
            _warp_tl = ImVec2(0, 1);
            _warp_tr = ImVec2(1, 1);
            _warp_br = ImVec2(1, 0);
            _warp_bl = ImVec2(0, 0);
            changed = true;
        }
        ImGui::RadioButton("Nearest",   (int *)&_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        ImGui::RadioButton("Bilinear",  (int *)&_mode, IM_INTERPOLATE_BILINEAR); ImGui::SameLine();
        ImGui::RadioButton("Bicubic",   (int *)&_mode, IM_INTERPOLATE_BICUBIC); ImGui::SameLine();

        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        _warp_tl = ImVec2(_warp_tl.x, 1 - _warp_tl.y);
        _warp_tr = ImVec2(_warp_tr.x, 1 - _warp_tr.y);
        _warp_br = ImVec2(_warp_br.x, 1 - _warp_br.y);
        _warp_bl = ImVec2(_warp_bl.x, 1 - _warp_bl.y);
        if (_warp_tl != m_warp_tl) { m_warp_tl = _warp_tl; changed = true; }
        if (_warp_tr != m_warp_tr) { m_warp_tr = _warp_tr; changed = true; }
        if (_warp_br != m_warp_br) { m_warp_br = _warp_br; changed = true; }
        if (_warp_bl != m_warp_bl) { m_warp_bl = _warp_bl; changed = true; }
        if (_mode != m_interpolation_mode) { m_interpolation_mode = _mode; changed = true; }
        return changed;
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
        if (value.contains("warp_tl"))
        {
            auto& val = value["warp_tl"];
            if (val.is_vec2()) 
                m_warp_tl = val.get<imgui_json::vec2>();
        }
        if (value.contains("warp_tr"))
        {
            auto& val = value["warp_tr"];
            if (val.is_vec2()) 
                m_warp_tr = val.get<imgui_json::vec2>();
        }
        if (value.contains("warp_br"))
        {
            auto& val = value["warp_br"];
            if (val.is_vec2()) 
                m_warp_br = val.get<imgui_json::vec2>();
        }
        if (value.contains("warp_bl"))
        {
            auto& val = value["warp_bl"];
            if (val.is_vec2()) 
                m_warp_bl = val.get<imgui_json::vec2>();
        }
        if (value.contains("interpolation"))
        {
            auto& val = value["interpolation"];
            if (val.is_number()) 
                m_interpolation_mode = (ImInterpolateMode)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["warp_tl"] = imgui_json::vec2(m_warp_tl);
        value["warp_tr"] = imgui_json::vec2(m_warp_tr);
        value["warp_br"] = imgui_json::vec2(m_warp_br);
        value["warp_bl"] = imgui_json::vec2(m_warp_bl);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
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
        ImGui::Button((std::string(u8"\ue437") + "##" + std::to_string(m_ID)).c_str(), size);
#else
        ImGui::Button((std::string("F") + "##" + std::to_string(m_ID)).c_str(), size);
#endif
        ImGui::PopStyleColor(3);
        ImGui::SetWindowFontScale(1.0);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_IReset  = { this, "Reset In" };
    FlowPin   m_Exit    = { this, "Exit" };
    FlowPin   m_OReset  = { this, "Reset Out" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[3] = { &m_Enter, &m_IReset, &m_MatIn };
    Pin* m_OutputPins[3] = { &m_Exit, &m_OReset, &m_MatOut };

private:
    ImGui::warpPerspective_vulkan * m_transform {nullptr};
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImInterpolateMode m_interpolation_mode {IM_INTERPOLATE_NEAREST};

    ImVec2 m_warp_tl      {0.f, 0.f};
    ImVec2 m_warp_tr      {1.f, 0.f};
    ImVec2 m_warp_br      {1.f, 1.f};
    ImVec2 m_warp_bl      {0.f, 1.f};
    ImGui::ImMat m_matrix;
};
} //namespace BluePrint
