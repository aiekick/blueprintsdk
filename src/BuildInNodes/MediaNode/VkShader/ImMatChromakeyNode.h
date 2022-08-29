#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <ChromaKey_vulkan.h>
#include <imgui_node_editor_internal.h>
namespace edd = ax::NodeEditor::Detail;

namespace BluePrint
{
struct ChromaKeyNode final : Node
{
    BP_NODE_WITH_NAME(ChromaKeyNode, "Chroma Key", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Matting")
    ChromaKeyNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Chroma Key"; }

    ~ChromaKeyNode()
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
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_filter || gpu != m_device)
            {
                if (m_filter) { delete m_filter; m_filter = nullptr; }
                m_filter = new ImGui::ChromaKey_vulkan(gpu);
            }
            if (!m_filter)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_in.type : m_mat_data_type;
            m_filter->filter(mat_in, im_RGB, m_lumaMask, m_chromaColor,
                                m_alphaCutoffMin, m_alphaScale, m_alphaExponent,
                                m_alpha_only ? CHROMAKEY_OUTPUT_ALPHA_RGBA : CHROMAKEY_OUTPUT_NORMAL);
            im_RGB.time_stamp = mat_in.time_stamp;
            im_RGB.rate = mat_in.rate;
            im_RGB.flags = mat_in.flags;
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
        ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        bool _alpha_only = m_alpha_only;
        float _lumaMask = m_lumaMask;
        float _alphaCutoffMin = m_alphaCutoffMin;
        float _alphaScale = m_alphaScale;
        std::vector<float> _chromaColor = m_chromaColor;
        _chromaColor[3] = m_alphaExponent;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::Checkbox("Alpha Output##ChromaKey",&_alpha_only);
        ImGui::SliderFloat("Luma Mask##ChromaKey", &_lumaMask, 0.f, 20.f, "%.1f", flags);
        ImGui::SliderFloat("Alpha Cutoff Min##ChromaKey", &_alphaCutoffMin, 0.f, 1.f, "%.2f", flags);
        ImGui::SliderFloat("Alpha Scale##ChromaKey", &_alphaScale, 0.f, 40.f, "%.1f", flags);
        ImGui::PopItemWidth();
        ImGui::SetNextItemWidth(200);
        ImGui::ColorPicker4("##ChromaKey", (float *)_chromaColor.data(), /*ImGuiColorEditFlags_NoLabel |*/ ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_AlphaBar);
        if (_lumaMask != m_lumaMask) { m_lumaMask = _lumaMask; changed = true; }
        if (_alphaCutoffMin != m_alphaCutoffMin) { m_alphaCutoffMin = _alphaCutoffMin; changed = true; }
        if (_alphaScale != m_alphaScale) { m_alphaScale = _alphaScale; changed = true; }
        if (_chromaColor[0] != m_chromaColor[0] || _chromaColor[1] != m_chromaColor[1] || _chromaColor[2] != m_chromaColor[2]) { m_chromaColor = _chromaColor; changed = true; }
        if (_chromaColor[3] != m_alphaExponent) { m_alphaExponent = _chromaColor[3]; changed = true; }
        if (_alpha_only != m_alpha_only) { m_alpha_only = _alpha_only; changed = true; }
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
        if (value.contains("alpha_only"))
        { 
            auto& val = value["alpha_only"];
            if (val.is_boolean())
                m_alpha_only = val.get<imgui_json::boolean>();
        }
        if (value.contains("lumaMask"))
        {
            auto& val = value["lumaMask"];
            if (val.is_number()) 
                m_lumaMask = val.get<imgui_json::number>();
        }
        if (value.contains("alphaCutoffMin"))
        {
            auto& val = value["alphaCutoffMin"];
            if (val.is_number()) 
                m_alphaCutoffMin = val.get<imgui_json::number>();
        }
        if (value.contains("alphaScale"))
        {
            auto& val = value["alphaScale"];
            if (val.is_number()) 
                m_alphaScale = val.get<imgui_json::number>();
        }
        if (value.contains("alphaExponent"))
        {
            auto& val = value["alphaExponent"];
            if (val.is_number()) 
                m_alphaExponent = val.get<imgui_json::number>();
        }
        if (value.contains("chroma_color"))
        {
            imgui_json::GetTo<imgui_json::number>(value["chroma_color"], "x", m_chromaColor[0]);
            imgui_json::GetTo<imgui_json::number>(value["chroma_color"], "y", m_chromaColor[1]);
            imgui_json::GetTo<imgui_json::number>(value["chroma_color"], "z", m_chromaColor[2]);
            imgui_json::GetTo<imgui_json::number>(value["chroma_color"], "w", m_chromaColor[3]);
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["alpha_only"] = imgui_json::boolean(m_alpha_only);
        value["lumaMask"] = imgui_json::number(m_lumaMask);
        value["alphaCutoffMin"] = imgui_json::number(m_alphaCutoffMin);
        value["alphaScale"] = imgui_json::number(m_alphaScale);
        value["alphaExponent"] = imgui_json::number(m_alphaExponent);
        value["chroma_color"] = edd::Serialization::ToJson(m_chromaColor);
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

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type  {IM_DT_UNDEFINED};
    int m_device            {-1};
    ImGui::ChromaKey_vulkan * m_filter {nullptr};
    bool  m_alpha_only          {false};
    float m_lumaMask            {10.0f};
    std::vector<float> m_chromaColor        {0.0f, 1.0f, 0.0f, 1.0f};
    float m_alphaCutoffMin      {0.05f};
    float m_alphaScale          {40.f};
    float m_alphaExponent       {1.0f};
};
} //namespace BluePrint
