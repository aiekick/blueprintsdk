#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <ImVulkanShader.h>
#include <warpAffine_vulkan.h>
#include <warpPerspective_vulkan.h>

namespace BluePrint
{
struct MatWarpAffineNode final : Node
{
    BP_NODE_WITH_NAME(MatWarpAffineNode, "Mat Warp Affine", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Media")
    MatWarpAffineNode(BP* blueprint): Node(blueprint) {
        m_Name = "Mat Warp Affine";
        m_matrix.create_type(3, 2, IM_DT_FLOAT32);
    }

    ~MatWarpAffineNode()
    {
        if (m_transform) { delete m_transform; m_transform = nullptr; }
    }

    void Reset(Context& context) override
    {
        Node::Reset(context);
        if (m_transform) { delete m_transform; m_transform = nullptr; }
    }

    void OnStop(Context& context) override
    {
        m_mutex.lock();
        m_MatOut.SetValue(ImGui::ImMat());
        m_mutex.unlock();
    }

    void calculate_matrix(int w, int h, float x_offset, float y_offset, float x_scale, float y_scale)
    {
        float _angle = m_angle / 180.f * M_PI;
        float _x_scale = 1.f / (x_scale + FLT_EPSILON);
        float _y_scale = 1.f / (y_scale + FLT_EPSILON);
        float alpha_00 = std::cos(_angle) * _x_scale;
        float alpha_11 = std::cos(_angle) * _y_scale;
        float beta_01 = std::sin(_angle) * _x_scale;
        float beta_10 = std::sin(_angle) * _y_scale;
        int center_x = w / 2 + x_offset * w;
        int center_y = h / 2 + y_offset * h;
        m_matrix.at<float>(0, 0) =  alpha_00; m_matrix.at<float>(1, 0) = beta_01;      m_matrix.at<float>(2, 0) = (1 - alpha_00) * center_x - beta_01 * center_y - x_offset * w;
        m_matrix.at<float>(0, 1) = -beta_10;     m_matrix.at<float>(1, 1) = alpha_11;  m_matrix.at<float>(2, 1) = beta_10 * center_x + (1 - alpha_11) * center_y - y_offset * h;
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
                m_transform = new ImGui::warpAffine_vulkan(gpu);
                if (!m_transform)
                {
                    return {};
                }
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            calculate_matrix(mat_in.w, mat_in.h, m_offset_x, m_offset_y, m_scale_x, m_scale_y);
            float _l = m_crop_l, _t = m_crop_t, _r = m_crop_r, _b = m_crop_b;
            if (m_crop_r + m_crop_l > 1.f) { _l = 1.f - m_crop_r; _r = 1.f - m_crop_l; }
            if (m_crop_b + m_crop_t > 1.f) { _t = 1.f - m_crop_b; _b = 1.f - m_crop_t; }
            ImPixel crop = ImPixel(_l * mat_in.w, 
                                    _t * mat_in.h, 
                                    _r * mat_in.w,
                                    _b * mat_in.h);
            m_NodeTimeMs = m_transform->filter(mat_in, im_RGB, m_matrix, m_interpolation_mode, ImPixel(0, 0, 0, 0), crop);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
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
        float _angle = m_angle;
        float _scale_x = m_scale_x;
        float _scale_y = m_scale_y;
        float _offset_x = m_offset_x;
        float _offset_y = m_offset_y;
        float _crop_l = m_crop_l;
        float _crop_t = m_crop_t;
        float _crop_r = m_crop_r;
        float _crop_b = m_crop_b;
        ImInterpolateMode _mode = m_interpolation_mode;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(240, 8));
        ImGui::PushItemWidth(240);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::SliderFloat("angle", &_angle, -360.f, 360.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_angle##WarpAffine")) { _angle = 0.f; changed = true; }
        ImGui::SliderFloat("scale x", &_scale_x, 0.1f, 8.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_scale_x##WarpAffine")) { _scale_x = 1.f; changed = true; }
        ImGui::SliderFloat("scale y", &_scale_y, 0.1f, 8.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_scale_y##WarpAffine")) { _scale_y = 1.f; changed = true; }
        ImGui::SliderFloat("offet x", &_offset_x, -1.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_offset_x##WarpAffine")) { _offset_x = 0.f; changed = true; }
        ImGui::SliderFloat("offet y", &_offset_y, -1.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_offset_y##WarpAffine")) { _offset_y = 0.f; changed = true; }

        ImGui::SliderFloat("crop L", &_crop_l, 0.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_crop_l##WarpAffine")) { _crop_l = 0.f; changed = true; }
        ImGui::SliderFloat("crop T", &_crop_t, 0.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_crop_t##WarpAffine")) { _crop_t = 0.f; changed = true; }
        ImGui::SliderFloat("crop R", &_crop_r, 0.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_crop_r##WarpAffine")) { _crop_r = 0.f; changed = true; }
        ImGui::SliderFloat("crop B", &_crop_b, 0.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_crop_b##WarpAffine")) { _crop_b = 0.f; changed = true; }

        ImGui::RadioButton("Nearest",   (int *)&_mode, IM_INTERPOLATE_NEAREST); ImGui::SameLine();
        ImGui::RadioButton("Bilinear",  (int *)&_mode, IM_INTERPOLATE_BILINEAR); ImGui::SameLine();
        ImGui::RadioButton("Bicubic",   (int *)&_mode, IM_INTERPOLATE_BICUBIC); ImGui::SameLine();

        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        if (_angle != m_angle) { m_angle = _angle; changed = true; }
        if (_scale_x != m_scale_x) { m_scale_x = _scale_x; changed = true; }
        if (_scale_y != m_scale_y) { m_scale_y = _scale_y; changed = true; }
        if (_offset_x != m_offset_x) { m_offset_x = _offset_x; changed = true; }
        if (_offset_y != m_offset_y) { m_offset_y = _offset_y; changed = true; }
        if (_crop_l != m_crop_l) { m_crop_l = _crop_l; changed = true; }
        if (_crop_t != m_crop_t) { m_crop_t = _crop_t; changed = true; }
        if (_crop_r != m_crop_r) { m_crop_r = _crop_r; changed = true; }
        if (_crop_b != m_crop_b) { m_crop_b = _crop_b; changed = true; }
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
        if (value.contains("angle"))
        {
            auto& val = value["angle"];
            if (val.is_number()) 
                m_angle = val.get<imgui_json::number>();
        }
        if (value.contains("scale_x"))
        {
            auto& val = value["scale_x"];
            if (val.is_number()) 
                m_scale_x = val.get<imgui_json::number>();
        }
        if (value.contains("scale_y"))
        {
            auto& val = value["scale_y"];
            if (val.is_number()) 
                m_scale_y = val.get<imgui_json::number>();
        }
        if (value.contains("offset_x"))
        {
            auto& val = value["offset_x"];
            if (val.is_number()) 
                m_offset_x = val.get<imgui_json::number>();
        }
        if (value.contains("offset_y"))
        {
            auto& val = value["offset_y"];
            if (val.is_number()) 
                m_offset_y = val.get<imgui_json::number>();
        }
        if (value.contains("crop_l"))
        {
            auto& val = value["crop_l"];
            if (val.is_number()) 
                m_crop_l = val.get<imgui_json::number>();
        }
        if (value.contains("crop_t"))
        {
            auto& val = value["crop_t"];
            if (val.is_number()) 
                m_crop_t = val.get<imgui_json::number>();
        }
        if (value.contains("crop_r"))
        {
            auto& val = value["crop_r"];
            if (val.is_number()) 
                m_crop_r = val.get<imgui_json::number>();
        }
        if (value.contains("crop_b"))
        {
            auto& val = value["crop_b"];
            if (val.is_number()) 
                m_crop_b = val.get<imgui_json::number>();
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
        value["angle"] = imgui_json::number(m_angle);
        value["scale_x"] = imgui_json::number(m_scale_x);
        value["scale_y"] = imgui_json::number(m_scale_y);
        value["offset_x"] = imgui_json::number(m_offset_x);
        value["offset_y"] = imgui_json::number(m_offset_y);
        value["crop_l"] = imgui_json::number(m_crop_l);
        value["crop_t"] = imgui_json::number(m_crop_t);
        value["crop_r"] = imgui_json::number(m_crop_r);
        value["crop_b"] = imgui_json::number(m_crop_b);
        value["interpolation"] = imgui_json::number(m_interpolation_mode);
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        float font_size = ImGui::GetFontSize();
        ImGui::SetWindowFontScale((size.x - 16) / font_size);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
#if IMGUI_ICONS
        ImGui::Button((std::string(u8"\ue980") + "##" + std::to_string(m_ID)).c_str(), size);
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
    ImGui::warpAffine_vulkan * m_transform {nullptr};
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    ImInterpolateMode m_interpolation_mode {IM_INTERPOLATE_NEAREST};
    float m_angle       {0.0};
    float m_scale_x     {1.0};
    float m_scale_y     {1.0};
    float m_offset_x    {0.f};
    float m_offset_y    {0.f};
    float m_crop_l      {0.f};
    float m_crop_t      {0.f};
    float m_crop_r      {0.f};
    float m_crop_b      {0.f};
    ImGui::ImMat m_matrix;
};
} //namespace BluePrint
