#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <CopyTo_vulkan.h>

typedef enum Slider_Type : int32_t
{
    SLIDER_RIGHT = 0,
    SLIDER_LEFT,
    SLIDER_BOTTOM,
    SLIDER_TOP,
    SLIDER_RIGHT_BOTTOM,
    SLIDER_LEFT_TOP,
    SLIDER_RIGHT_TOP,
    SLIDER_LEFT_BOTTOM,
} Slider_Type;

namespace BluePrint
{
struct SliderFusionNode final : Node
{
    BP_NODE_WITH_NAME(SliderFusionNode, "Slider Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    SliderFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Slider Transform"; }

    ~SliderFusionNode()
    {
        if (m_copy) { delete m_copy; m_copy = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
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
        int x = 0, y = 0;
        auto mat_first = context.GetPinValue<ImGui::ImMat>(m_MatInFirst);
        auto mat_second = context.GetPinValue<ImGui::ImMat>(m_MatInSecond);
        float percentage = 1.0f - context.GetPinValue<float>(m_Pos);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            switch (m_slider_type)
            {
                case SLIDER_RIGHT :
                    x = (1.0 - percentage) * mat_first.w;
                break;
                case SLIDER_LEFT:
                    x = - (1.0 - percentage) * mat_first.w;
                break;
                case SLIDER_BOTTOM:
                    y = (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_TOP:
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_RIGHT_BOTTOM:
                    x = (1.0 - percentage) * mat_first.w;
                    y = (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_LEFT_TOP:
                    x = - (1.0 - percentage) * mat_first.w;
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_RIGHT_TOP:
                    x = (1.0 - percentage) * mat_first.w;
                    y = - (1.0 - percentage) * mat_first.h;
                break;
                case SLIDER_LEFT_BOTTOM:
                    x = - (1.0 - percentage) * mat_first.w;
                    y = (1.0 - percentage) * mat_first.h;
                break;
                default: break;
            }
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_copy || m_device != gpu)
            {
                if (m_copy) { delete m_copy; m_copy = nullptr; }
                m_copy = new ImGui::CopyTo_vulkan(gpu);
            }
            if (!m_copy)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            double node_time = 0;
            node_time += m_copy->copyTo(mat_first, im_RGB, 0, 0);
            node_time += m_copy->copyTo(mat_second, im_RGB, x, y);
            m_NodeTimeMs = node_time;
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
        int type = m_slider_type;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        //ImGui::Combo("Type:", &type, items, IM_ARRAYSIZE(items));
        ImGui::RadioButton("Right In", &type, SLIDER_RIGHT);
        ImGui::RadioButton("Left In", &type, SLIDER_LEFT);
        ImGui::RadioButton("Bottom In", &type, SLIDER_BOTTOM);
        ImGui::RadioButton("Top In", &type, SLIDER_TOP);
        ImGui::RadioButton("Right Bottom In", &type, SLIDER_RIGHT_BOTTOM);
        ImGui::RadioButton("Left Top In", &type, SLIDER_LEFT_TOP);
        ImGui::RadioButton("Right Top In", &type, SLIDER_RIGHT_TOP);
        ImGui::RadioButton("Left Bottom In", &type, SLIDER_LEFT_BOTTOM);
        if (type != m_slider_type) { m_slider_type = type; changed = true; }
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
        if (value.contains("slider_type"))
        { 
            auto& val = value["slider_type"];
            if (val.is_number())
                m_slider_type = (Slider_Type)val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["slider_type"] = imgui_json::number(m_slider_type);
    }

    void load_logo() const
    {
        int width = 0, height = 0, component = 0;
        if (auto data = stbi_load_from_memory((stbi_uc const *)logo_data, logo_size, &width, &height, &component, 4))
        {
            m_logo = ImGui::ImCreateTexture(data, width, height);
        }
    }

    void DrawNodeLogo(ImGuiContext * ctx, ImVec2 size) const override
    {
        if (ctx) ImGui::SetCurrentContext(ctx); // External Node must set context
        // if show icon then we using u8"\ue882"
        if (!m_logo)
        {
            load_logo();
        }
        if (m_logo)
        {
            int logo_col = (m_logo_index / 4) % 4;
            int logo_row = (m_logo_index / 4) / 4;
            float logo_start_x = logo_col * 0.25;
            float logo_start_y = logo_row * 0.25;
            ImGui::Image(m_logo, size, ImVec2(logo_start_x, logo_start_y),  ImVec2(logo_start_x + 0.25f, logo_start_y + 0.25f));
            m_logo_index++; if (m_logo_index >= 64) m_logo_index = 0;
        }
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatInFirst, &m_MatInSecond}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatInFirst   = { this, "In 1" };
    MatPin    m_MatInSecond   = { this, "In 2" };
    FloatPin  m_Pos = { this, "Pos" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Pos };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    int m_slider_type   {SLIDER_RIGHT};
    ImGui::CopyTo_vulkan * m_copy   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4515;
    const unsigned int logo_data[4516/4] =
{
    0xe0ffd8ff, 0x464a1000, 0x01004649, 0x01000001, 0x00000100, 0x8400dbff, 0x07070a00, 0x0a060708, 0x0b080808, 0x0e0b0a0a, 0x0d0e1018, 0x151d0e0d, 
    0x23181116, 0x2224251f, 0x2621221f, 0x262f372b, 0x21293429, 0x31413022, 0x3e3b3934, 0x2e253e3e, 0x3c434944, 0x3e3d3748, 0x0b0a013b, 0x0e0d0e0b, 
    0x1c10101c, 0x2822283b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 0x3b3b3b3b, 
    0x3b3b3b3b, 0x3b3b3b3b, 0xc0ff3b3b, 0x00081100, 0x03000190, 0x02002201, 0x11030111, 0x01c4ff01, 0x010000a2, 0x01010105, 0x00010101, 0x00000000, 
    0x01000000, 0x05040302, 0x09080706, 0x00100b0a, 0x03030102, 0x05030402, 0x00040405, 0x017d0100, 0x04000302, 0x21120511, 0x13064131, 0x22076151, 
    0x81321471, 0x2308a191, 0x15c1b142, 0x24f0d152, 0x82726233, 0x17160a09, 0x251a1918, 0x29282726, 0x3635342a, 0x3a393837, 0x46454443, 0x4a494847, 
    0x56555453, 0x5a595857, 0x66656463, 0x6a696867, 0x76757473, 0x7a797877, 0x86858483, 0x8a898887, 0x95949392, 0x99989796, 0xa4a3a29a, 0xa8a7a6a5, 
    0xb3b2aaa9, 0xb7b6b5b4, 0xc2bab9b8, 0xc6c5c4c3, 0xcac9c8c7, 0xd5d4d3d2, 0xd9d8d7d6, 0xe3e2e1da, 0xe7e6e5e4, 0xf1eae9e8, 0xf5f4f3f2, 0xf9f8f7f6, 
    0x030001fa, 0x01010101, 0x01010101, 0x00000001, 0x01000000, 0x05040302, 0x09080706, 0x00110b0a, 0x04020102, 0x07040304, 0x00040405, 0x00770201, 
    0x11030201, 0x31210504, 0x51411206, 0x13716107, 0x08813222, 0xa1914214, 0x2309c1b1, 0x15f05233, 0x0ad17262, 0xe1342416, 0x1817f125, 0x27261a19, 
    0x352a2928, 0x39383736, 0x4544433a, 0x49484746, 0x5554534a, 0x59585756, 0x6564635a, 0x69686766, 0x7574736a, 0x79787776, 0x8483827a, 0x88878685, 
    0x93928a89, 0x97969594, 0xa29a9998, 0xa6a5a4a3, 0xaaa9a8a7, 0xb5b4b3b2, 0xb9b8b7b6, 0xc4c3c2ba, 0xc8c7c6c5, 0xd3d2cac9, 0xd7d6d5d4, 0xe2dad9d8, 
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xb2a22820, 0xc9a9493c, 0xa0c1e9a6, 
    0xc0c8937d, 0x1d38c021, 0xf3fc074f, 0xb02bbb49, 0x7faee835, 0xddb57ac3, 0x9716d7e4, 0xf9e399ed, 0xc7295486, 0xd3073042, 0x9482aef3, 0x80aea464, 
    0xa08aa228, 0x80a2280a, 0x8fac280a, 0x69726a12, 0x1f6870ba, 0x0830f264, 0x53070e70, 0x3c00ffc1, 0xecca6ed2, 0x9f2b7a0d, 0x77addef0, 0xa5c53579, 
    0xfe7866fb, 0x710a9561, 0xf4018cd0, 0xa5a0ebfc, 0xa02b2919, 0xa8a2280a, 0xa0288a02, 0x232b8a02, 0x9a9c9ac4, 0x071a9c6e, 0x028c3cd9, 0xd481031c, 
    0x34cf7ff0, 0x03bbb29b, 0xfce78a5e, 0xde5dab37, 0x7e69714d, 0x983f9ed9, 0x749c4265, 0x3f7d0023, 0x4629e83a, 0x02e84a4a, 0x00aa288a, 0x00288aa2, 
    0xf1c88aa2, 0x9b26a726, 0xf68106a7, 0x8700234f, 0x3c75e000, 0x26cdf31f, 0xd7c0aeec, 0x00ffb9a2, 0x77d7ea0d, 0x5f5a5c93, 0xe68f67b6, 0x1da75019, 
    0x4f1fc008, 0x510abace, 0x00ba9292, 0x802a8aa2, 0x008aa228, 0xf5d78b2b, 0x1cf16418, 0x82676542, 0x7ac180d0, 0x00ffd493, 0x6957f841, 0x8b76da59, 
    0x35739d06, 0xb22c4bca, 0x429af7cd, 0x0a7a723d, 0xd17271ce, 0xf6b71c0d, 0x24fcbfc5, 0xacdba8c9, 0x09e4c491, 0xe075e043, 0xd7d0edf4, 0xeaa8d675, 
    0xafbe45ba, 0x3a3b4712, 0x4c866cec, 0x5bcde967, 0x04262f82, 0xc53e7b8b, 0x9cbab90b, 0xb7b80851, 0x14451f70, 0x280a8256, 0xe20a80a2, 0x1946fdf5, 
    0x9910473c, 0x20b4e059, 0xf5a45e30, 0x157ed03f, 0x9d7656da, 0x5ca7c1a2, 0xcb9272cd, 0xe67db32c, 0x9e5c8f90, 0x5c9cb382, 0x2d4743b4, 0xff6fb1fd, 
    0x6a320900, 0x7124eb36, 0xf8500239, 0x3b3d781d, 0x75dd3574, 0x91ae3aaa, 0x91c4ab6f, 0x1bbbcece, 0xfa199321, 0x8be05673, 0xde2281c9, 0xee42b1cf, 
    0x4214a76e, 0x07dc2d2e, 0xa01545d1, 0xa0288a82, 0x7fbdb802, 0x114f8651, 0x785626c4, 0x170c082d, 0xf44f3da9, 0x9576851f, 0xb068a79d, 0x5c33d769, 
    0x2ccbb2a4, 0x23a479df, 0xaca027d7, 0x102d17e7, 0x6c7fcbd1, 0xc200ff5b, 0xba8d9a4c, 0x404e1cc9, 0x5e073e94, 0x0ddd4e0f, 0x8e6a5d77, 0xea5ba4ab, 
    0xb37324f1, 0x64c8c6ae, 0xd59c7ec6, 0x60f222b8, 0xecb3b748, 0xa99bbb50, 0x8b8b10c5, 0x50f40177, 0x3b054e06, 0x37ee7fcb, 0x5bda56e5, 0xa268e38a, 
    0xc515608a, 0x328cfaeb, 0x32218e78, 0x4068c1b3, 0xea49bd60, 0x2bfca07f, 0x3bedacb4, 0xb94e8345, 0x9625e59a, 0xcdfb6659, 0x3db91e21, 0xb9386705, 
    0x5b8e8668, 0xfedf62fb, 0x6dd46412, 0x72e248d6, 0x3af0a104, 0xe8767af0, 0x54ebba6b, 0xdf225d75, 0x9d238957, 0x4336769d, 0xe6f43326, 0x9317c1ad, 
    0x9fbd4502, 0xdddc8562, 0x5c84284e, 0xa20fb85b, 0x350f8aa5, 0x38c76e4b, 0x937d52e9, 0xa6d2bffd, 0xd9c5e955, 0x8ae21cb2, 0x6c4551d4, 0x43455150, 
    0x95057775, 0xc95c5cbb, 0xcbb8c6e5, 0x4abf9e60, 0xad8a9a00, 0x6adaa865, 0x9396b431, 0x07a78a79, 0xf10723e5, 0x702734ab, 0x80a2280a, 0x80a2280a, 
    0x6ea8280a, 0xb7b2e0ae, 0x3c998b6b, 0x6c19d7b8, 0x40e9d713, 0xac555113, 0x464d1bb5, 0x6fd29236, 0xfce05431, 0x35fe60a4, 0x01ee8466, 0xa2224752, 
    0x9e1bc6e0, 0xa6a2a3a6, 0x62565251, 0x1445e16a, 0xa2280c55, 0x82bbbaa1, 0x2eaeddca, 0x5ce3f264, 0x5f4fb065, 0x454d00a5, 0x6dd4b256, 0x4bda1835, 
    0x53c5bc49, 0x8391f283, 0x139ad5f8, 0x6ba70ab8, 0xab39f406, 0x00ff6b1f, 0xd4aaf563, 0x09a57356, 0x149748fc, 0xa2280af7, 0xa22828b4, 0x82bbbaa1, 
    0x2eaeddca, 0x5ce3f264, 0x5f4fb065, 0x454d00a5, 0x6dd4b256, 0x4bda1835, 0x53c5bc49, 0x8391f283, 0x139ad5f8, 0x3fc512b8, 0xe776a594, 0x3ea9f59c, 
    0xebc7fed7, 0x65aca855, 0xda9d9c42, 0x282cc225, 0xa0d88aa2, 0x73c567ae, 0xa6c5cd3d, 0xa5f90995, 0x87fccd60, 0xb89aaef5, 0x752fd3e4, 0xe9e66ebd, 
    0x53257b9e, 0xc868e498, 0xd331e838, 0xf65267b5, 0x4b34eab2, 0xf8a201a4, 0xe73c6d9e, 0x096198ca, 0x517fdcef, 0x9a71655d, 0x69a885a6, 0x157941f2, 
    0xabd2d7d4, 0xcb12768e, 0xc573478e, 0xd7cc3676, 0x0ae1c630, 0xb0765097, 0xe6741cc1, 0x9eae1d94, 0x5251c780, 0xee5f9e7d, 0xb38fa2fe, 0xd4dffdcb, 
    0xfeb0f653, 0xee324f64, 0x56144547, 0x7ce60a83, 0xdcdc3357, 0x9f50695a, 0xdf0c569a, 0xe95a7fc8, 0x324d8eab, 0xeed65bf7, 0xb2e7996e, 0x468e3955, 
    0x838e838c, 0x75563b1d, 0xa32e6b2f, 0x1a40ba44, 0xd3e6892f, 0x86a97cce, 0xc7fd9e10, 0x57d615f5, 0x5a68aa19, 0x17249f86, 0x7d4d5d91, 0x61e7b82a, 
    0x77e4b82c, 0x666f573c, 0x1469738b, 0xae0b1dec, 0x3d82914a, 0x8a2a59b1, 0x9489f79a, 0xd191d592, 0x9ecafe56, 0x53d947ad, 0xd5faa8d5, 0x4aa43d32, 
    0x74455194, 0x3e73059a, 0x6eee992b, 0x4fa8342d, 0x6f062bcd, 0x74ad3fe4, 0x9926c7d5, 0x77ebad7b, 0xd9f34c37, 0x23c79c2a, 0x41c74146, 0x3aab9d8e, 
    0x5197b597, 0x0d205da2, 0x69f3c417, 0xc3543ee7, 0xe37e4f08, 0x2beb8afa, 0xf4f0b19f, 0xb120bef6, 0xafe59eb8, 0x32067397, 0x467e3b21, 0xff3da923, 
    0xfceb5a00, 0xfee2feb4, 0xfbebce55, 0x9cd1ca27, 0x14caa2e6, 0x2d00ff55, 0x95bfb83f, 0x717f5a1e, 0x5c5f2a7f, 0xaa7d628f, 0x15455128, 0xcc156ada, 
    0xb967aef8, 0xa1d2b4b8, 0x19ac343f, 0xb5fe90bf, 0x9a1c57d3, 0xadb7ee65, 0xcf33dddc, 0x1c73aa64, 0x1d07198d, 0xac763a06, 0x5dd65eea, 0x2d9a8b46, 
    0xaf95de85, 0xb3966ecf, 0x0ba3a4b5, 0xe3958c28, 0x514fcf91, 0xff963f5d, 0xca6fdc00, 0x495ba49d, 0x6fc3a567, 0x3a5cc32d, 0xd58dcc6e, 0x9ffac4b2, 
    0x5f5cbb5a, 0x366e7059, 0xca1d7530, 0x00ff5b1e, 0x3c2abf71, 0x7ee3feb7, 0x3e8a7e55, 0xf6c22eb9, 0x8aa2ceac, 0x0a37d12b, 0x0a80a228, 0x7afddf92, 
    0xa3ca3ffe, 0xe651a6a2, 0xea6a428b, 0x9d158dc6, 0x4f7d7145, 0x9781dffb, 0x280af3b2, 0x82cdbba2, 0x02a0288a, 0x00ffdbae, 0xf9c75fa8, 0x1a2b2ad5, 
    0x6da4bdd4, 0x651e6572, 0xce8a4663, 0xa7beb9a2, 0xcfc0effd, 0x148579d9, 0xc1e65d51, 0x04501445, 0xeb00ff96, 0x55fef1d7, 0xaea8b376, 0x697fd86a, 
    0x50cedc2b, 0x683477e6, 0x1f2beaac, 0x7b00ffa9, 0x79d927f0, 0x5d511485, 0x1445c1e6, 0x7fdb0550, 0xfff80bf5, 0xb3923a00, 0x6129aea8, 0xccdda439, 
    0xdcbb3b9d, 0xc0abaed1, 0x72b776f6, 0x689b0b5e, 0x6f13daa7, 0xd7b18198, 0xa2846ba6, 0xde59c292, 0xe38000ff, 0x45e1ce4e, 0xa0715714, 0x35894756, 
    0x38dd3439, 0x79b20f34, 0x07380418, 0xffe0a903, 0x5e6b9e00, 0xdfdbf4b2, 0x78ca78b5, 0xf696fc54, 0xe48ebf8c, 0x56fbfc27, 0x06c9a755, 0xca95ddc4, 
    0xaed51b7e, 0xb4b826ef, 0x1fcf6cbf, 0x4ea132cc, 0x3e80113a, 0x15749d9f, 0xc77145a3, 0x5fb3d216, 0xf2b59789, 0x1aada833, 0xfdb9be2a, 0xaf3dc4df, 
    0x14459d91, 0x55b07957, 0x532d5a5d, 0x32fb07ec, 0x18b1d32d, 0x73075e39, 0xaa5671f3, 0x93ea7ffc, 0xd85cd1fd, 0x15a7928a, 0x1d272762, 0x1d120d8c, 
    0xabab6c77, 0x4741ce43, 0x8cfa9cdc, 0x9e7d6a2d, 0xa2feee5f, 0x8b5cd1ae, 0x192b5115, 0xfb5256fb, 0xfdddbf3c, 0x97671f45, 0xaba8bffb, 0xdcfa53b4, 
    0x567b21fb, 0x15455167, 0x90049de9, 0x0e7224c6, 0x928a677a, 0x3292255b, 0x139134ab, 0x23b769fc, 0x947e04f3, 0xa7f55fdb, 0x5eb7dafd, 0xe3a42276, 
    0x3961263b, 0x412af434, 0x1e522260, 0x398969e6, 0x47d026dd, 0xa5a2f2fd, 0xb57a2afb, 0xc25b524d, 0x401137d7, 0xc2950684, 0x9c647a02, 0xa7da1e56, 
    0xdce53972, 0xf554f6ab, 0x9eca3e6a, 0xfc675dad, 0xfcaf7a20, 0xdf7f5afc, 0xc400ff6d, 0xa0ad5ed6, 0x0ddb68df, 0x3847a1d2, 0x4f904357, 0xaa5637a5, 
    0x5be753ea, 0x5114e59c, 0x5dd0b95e, 0xa931348e, 0x6a07bd28, 0xdc9f9677, 0xff88ca5f, 0xfb27d500, 0x2c5e9da2, 0xa9672ea5, 0xdfb8dbc8, 0xbfb83f2d, 
    0x7f5a1e95, 0x752a7f71, 0xf01abe74, 0x61b196e5, 0xb3d4c535, 0x512147a3, 0x1803b0e5, 0xb43eb807, 0x4c769393, 0xc84e4e71, 0xfeb43ce6, 0x7954fee2, 
    0xfcc5fd69, 0xf0b7b4ab, 0x47a985ae, 0xa576f627, 0x729d8e34, 0x46a6c741, 0x30577205, 0xb6d4ad35, 0x9c6841ee, 0x1cd423a1, 0xa84b6e53, 0x683292da, 
    0x8ef68aa2, 0x269aaa90, 0x7d9d068b, 0x2c4bca3d, 0x33bfccb2, 0x4fae4748, 0xf1e95641, 0x6c31e549, 0x55568c67, 0x9606cda1, 0xc5aa96e2, 0xdabf2aea, 
    0xfdd800ff, 0xfb5ffb68, 0xdf79ad1f, 0xe7d8ab56, 0x2c3472f6, 0x1bf54922, 0x921ba468, 0x67585149, 0xd78c0419, 0xbe69aa67, 0x251ed215, 0xe671b2bb, 
    0x735769e7, 0x7bed1fd3, 0xa96b03d7, 0x1bee9a7d, 0x95772b8f, 0xd8ddbe22, 0xd2710ece, 0x223e99ba, 0x492b2bc3, 0xfb9523a1, 0x1f07a7a5, 0x705ce5f8, 
    0x35aa2df5, 0x38b52c84, 0xbda2288a, 0xb6ab4033, 0xc4dae5c9, 0x40ed8b85, 0xc7533737, 0xc2aa5453, 0xbb506ddd, 0xae750c3a, 0x4d394d5c, 0x5267542e, 
    0x340bad2d, 0xffb57f55, 0xd1fab100, 0x3ff6bff6, 0xb5fae35a, 0xce5ec65e, 0x1078a247, 0x90dca391, 0x1fe74432, 0x0dadc8f7, 0xfef2f236, 0x67fbe3de, 
    0x90684b4e, 0xc3e8170c, 0x23ae41be, 0xd191f142, 0x16b7e52c, 0xf791711e, 0xe3b6f3ee, 0x3e6d3a80, 0xc48f2d95, 0x49094a1d, 0x8cbb45bc, 0x503b6d8c, 
    0x5b0bf583, 0x5a12352a, 0x24d1111d, 0x92539472, 0x463b9336, 0x3cfb4e71, 0x45fdddbf, 0x4e90ba76, 0x1dd285cd, 0xffb4fe6b, 0xea56bb00, 0x13cb51b4, 
    0xc6f3f216, 0x7c978a3a, 0xffc7f3bf, 0xe7151e00, 0xef3c17e2, 0x61f4be16, 0x25a17735, 0x00ffd25a, 0xff672de4, 0xffd35d00, 0x47154200, 0xc7f3bf7c, 
    0x151e00ff, 0xb6d4ad2d, 0x7ddc70d7, 0x2295779f, 0x67e0ddbe, 0x15ac3807, 0xa2f7e537, 0x9df4d452, 0xf2d6c67e, 0xd5b9666b, 0x3b53c27e, 0xbf149847, 
    0x73fe534f, 0x4b37fe59, 0x4cc874a9, 0x8c68af48, 0x930da41d, 0x1c7e609c, 0xe2af06d6, 0x85a67558, 0x979974ae, 0x1dd1cec9, 0x1de78cda, 0x9dd28772, 
    0xb8c6e27b, 0xedb23fd1, 0x3d22aff4, 0xc68da1aa, 0x827e00fc, 0xf5bd71b6, 0x93377a5f, 0x4523e74e, 0xfb27fb59, 0x641fa57f, 0xf46f00ff, 0xd2acdfae, 
    0x24d21eee, 0xaa00ffd1, 0x3a45f74f, 0x0bd54691, 0x5e4b31e8, 0x39db9d4c, 0xdf5de19e, 0x6b347081, 0x388f8c9c, 0x15f9fee7, 0x83f656c2, 0x4bec7fe2, 
    0x9fcd2d39, 0xfc3ee69f, 0xc0719bf9, 0xa50fc618, 0x524f1a54, 0x76a5b4e9, 0xa50e7a74, 0x2d4bdda2, 0x2d6f97a6, 0x2c99ae9c, 0xdfe36450, 0xd77ba627, 
    0xbed8ad1b, 0x6e4fab9d, 0x6d4899f2, 0xcf56cfdb, 0x5bf3fb3c, 0xdacef890, 0xd0b431d4, 0x759881a0, 0xc2f30347, 0x6fefe68a, 0x92bbbf26, 0x24b3e1ea, 
    0x071d2787, 0xd13439b5, 0xc66a9253, 0x4b044b3d, 0xf1c4ed2e, 0x9b5551e9, 0x2bfce34f, 0x1a27afd4, 0xbb59a36d, 0x53d97744, 0x2afba8d5, 0x454db57a, 
    0xa7debe79, 0x5d9e9f73, 0x9eca7ec8, 0x53d947ad, 0x9ffeaad5, 0x7da52e63, 0x8b2ca41d, 0x5c80a424, 0xed193890, 0xf8dfadf4, 0xf95ff540, 0x00ff69ef, 
    0x00ffb77d, 0xaa554d13, 0x372765f6, 0x95fdc9b1, 0xb28f5a3d, 0x9e56aba7, 0xe9dda4a9, 0x6857ac13, 0x65956101, 0xaa3d0c39, 0x556d2f95, 0x91941375, 
    0x9d355a9d, 0x8cd3355a, 0xea323ffb, 0xa2280af4, 0x8a420cb8, 0x1b4db4ec, 0x43c39746, 0x32b5be51, 0x60670732, 0x9e0009ec, 0xa7c28ec0, 0x3bbcd0b7, 
    0xcdf2e9ad, 0x0524c7a7, 0x9be55d49, 0x20cf38e5, 0x235fc593, 0x38db5434, 0xa0a2286a, 0xfc69afcc, 0xaa58857f, 0xf8c79ff6, 0xc4e88a55, 0x971500ff, 
    0x280ae253, 0xff0a7fad, 0x67cbc800, 0xe87ff3fe, 0xee4ab026, 0x6377a5c4, 0x0bf58a22, 0x1d4efddb, 0x7d6b1b5a, 0x70b5cf3f, 0xb7e9e5bb, 0x056e9f27, 
    0x5b39fe72, 0x5b80bf25, 0x3eed1384, 0x19c34f70, 0x2857ebf7, 0x54d25c59, 0x9773afec, 0x332b8aa2, 0x288aa220, 0x3f2bad00, 0x85ba6a0f, 0xdadadcb2, 
    0x703691f9, 0xe08c62de, 0x9b35a1e3, 0xa8e1875e, 0xcbc19be5, 0x5279121c, 0xff2390b8, 0x38ee7400, 0xd98d7035, 0xb3938a70, 0x0d9f0b39, 0xb49016eb, 
    0x453059d3, 0x5b592519, 0xb226f003, 0x21edd3eb, 0x26dd74d4, 0xda4ab56f, 0x5525e38c, 0x9ed3317d, 0xa52ce6b5, 0x0802ab58, 0xa2f62038, 0x15e7b071, 
    0x1445091b, 0xd5991954, 0xfff8d366, 0x56ad0a00, 0xf08f3f6d, 0xfc135faf, 0xf8d47426, 0x4551144b, 0xaf310779, 0x19f95fe1, 0xde00ff6c, 0xd704fd6f, 
    0xd958a965, 0x7541aecb, 0xa42dae36, 0xf3202e8c, 0x3ec19602, 0xe24abfa7, 0x0d733d3c, 0x3d6dbba6, 0x903882c4, 0x7c7bcc9d, 0x507dbaa6, 0xbd565ff8, 
    0x499dbb5b, 0xda852ab7, 0x27400a87, 0xf5f79cfd, 0x9bf061ad, 0x5fa5fbc1, 0x4bdb251e, 0xa2c4236f, 0xeaa832d5, 0xb91e4b72, 0x855ce9f4, 0xf12a3e75, 
    0xacfda505, 0x78391656, 0x843483d1, 0x00307060, 0x33d572cd, 0x758636b7, 0x9da1cd2d, 0x5a9d355a, 0xfb8cd935, 0xf4ea323f, 0xb8a2280a, 0xf0414f0c, 
    0xe06bebfc, 0x12e23daf, 0x142369ac, 0xf1041b3d, 0x94fa6852, 0xb8b2221e, 0xda03fbb5, 0x8d090644, 0x087d0ef0, 0x74e69a03, 0x69dd16bf, 0x1d670956, 
    0x49a84eb4, 0x93e7c40c, 0xc7e39a9a, 0x32b1943a, 0x92300c45, 0x2441bd31, 0x295b337d, 0x4da123ab, 0x5f616a59, 0xf93b0bdb, 0x08b795ed, 0xae0f2aa4, 
    0x66a5570d, 0x4bcc6267, 0x7a921c33, 0x9c584993, 0xe34f7be5, 0x57c52afc, 0xc23ffeb4, 0x274657ac, 0x9fbaacf8, 0x85bf5610, 0xb365e47f, 0xbf7900ff, 
    0x155913f4, 0x6bcb1324, 0x8d04cd32, 0x59f78b1c, 0x13ac084e, 0x3b3b25b3, 0xffac859e, 0xfd49c200, 0x5bd99fa6, 0x6eb0cd7e, 0xb21bb3dd, 0xe9f39c73, 
    0xe978a9d2, 0xb7cefe60, 0x56fb0432, 0xe33a82f1, 0xae19fe1c, 0x0d12fe5f, 0x3382fe5f, 0x75df00ff, 0xae897b4a, 0x2bcf32a5, 0x9df8e7ca, 0x34476bb2, 
    0x449535d3, 0x8aa223d3, 0xa210232b, 0xae00288a, 0x596b40f3, 0x29f61ffc, 0xa585e22e, 0x61961059, 0xe33cc995, 0x55d1c135, 0xcba85819, 0xa4819e95, 
    0x66f8a6c1, 0x5897e409, 0xf281c349, 0xd1e30082, 0x0fd73c41, 0x9ab7727f, 0xaecac585, 0x604696d5, 0x5e35013d, 0xacae1c8a, 0xc1ea4a39, 0x90541445, 
    0x00d9ff7f, 
};
};
} // namespace BluePrint
