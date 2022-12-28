#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <GridFlip_vulkan.h>

namespace BluePrint
{
struct GridFlipFusionNode final : Node
{
    BP_NODE_WITH_NAME(GridFlipFusionNode, "GridFlip Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    GridFlipFusionNode(BP* blueprint): Node(blueprint) { m_Name = "GridFlip Transform"; }

    ~GridFlipFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
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
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float progress = context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_fusion || m_device != gpu)
            {
                if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
                m_fusion = new ImGui::GridFlip_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_backColor, m_pause, m_dividerWidth, m_randomness, m_size_x, m_size_y);
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
        float _pause = m_pause;
        float _dividerWidth = m_dividerWidth;
        float _randomness = m_randomness;
        int size_x = m_size_x;
        int size_y = m_size_y;
        ImPixel _backColor = m_backColor;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Pause##GridFlip", &_pause, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_pause##GridFlip")) { _pause = 0.1f; changed = true; }
        ImGui::SliderFloat("Divider##GridFlip", &_dividerWidth, 0.f, 1.f, "%.2f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_divider##GridFlip")) { _dividerWidth = 0.05f; changed = true; }
        ImGui::SliderFloat("Randomness##GridFlip", &_randomness, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_randomness##GridFlip")) { _randomness = 0.1f; changed = true; }
        ImGui::SliderInt("Size X##GridFlip", &size_x, 1, 10, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_size_x##GridFlip")) { size_x = 4; changed = true; }
        ImGui::SliderInt("Size Y##GridFlip", &size_y, 1, 10, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_size_y##GridFlip")) { size_y = 4; changed = true; }
        ImGui::PopItemWidth();
        if (ImGui::ColorEdit4("BackColor##GridFlip", (float*)&_backColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_backColor = _backColor; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Back Color");
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_backcolor##GridFlip")) { m_backColor = {0.0f, 0.0f, 0.0f, 1.0f}; changed = true; }
        ImGui::EndDisabled();
        if (_pause != m_pause) { m_pause = _pause; changed = true; }
        if (_dividerWidth != m_dividerWidth) { m_dividerWidth = _dividerWidth; changed = true; }
        if (_randomness != m_randomness) { m_randomness = _randomness; changed = true; }
        if (size_x != m_size_x) { m_size_x = size_x; changed = true; }
        if (size_y != m_size_y) { m_size_y = size_y; changed = true; }
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
        if (value.contains("backColor"))
        {
            auto& val = value["backColor"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_backColor = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        if (value.contains("pause"))
        {
            auto& val = value["pause"];
            if (val.is_number()) 
                m_pause = val.get<imgui_json::number>();
        }
        if (value.contains("dividerWidth"))
        {
            auto& val = value["dividerWidth"];
            if (val.is_number()) 
                m_dividerWidth = val.get<imgui_json::number>();
        }
        if (value.contains("randomness"))
        {
            auto& val = value["randomness"];
            if (val.is_number()) 
                m_randomness = val.get<imgui_json::number>();
        }
        if (value.contains("size_x"))
        {
            auto& val = value["size_x"];
            if (val.is_number()) 
                m_size_x = val.get<imgui_json::number>();
        }
        if (value.contains("size_y"))
        {
            auto& val = value["size_y"];
            if (val.is_number()) 
                m_size_y = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["backColor"] = imgui_json::vec4(ImVec4(m_backColor.r, m_backColor.g, m_backColor.b, m_backColor.a));
        value["pause"] = imgui_json::number(m_pause);
        value["dividerWidth"] = imgui_json::number(m_dividerWidth);
        value["randomness"] = imgui_json::number(m_randomness);
        value["size_x"] = imgui_json::number(m_size_x);
        value["size_y"] = imgui_json::number(m_size_y);
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
        ImGui::Button((std::string(u8"\uf37f") + "##" + std::to_string(m_ID)).c_str(), size);
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

    FlowPin   m_Enter       = { this, "Enter" };
    FlowPin   m_Exit        = { this, "Exit" };
    MatPin    m_MatInFirst  = { this, "In 1" };
    MatPin    m_MatInSecond = { this, "In 2" };
    FloatPin  m_Pos         = { this, "Pos" };
    MatPin    m_MatOut      = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImPixel m_backColor {0.0f, 0.0f, 0.0f, 1.0f};
    float m_pause       {0.1};
    float m_dividerWidth {0.05};
    float m_randomness  {0.1};
    int m_size_x        {4};
    int m_size_y        {4};
    ImGui::GridFlip_vulkan * m_fusion   {nullptr};
};
} // namespace BluePrint
