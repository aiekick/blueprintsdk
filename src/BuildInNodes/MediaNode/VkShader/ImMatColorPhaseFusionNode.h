#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <ColorPhase_vulkan.h>

namespace BluePrint
{
struct ColorPhaseFusionNode final : Node
{
    BP_NODE_WITH_NAME(ColorPhaseFusionNode, "ColorPhase Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Color")
    ColorPhaseFusionNode(BP* blueprint): Node(blueprint) { m_Name = "ColorPhase Transform"; }

    ~ColorPhaseFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
        if (m_logo) { ImGui::ImDestroyTexture(m_logo); m_logo = nullptr; }
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
                m_fusion = new ImGui::ColorPhase_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_fromColor, m_toColor);
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
        ImPixel _fromColor = m_fromColor;
        ImPixel _toColor = m_toColor;
        if (ImGui::ColorEdit4("##FromColorPhase", (float*)&_fromColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_fromColor = _fromColor; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Color From");
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_fromcolor##CrazyParametric")) { m_fromColor = {0.0f, 0.2f, 0.4f, 0.0f}; changed = true; }
        if (ImGui::ColorEdit4("##ToColorPhase", (float*)&_toColor, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_toColor = _toColor; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Color To");
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_tocolor##CrazyParametric")) { m_toColor = {0.6f, 0.8f, 1.0f, 1.0f}; changed = true; }
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
        if (value.contains("fromColor"))
        {
            auto& val = value["fromColor"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_fromColor = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        if (value.contains("toColor"))
        {
            auto& val = value["toColor"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_toColor = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["fromColor"] = imgui_json::vec4(ImVec4(m_fromColor.r, m_fromColor.g, m_fromColor.b, m_fromColor.a));
        value["toColor"] = imgui_json::vec4(ImVec4(m_toColor.r, m_toColor.g, m_toColor.b, m_toColor.a));
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
        // if show icon then we using u8"\ue162"
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
    ImPixel m_fromColor {0.0f, 0.2f, 0.4f, 0.0f};
    ImPixel m_toColor   {0.6f, 0.8f, 1.0f, 1.0f};
    ImGui::ColorPhase_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4202;
    const unsigned int logo_data[4204/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa8a22820, 0x4ec7776b, 0x1b9da6d2, 
    0x19db986c, 0xf46800ff, 0x4d1a00ff, 0xf40b5cd9, 0xede12f57, 0x51e3fa6e, 0x26a12efb, 0xd13d36ef, 0xf6fb42e5, 0xaef2c71d, 0xa46494a2, 0xa22880ae, 
    0x280aa08a, 0x280a80a2, 0xf1d53aaa, 0xa7a9f4d3, 0x3626db56, 0x3dda7fc6, 0x7693c63f, 0x15f50257, 0x5b7b78cc, 0x7ed4b8be, 0xbb49a8cb, 0x79748fcc, 
    0x87fdbe50, 0xa7abfc71, 0x2b2919a5, 0xa260b5a1, 0x14842a8a, 0x14004551, 0x8bf55451, 0x5369b0df, 0x6312a74e, 0xa3fd676c, 0x577693d2, 0xc5ee4a63, 
    0xf09b2bca, 0x7379b3f6, 0xf75b1ba8, 0xba47e62d, 0x7e5fa83c, 0x55feb8c3, 0x30b51ed2, 0x1cba929a, 0x5114e2a2, 0x45215945, 0x45015014, 0xbb555715, 
    0xd3543a16, 0xea3e8983, 0x94d2731f, 0xcaaea2a4, 0x91dd5c84, 0xf0c08a6a, 0x7573affe, 0x9c6f6d7e, 0xe81e99b7, 0xfb7da1f2, 0x57f9e30e, 0x15094640, 
    0xba9a9a30, 0x37dc742a, 0xab288a12, 0xa2280a33, 0x7ce40a80, 0x79af7a61, 0xb273646f, 0x7b4c9e3f, 0xbff500ff, 0x9dd5759d, 0x5a04890e, 0x5946eab3, 
    0xc3f39964, 0x9c3e0363, 0x3451717a, 0x8e86ace4, 0x2dd6514f, 0x6fedd4a6, 0x2696a36c, 0x80c10080, 0xd0f10019, 0x755764fa, 0x293ca90c, 0xea28672c, 
    0x6a501f18, 0x874f470d, 0xd336b453, 0x20480a96, 0xa54750af, 0x6cd1c63a, 0x49b6e36c, 0x8031d61d, 0xe1c7195f, 0x6e71314a, 0x8aa258e0, 0x1405412b, 
    0x72054051, 0x57bd2f5e, 0xe7cc82bb, 0x983c7f64, 0xeb00fff7, 0x6beb3a7f, 0x08161d3a, 0x8cd45775, 0xe73349b2, 0x7d06c686, 0xa2e2f438, 0x435672a2, 
    0xa829c74e, 0x6ad316eb, 0x51b6b776, 0x00401349, 0x800cc060, 0x327de878, 0x548ab92b, 0x3396149e, 0x0f0c7594, 0x1b35a8b1, 0x433b7508, 0x4921316d, 
    0x08ea1504, 0x2ddad8a7, 0xc976a48d, 0x30c6ba23, 0xfc38e30b, 0x2d2e4229, 0x9edc6ddc, 0x09ad288a, 0x80a2280a, 0x597ce50a, 0x0575af7a, 0xfec8ce99, 
    0xffef3179, 0x75fed600, 0x1a7c56d5, 0xafea103c, 0x922499a8, 0x8c0dcf67, 0xe971fa0c, 0x928bd459, 0x270d2eb2, 0xfaf77276, 0x95fab4c5, 0x5294e5ad, 
    0x1880d044, 0x74200330, 0x2b327de8, 0x9a448eb6, 0x94339624, 0xd8a70c75, 0x948d3ad4, 0xb6a19d5a, 0x0449219b, 0x5204f532, 0x952d5ad9, 0x8e2c5ba2, 
    0xbe0083ea, 0x94c28f33, 0xdcc92823, 0x13699272, 0xa91545d1, 0x14455198, 0xe2355700, 0x886bc57b, 0x464ace6c, 0x7f8fc977, 0xaef3b7fe, 0xe9dba896, 
    0xbea9c150, 0x499264a0, 0x30363c5f, 0xa5c7e933, 0x512e5665, 0x35a336b2, 0xd59c5d09, 0x716db3ee, 0xd95d5ba9, 0x10462443, 0xc8c00603, 0x717b3a1d, 
    0x4aba925d, 0x2867248b, 0xb14f19ea, 0x680bb5a8, 0x6f432bf5, 0x08924236, 0xa408ea65, 0x7616b5b3, 0x3bb26ea9, 0xe30b70aa, 0x09a7a938, 0x32fb4e46, 
    0xa28cd4aa, 0x8a9edc92, 0x70cead28, 0x00288aa2, 0xf71f8aa2, 0xc4cdbc70, 0xe8630968, 0xe3963629, 0x1464b749, 0x90973654, 0xd216165f, 0x58068579, 
    0x53e78f60, 0xaea4a450, 0x4709e598, 0x14052b49, 0x76914c51, 0x8a96b6d1, 0x1b6d1342, 0x18c56969, 0xfb11f342, 0x9e6319b4, 0x5d499b94, 0x76522a8e, 
    0x36da6648, 0x974a5bd2, 0xdeb98585, 0x628c6450, 0x8c334a9f, 0x39eee295, 0x92767042, 0x6db4ddb0, 0x91a8a2a5, 0xb3d17b76, 0x2b459dde, 0x67377693, 
    0xe93d1bbd, 0x23c122d5, 0xfe2af3c2, 0xc8f13eed, 0x8a9293e2, 0x4a4565bb, 0x2157d14e, 0xcf46efd9, 0x6b04927a, 0x2f48de9d, 0x4d8164b4, 0xcd488da9, 
    0x9ce32e5e, 0x2b690727, 0x46efd90d, 0x15757acf, 0x4fec2277, 0xd2affd2b, 0xaffd2b8f, 0xa7a2a4d2, 0x91998b99, 0x7eed5ff9, 0xed5f7994, 0x4f25957e, 
    0x42f39c15, 0x449946f3, 0x1870c7fb, 0x55f994a9, 0xcfe3b0db, 0x54ea6827, 0xfddabff2, 0xdabff228, 0x56d42afd, 0xc718cc93, 0x27a36d19, 0x504a1715, 
    0xc1ab19ab, 0xc6a9d0df, 0x9aa41da4, 0xbff222f5, 0xf228fdda, 0x2afddabf, 0x99f92a4a, 0x45afcc1c, 0x81865614, 0xd1edfa59, 0xd1164d8f, 0xc687fbfd, 
    0x00ff873d, 0x58a1155f, 0x6a5cda52, 0xefa7d3da, 0x163e8a20, 0x3b8e4356, 0x9feb9d8e, 0x9725df12, 0x6f07af53, 0x4bd0776b, 0x3f3c695b, 0x1c89dbad, 
    0x56184dba, 0xfa83e73e, 0xce07bae2, 0x3dc7c8f6, 0xf0bf9e2b, 0xbfadcded, 0x64b47fda, 0x083864bd, 0xdc03e454, 0xd2666b72, 0x22adeee1, 0x72644496, 
    0x206530a0, 0xb3d271e4, 0x7132dec3, 0xce2aaeb1, 0x26f55d9c, 0xb02b8aa2, 0x4551c7f3, 0x830a8914, 0xe931b953, 0xbf4f679f, 0x38fc983c, 0x54eb00ff, 
    0xd3b698f5, 0xa1ab7a6a, 0xf071042f, 0x719c32b2, 0x5cef74dc, 0x95fdb698, 0xb03bbba3, 0x7dd7f616, 0x4ddb5907, 0xf9f06aa0, 0x541219d2, 0xd7739f00, 
    0x4b6ac5f5, 0x1b89359f, 0xd2e32449, 0x3dbc2eb2, 0xe4269435, 0xc9786fea, 0x20a742c0, 0x5a93e31e, 0xdcb5dc30, 0x23a33309, 0x2e830195, 0x2b1d470e, 
    0x39e32d2c, 0x6d6eda46, 0x45a9b48c, 0x51f451df, 0x96077a45, 0xa9288a3e, 0xa7492a24, 0x09b4a529, 0x1f93e6f7, 0x00ff0786, 0x2da8a35a, 0xbe509321, 
    0x08340b91, 0x38ec0a87, 0x3b1d75e0, 0xcb3d0ed7, 0x77c7f2d8, 0xd665e8a1, 0x91bde9f6, 0xc6ad8d35, 0xe7c7a987, 0x505966cc, 0xfdf5c40f, 0x93cf5271, 
    0x6997853c, 0x4a5f492c, 0xda802ea9, 0xeae4667c, 0x8057768d, 0xd4efe998, 0x6a00ffd4, 0x32b38a37, 0x210092b2, 0x1d470e86, 0xe3151c2b, 0x9bb65052, 
    0x2aade39b, 0xf5f59550, 0x154551b8, 0xfa401eea, 0x90a0a228, 0x8a790bad, 0x7d0a4fe9, 0xe1c765e9, 0x36f55981, 0x34778d96, 0x2a79d2a8, 0x0cbb42ac, 
    0xc68f1c76, 0xcbc9fcbc, 0x77c738d8, 0xd665e8a1, 0x91bde9f6, 0xefd6c26a, 0xb45fbe45, 0x201267ce, 0xfabd1e18, 0x5caae2fb, 0x975237ee, 0x925c8c42, 
    0xe534b7a7, 0x1a9886d0, 0x7263b5e9, 0x31666354, 0x6b324dfd, 0xe749b7bf, 0x06902bed, 0xc6f16018, 0xe1050c6b, 0x9bb67056, 0xa5757ca3, 0xbdbe1246, 
    0x283a72af, 0xc873bda2, 0x2857d12b, 0x8a3ceda7, 0x57d129e7, 0x79daa328, 0x8a4e3907, 0xd31e45b9, 0x74ca39c8, 0xf628ca55, 0x55ce419e, 0x53d1aea2, 
    0xf6897ced, 0xed2a2a85, 0x205f7b14, 0x2a2a85f6, 0x5f7b14ed, 0x2a85f620, 0x7b14ed2a, 0x85f6205f, 0x15e52a5a, 0x9ec8db1e, 0x72159d72, 0x90b73d8a, 
    0xabe89473, 0xbced5194, 0x45a79c83, 0x6d8fa25c, 0x6ae51ce4, 0x9f15d52a, 0x3917f2b7, 0x45b58a56, 0x39c8df1e, 0xaa55b4ca, 0x41fef628, 0xada255ce, 
    0xf2b74751, 0x281a720e, 0x5618b5a2, 0x696799be, 0xf73c972e, 0x7d756f11, 0xd81177aa, 0xadc81a7a, 0xf6612e5d, 0xabf28c37, 0x85fce626, 0x5d156670, 
    0x76923b2c, 0x9a026e67, 0x2b295da9, 0xf48874a1, 0x224956ad, 0x1452e448, 0x8c204910, 0xf9f164d6, 0x0e441237, 0x3e578eed, 0x65a135b8, 0x57eadbae, 
    0x5eec6d61, 0xc5fc2ad8, 0x8c7b1c4f, 0x566aab62, 0xf17c6de9, 0x8293c5c4, 0xcdc8eb09, 0x93d59761, 0x4dcaa994, 0x86bebbdb, 0xf671dad8, 0xb514a971, 
    0x15f8adec, 0xf58aa268, 0x147d308f, 0x15125951, 0xd669a2bd, 0xf3643a37, 0xd6bdc3dc, 0x1177aa42, 0xc11a7ad8, 0xf2c387ae, 0x54f9ec0f, 0x485216f2, 
    0x637105fc, 0x1c54baea, 0xf03ab393, 0x93ac5370, 0xda566942, 0x47d2ae26, 0x282b520c, 0x46902407, 0x61c5777a, 0x5ce4c5dc, 0xde6d08cb, 0xaecf955b, 
    0x4c7b2b0e, 0x2ed560d7, 0xede9504c, 0xccfc026c, 0x00e6c78d, 0xd6ea64c5, 0x32ead9b1, 0x411613c3, 0xf23c0982, 0x2a605833, 0x945329b7, 0x4d7db79b, 
    0x248f7071, 0x5abe1467, 0x8aa2e814, 0x753c0ff5, 0x92595114, 0x77f8d115, 0xd3bdb04b, 0x0fee9e27, 0xa042d231, 0x07c661ef, 0xba72aea1, 0x1ab80abf, 
    0x818ce05c, 0xc07f2429, 0x2a637245, 0x5b5b743a, 0xa968389d, 0xd38d4954, 0xac3f7c2c, 0x43dbb1f9, 0x9201322a, 0xe9198159, 0xd55c91d4, 0xba673fd4, 
    0x84db009a, 0xd7bbc96e, 0x69d51507, 0xad1aec1a, 0xa7c35bc3, 0xc9ecaa35, 0xcc8fdb68, 0xf65c8301, 0x8e1d67b3, 0x31112ca9, 0x93201864, 0x65cdc893, 
    0xa5dca883, 0x996fef06, 0x912b22a6, 0x29f42b4a, 0xe8154551, 0x45d1421c, 0xba829815, 0x1a690c8f, 0x3c99fe75, 0x63fe96f7, 0xde41a5a4, 0xd8038cc3, 
    0x9f5d2fd7, 0x4d1a5b83, 0x984046c6, 0x22e03f92, 0xe413f1b9, 0xa037daa6, 0xcd989d93, 0xe6f04e37, 0xa4a5b2b4, 0x06c8c612, 0x67046649, 0x724552a7, 
    0x6b8b5097, 0xb80da0b9, 0xef262345, 0xbb661c5c, 0x835d172d, 0xa2ed9956, 0x76d55ad3, 0xe3365e12, 0xd76000f3, 0x47d9ad2f, 0x5bd3eaa7, 0x1090c5c4, 
    0x91272741, 0x360fcf9a, 0xb1778bdb, 0x52e59275, 0xa2280a45, 0x1a520ebb, 0xbd5ffe29, 0x74bd5f1e, 0x347758fb, 0xb45619ba, 0x19d3e6cb, 0x5d4810d9, 
    0xa7e77c8b, 0x0795fe18, 0x9747ef97, 0xf6d459ef, 0xd4521255, 0x379555b8, 0x771abb78, 0xb9b8213e, 0xc6e4c784, 0x27c3e899, 0x1c379015, 0x27767697, 
    0xa9a99b25, 0x3c7abf3c, 0x54517abf, 0xad682e61, 0xcd799d4a, 0x65e84e5a, 0x2f00ff14, 0xde2f8fde, 0xeeb0f6b5, 0x52147465, 0x6fa3f7ed, 0x23ed67bd, 
    0x57899bdc, 0x4d5aadf4, 0x1a475c25, 0x65b7bdc9, 0xfd314ecf, 0x7adf8e2a, 0x73d6fb36, 0x963455f6, 0xa0a4c2a5, 0x5c6d16ef, 0x78eeaa78, 0x4222088a, 
    0x1527377a, 0xc5eece88, 0x637776e4, 0xd2d4cd92, 0x6fa3f7ed, 0x31aa28bd, 0xa5563497, 0xade6b44e, 0xbb142526, 0xefdbe87d, 0xf748fb5a, 0x51b4b832, 
    0x11cf6745, 0x1ae96905, 0x4e44dae4, 0x6f924491, 0x7872d97d, 0x59e90fe0, 0xd94f5494, 0x332ae2cf, 0xd0897771, 0xd91be34d, 0xf0b62863, 0x931b3da1, 
    0x4992c18a, 0x95a49126, 0x961cbbcb, 0x8a364d3d, 0xf9a25451, 0xa44ae596, 0x51d8aca4, 0x3ca71545, 0x92a2c54c, 0x02648e8a, 0xe28fb6d6, 0x1e68f409, 
    0x408ea128, 0x2797bcef, 0x95fe008e, 0xb94c4591, 0x27e3b825, 0xb2747417, 0x48fadaf8, 0x08bca5ca, 0xe3e4464f, 0x597aaef5, 0x96569ea4, 0x9c232f67, 
    0xa3a61eb3, 0x4e5092a2, 0xa59c72e8, 0x4a8a16bb, 0x2291f92a, 0x221545d1, 0xa263bf0a, 0x2d9c3aea, 0x9abf9d2d, 0xde49db8a, 0x35f107a3, 0x048fbb42, 
    0x20f7e864, 0xc731131c, 0x672af2fd, 0x4a73552e, 0x99955271, 0x8de149ce, 0xd62e2466, 0xb5eb402c, 0x0de48fd5, 0xc1411065, 0x2bbd2218, 0xf4d4b349, 
    0xafaf99f4, 0x1420e90d, 0x277ed441, 0x50fbbcd6, 0xe5461d32, 0xedc288a5, 0xff8c312b, 0x714e0e00, 0xdc72124a, 0x495190aa, 0x5114b5a2, 0x51146256, 
    0xafc24845, 0x8e9a6858, 0x674b0ba7, 0xb6a2e66f, 0xc1a877d2, 0xae514dfc, 0x3ad9c0e7, 0x0407c83d, 0x00ff71ce, 0x840b8a7c, 0x7366a554, 0x5a5f7872, 
    0x8bb50b89, 0x75ed3a10, 0x5903f963, 0x46701044, 0xd24caf08, 0x3e3df56c, 0xf4fa9a39, 0x440192de, 0x7de2471d, 0x13f5ce6b, 0x5aaed421, 0xd22e8c58, 
    0xf0cf18b3, 0x0e14e7e4, 0xa2495170, 0x415114bd, 0x45519498, 0xb05f0548, 0x4e1df5d0, 0xdfce9616, 0xa46d45cd, 0xfcc155ef, 0xefae504d, 0x1e9d80c0, 
    0x678203e4, 0xbe00ff38, 0xd9555445, 0xb3938a70, 0x2d7ca939, 0xda85c4ad, 0xed3ab0c1, 0x03f96375, 0x70102459, 0x4eaf0846, 0x3df56cd2, 0xfada293e, 
    0x0192def4, 0x3d461d44, 0xd679adcf, 0x953a64a2, 0x85114bcb, 0x196356da, 0xe29c1cfe, 0x3964a39c, 0x562645c1, 0x0c2a8aa2, 0xa0288ac2, 0x68d8af02, 
    0x0ba98e7a, 0xe66d674b, 0x77d2b6a2, 0x27fee0aa, 0x7957a8de, 0x8f4e40e0, 0xf3c10172, 0x91ef3fce, 0x62771555, 0x662715e1, 0x5bf85273, 0xb50b855b, 
    0xda756083, 0x06f2c7ea, 0xe02008b2, 0x9f5e118c, 0x7aead9a4, 0xf5b5537c, 0x0224bde9, 0x7b8c3a88, 0x9df35a9f, 0x2b75c844, 0x0b239696, 0x33c6acb4, 
    0xc53939fc, 0x73c84639, 0x5ab98a82, 0x33a8288a, 0xad288a16, 0xf8d91504, 0x4a137832, 0xe3598eb8, 0x1bc7b488, 0x15e10fd8, 0x285351c6, 0xe1322bf3, 
    0xb873572e, 0x0dbb34d2, 0x7dba6706, 0xd246396e, 0x7e148236, 0x5fae3959, 0x528bbe5d, 0x84b9e7d5, 0x17101b11, 0x63e00423, 0x28459f35, 0x0edd45c1, 
    0x41b2ba53, 0x99551445, 0x52511485, 0xc1d3ae30, 0xe9f11657, 0xf12c0937, 0x80635ac4, 0x8af007ec, 0xa8a0e8e2, 0xeedc954b, 0x4f2f7d74, 0xa77b27d0, 
    0x1594e3d6, 0x47216823, 0xe59a93e5, 0xb5f8ebb5, 0xa87b622d, 0x01b11141, 0x01241849, 0x147dd68c, 0xba530e7a, 0x144541b2, 0x51944050, 0x76054845, 
    0xb7b809de, 0x8eb9488b, 0x2de3886b, 0x0377c031, 0x347145f8, 0xa8cc4e53, 0xefdc95cb, 0x4f336d34, 0xa97b27d0, 0x05e5b835, 0x5108da48, 0xb9e664f9, 
    0x2dfe7a4d, 0xea9e584b, 0x406c4410, 0x00094652, 0x459d3563, 0x1cb2f436, 0x826475a7, 0x20a9288a, 0x008aa228, 0x4df0b72b, 0x455abcc5, 0x1c714dca, 
    0x0e38a665, 0x2bba1fe0, 0x2eaaa288, 0x5c46e5ce, 0xa379e7ae, 0x817e9a69, 0xad49dd3b, 0x462a28c7, 0xc48f42d0, 0x4db9e664, 0x4b2dfe7e, 0x10ea9e59, 
    0x52406c44, 0x63000946, 0x0e459d35, 0x72435657, 0x4541b2ba, 0x7f905414, 0x0000d9ff, 
};
};
} // namespace BluePrint
