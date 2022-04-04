#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <CopyTo_vulkan.h>
#include <Crop_vulkan.h>

namespace BluePrint
{
struct DoorFusionNode final : Node
{
    BP_NODE_WITH_NAME(DoorFusionNode, "Door Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video")
    DoorFusionNode(BP& blueprint): Node(blueprint) { m_Name = "Mat Door Transform"; }

    ~DoorFusionNode()
    {
        if (m_crop) { delete m_crop; m_crop = nullptr; }
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
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        auto current = context.GetPinValue<int64_t>(m_FusionTimeStamp);
        auto total = context.GetPinValue<int64_t>(m_FusionDuration);
        auto percentage = (float)current / (float)(total - 40);
        percentage = ImClamp(percentage, 0.0f, 1.0f);
        if (!mat_first.empty() && !mat_second.empty())
        {
            if (!m_bEnabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_crop)
            {
                int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
                if (m_crop) { delete m_crop; m_crop = nullptr; }
                m_crop = new ImGui::Crop_vulkan(gpu);
            }
            if (!m_copy)
            {
                int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
                if (m_copy) { delete m_copy; m_copy = nullptr; }
                m_copy = new ImGui::CopyTo_vulkan(gpu);
            }
            if (!m_crop || !m_copy)
            {
                return {};
            }
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            ImGui::VkMat first_half; first_half.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            ImGui::VkMat second_half; second_half.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            if (m_bOpen)
            {
                m_copy->copyTo(mat_second, im_RGB, 0, 0);
                if (m_bHorizon)
                {
                    m_crop->crop(mat_first, first_half, 0, 0, mat_first.w / 2, mat_first.h);
                    m_crop->crop(mat_first, second_half, mat_first.w / 2, 0, mat_first.w / 2, mat_first.h);
                    int x1 = - percentage * first_half.w;
                    m_copy->copyTo(first_half, im_RGB, x1, 0);
                    int x2 = (percentage + 1.0) * second_half.w;
                    m_copy->copyTo(second_half, im_RGB, x2, 0);
                }
                else
                {
                    m_crop->crop(mat_first, first_half, 0, 0, mat_first.w, mat_first.h / 2);
                    m_crop->crop(mat_first, second_half, 0, mat_first.h / 2, mat_first.w, mat_first.h / 2);
                    int y1 = - percentage * first_half.h;
                    m_copy->copyTo(first_half, im_RGB, 0, y1);
                    int y2 = (percentage + 1.0) * second_half.h;
                    m_copy->copyTo(second_half, im_RGB, 0, y2);
                }
            }
            else
            {
                m_copy->copyTo(mat_first, im_RGB, 0, 0);
                if (m_bHorizon)
                {
                    m_crop->crop(mat_second, first_half, 0, 0, mat_second.w / 2, mat_second.h);
                    m_crop->crop(mat_second, second_half, mat_second.w / 2, 0, mat_second.w / 2, mat_second.h);
                    int x1 = - (1.0 - percentage) * first_half.w;
                    m_copy->copyTo(first_half, im_RGB, x1, 0);
                    int x2 = (2.0 - percentage) * second_half.w;
                    m_copy->copyTo(second_half, im_RGB, x2, 0);
                }
                else
                {
                    m_crop->crop(mat_second, first_half, 0, 0, mat_second.w, mat_second.h / 2);
                    m_crop->crop(mat_second, second_half, 0, mat_second.h / 2, mat_second.w, mat_second.h / 2);
                    int y1 = - (1.0 - percentage) * first_half.h;
                    m_copy->copyTo(first_half, im_RGB, 0, y1);
                    int y2 = (2.0 - percentage) * second_half.h;
                    m_copy->copyTo(second_half, im_RGB, 0, y2);
                }
            }

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

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        bool check = m_bEnabled;
        int open = m_bOpen ? 0 : 1;
        int horizon = m_bHorizon ? 0 : 1;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::TextUnformatted("Enable"); ImGui::SameLine();
        if (ImGui::ToggleButton("##enable_filter_Brightness",&check)) { m_bEnabled = check; changed = true; }
        if (check) ImGui::BeginDisabled(false); else ImGui::BeginDisabled(true);
        ImGui::RadioButton("Open", &open, 0); ImGui::SameLine();
        ImGui::RadioButton("Close", &open, 1);
        if ((m_bOpen && open != 0) || (!m_bOpen && open != 1)) { m_bOpen = open == 0; changed = true; };
        ImGui::RadioButton("Horizon", &horizon, 0); ImGui::SameLine();
        ImGui::RadioButton("Vertical", &horizon, 1);
        if ((m_bHorizon && horizon != 0) || (!m_bHorizon && horizon != 1)) { m_bHorizon = horizon == 0; changed = true; };
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
        if (value.contains("enabled"))
        { 
            auto& val = value["enabled"];
            if (val.is_boolean())
                m_bEnabled = val.get<imgui_json::boolean>();
        }
        if (value.contains("open"))
        { 
            auto& val = value["open"];
            if (val.is_boolean())
                m_bOpen = val.get<imgui_json::boolean>();
        }
        if (value.contains("horizon"))
        { 
            auto& val = value["horizon"];
            if (val.is_boolean())
                m_bHorizon = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) const override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["enabled"] = imgui_json::boolean(m_bEnabled);
        value["open"] = imgui_json::boolean(m_bOpen);
        value["horizon"] = imgui_json::boolean(m_bHorizon);
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond, &m_FusionDuration, &m_FusionTimeStamp}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatInFirst   = { this, "In 1" };
    MatPin    m_MatInSecond   = { this, "In 2" };
    Int64Pin  m_FusionDuration = { this, "Fusion Duration" };
    Int64Pin  m_FusionTimeStamp = { this, "Time Stamp" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[5] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_FusionDuration, &m_FusionTimeStamp };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    bool m_bEnabled     {true};
    bool m_bOpen        {true};
    bool m_bHorizon     {true};
    ImGui::Crop_vulkan * m_crop   {nullptr};
    ImGui::CopyTo_vulkan * m_copy   {nullptr};
};
} // namespace BluePrint