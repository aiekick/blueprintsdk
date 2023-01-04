#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <LinearBlur_vulkan.h>

namespace BluePrint
{
struct BlurFusionNode final : Node
{
    BP_NODE_WITH_NAME(BlurFusionNode, "LinearBlur Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Mix")
    BlurFusionNode(BP* blueprint): Node(blueprint) { m_Name = "LinearBlur Transform"; }

    ~BlurFusionNode()
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
        auto percentage = context.GetPinValue<float>(m_Blur);
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
                m_fusion = new ImGui::LinearBlur_vulkan(gpu);
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, percentage, m_intensity, m_passes);
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
        float _intensity = m_intensity;
        int _passes = m_passes;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Intensity##LinearBlur", &_intensity, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_intensity##LinearBlur")) { _intensity = 0.1f; changed = true; }
        ImGui::SliderInt("Passes##LinearBlur", &_passes, 1, 10, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_passes##LinearBlur")) { _passes = 6; changed = true; }
        ImGui::PopItemWidth();
        if (_intensity != m_intensity) { m_intensity = _intensity; changed = true; }
        if (_passes != m_passes) { m_passes = _passes; changed = true; }
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
        if (value.contains("intensity"))
        {
            auto& val = value["intensity"];
            if (val.is_number()) 
                m_intensity = val.get<imgui_json::number>();
        }
        if (value.contains("passes"))
        {
            auto& val = value["passes"];
            if (val.is_number()) 
                m_passes = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["intensity"] = imgui_json::number(m_intensity);
        value["passes"] = imgui_json::number(m_passes);
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
        // if show icon then we using u8"\ue427"
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
    FloatPin  m_Blur = { this, "Blur" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Blur };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    float m_intensity {0.1};
    int m_passes {6};
    ImGui::LinearBlur_vulkan * m_fusion {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 2782;
    const unsigned int logo_data[2784/4] =
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
    0xa08aa228, 0x93e2520c, 0x10a9b934, 0x3f2b8a62, 0x3636bf58, 0xe7fcd085, 0xaeec26a5, 0x56144333, 0xcdac8706, 0xe38a3177, 0x6fcded86, 0x49c988e6, 
    0xa598005d, 0xcda419c5, 0x5c8a0b31, 0x52339a51, 0x1d388a01, 0x7a597369, 0x1084e9ad, 0xdda440bd, 0x07631a90, 0x62c528a6, 0x4a835ae9, 0xb5f57e40, 
    0x00ba139a, 0x34a318c5, 0x05069866, 0x19cd282e, 0x69a110a9, 0xd2d45109, 0xaeb302e7, 0x9a6776ee, 0xe10bd84d, 0x62a7f494, 0xf7aeedb2, 0xaca8b59e, 
    0x7742830a, 0x29463101, 0x609a4973, 0xad288a32, 0xf5e20a8a, 0x3c1946fd, 0x59991047, 0x3020b4e0, 0x3ff5a45e, 0xda157ed0, 0xa29d7656, 0xcd5ca741, 
    0x2cc99272, 0x90e67db3, 0x829e5c8f, 0xb45c9cb3, 0xfd2d4743, 0x00ff6fb1, 0x366a3209, 0x397124eb, 0x1df85002, 0x743b3d78, 0xaa75dd35, 0x6f91ae3a, 
    0xce91c4ab, 0x211bbbce, 0x73fa1993, 0xc98be056, 0xcfde2281, 0x6eee42b1, 0x2e4214a7, 0xd107dc2d, 0x83a01545, 0xcda4b814, 0xb810a919, 0xbc5f57ae, 
    0xe2083559, 0x24872839, 0x204fea0a, 0x458bcc8a, 0xe21a6f89, 0x63b71c52, 0xb2da3451, 0xbce6391a, 0x29522d8a, 0xd0ab5260, 0x891365d7, 0x081d5723, 
    0xd1be50cd, 0x0ae3eead, 0xe28e10a8, 0x6dc25aae, 0x678b58e0, 0xa645286d, 0xa3d804ee, 0xd68c6614, 0xe2520c82, 0x909ad18c, 0x27558811, 0xf99bb9d2, 
    0x07925b9a, 0x419aae39, 0xd5fa0ab9, 0xf363d315, 0x92a83837, 0x772b0c6c, 0x40506778, 0xe240d415, 0xaabd9548, 0x4c6cbaac, 0x567195fb, 0x3d2a4fa0, 
    0x043445b4, 0x6614a398, 0x6080d58c, 0xd18ce252, 0x940d919a, 0x1dd68ced, 0xd683b3da, 0x971b24b7, 0x03ad5515, 0x812b999a, 0x9e8c6893, 0xdb6abbb5, 
    0x8babe6a3, 0x42ada630, 0xab2805bb, 0xc5287e0c, 0x5633692e, 0xb3d13621, 0x284a9dde, 0x377661e6, 0x79b43ecb, 0xa8a4d667, 0x8ea65d5e, 0xbc631776, 
    0x9647ebb3, 0x9b356a7d, 0x9153a726, 0xb20b3b47, 0x8dd6873d, 0xd1a62195, 0x4fecc2cc, 0x2f8fde2f, 0xa9409fde, 0x1bbb62e6, 0xe5d17be5, 0x8187d47b, 
    0xd1e03351, 0x5fecc2ce, 0x2b8fde2b, 0x261f9ade, 0x1c455ea6, 0xbcc82eec, 0xbf3c7abf, 0x51d2907a, 0x67ecc2cc, 0xe54bef97, 0x47a9d3fb, 0x2b669e5a, 
    0xbd57beb1, 0x3cbd5f1e, 0x3b4773d3, 0x589ed81d, 0xf228fdf5, 0x4f73e9bd, 0xbbb07314, 0xdef1f222, 0xa9f7cb93, 0xccd1a648, 0x962776c5, 0xfdf2683d, 
    0xb50ef4e9, 0x6357cc3c, 0x5f7abf7c, 0xa7f4df2b, 0x1d3b47d1, 0xdf2bcfd8, 0xfdf2a4f4, 0xe7284aea, 0x9717a961, 0xef9747ef, 0x394a1a52, 0xf9c8ae98, 
    0xa5a419a5, 0xec40a115, 0xcc65d642, 0x5e462bec, 0x164da856, 0x32646ae0, 0x9b192e08, 0xe5406aa5, 0x8360b396, 0x4068a507, 0x4c204a31, 0x38cd4973, 
    0x3ac46ad3, 0xa9284581, 0x021bd910, 0x06214da9, 0x553348ae, 0x2c358fa5, 0xc4ca5164, 0x229be8d5, 0x351f45aa, 0x108a316a, 0x534a1a12, 0xa5205449, 
    0x4729691d, 0xc608915a, 0xa93966a2, 0x65a3264f, 0x682893e6, 0x4d9d9a63, 0x4acdab44, 0x39402806, 0xe72953ba, 0x239932a5, 0xa0681da8, 0xea10a975, 
    0x1ad2d24c, 0x289a410a, 0x490380a2, 0x2428694a, 0x695e9a8f, 0xe35cb5f4, 0xd30411b9, 0x53ed594c, 0x8e169ce2, 0xaadc217f, 0x3cb50320, 0x6d631529, 
    0xf9731437, 0x698ec805, 0x254da939, 0xdab8e21c, 0x53494905, 0x238c2bce, 0x6a1ed334, 0x094f3360, 0x87fc399a, 0x8ac5b272, 0xe6545c70, 0x515c613a, 
    0x1817e4cf, 0xb4f42969, 0x74e48a73, 0xd4a0a4a2, 0xfbc885f3, 0x40a915d2, 0xe6b048cd, 0xb9207f8e, 0x8a382d58, 0x46c5a19d, 0xfc398acb, 0x360de382, 
    0x73b414a4, 0x5251c685, 0x719e8a62, 0x4f838e5c, 0x219e2603, 0x7f8ed263, 0x290db920, 0xb40c8ca9, 0xa375a4c2, 0x472ec89f, 0x8e963e45, 0x141db970, 
    0xe0315451, 0x9c2a5271, 0x4ca51753, 0x1b1a28bd, 0xa908d3b4, 0x6ca026aa, 0x9b53d288, 0x478236ad, 0x435a1a52, 0x8cf12152, 0x3dd398d5, 0x8a7108aa, 
    0x685a09bb, 0x71d146a4, 0xd6baabd2, 0x35edb283, 0x6328424e, 0xb4f45e65, 0x2052b41e, 0x484b435a, 0x0a49446a, 0x0841afe6, 0xb19daa3d, 0xb3a5d600, 
    0xa2293da8, 0x183445e2, 0x6446955e, 0x898badc0, 0xcb4aaf10, 0x4d8320b8, 0x4b414583, 0x40524b49, 0xb834a4b9, 0x1d2235a4, 0xadf8dc10, 0xc47d7b3b, 
    0x04b67d56, 0x5768b7b5, 0xc445aa70, 0x68d3e28a, 0xdb84cbac, 0x21e31d5d, 0x3bacf441, 0x4471a4dd, 0x148ae490, 0xb514a4b9, 0x53746426, 0xdeb751b6, 
    0x5d45e6b4, 0x8a543c00, 0xa57854dc, 0xe80e99a3, 0x9a307593, 0x3a47314a, 0xd234640e, 0xb2d1b653, 0xa02b648e, 0xbeed34a4, 0xa9f76cf4, 0x04ba42e6, 
    0x93e3a938, 0x535c0015, 0xe81c4586, 0xd2b0c87c, 0x66335071, 0x07932693, 0x1c748ed6, 0x5a7a67c8, 0x6df49e5d, 0x0a99a3f7, 0x3f0d29e8, 0x7acf266d, 
    0x8b2b649e, 0x51ab6d1b, 0xd954454c, 0x1d194eef, 0x5243e7e8, 0x453e492e, 0x49f39154, 0xddf41ef3, 0xa073f4be, 0x4b4b1b72, 0x6d97deb3, 0x1357c81c, 
    0xe59f8634, 0xfbe5d1fb, 0x5d21f3d4, 0x7c6d1b09, 0x0a6ef8d5, 0xf251cd91, 0x9142e9fd, 0xa17354fc, 0x9ad258a9, 0xaaf5b2ec, 0x981a4a13, 0xa4e21f43, 
    0xb9e9fdf2, 0x655cb9a1, 0x5fde692e, 0xbd5f1ebd, 0xd04d642e, 0x47735294, 0x64a00034, 0x63c3ade2, 0x54ad63e6, 0xeb17998b, 0x1c9f365d, 0x9cc25d78, 
    0xb98acb55, 0xb09d268f, 0x24a79a71, 0xaec13666, 0x4a28eeba, 0xcc15a3fc, 0x97ec31ea, 0x1c56728a, 0x14ad62a3, 0xa97969de, 0x25451d33, 0x19282015, 
    0xfaf6ab38, 0x81bbce53, 0xdf27a822, 0xf4ec5a1f, 0xd0c73eb0, 0x8aaaf474, 0xce8d50b9, 0x481e5d76, 0xac3970d7, 0x548c52f7, 0x4adc15f5, 0x00d8ed15, 
    0x32aef433, 0xb5dc4af5, 0xa31c5612, 0x28291a62, 0x147520a9, 0x5a885494, 0xdda56db7, 0x18bec7dd, 0xae532df7, 0xce01c3eb, 0x8633c288, 0x8aaac1c0, 
    0xe6112ebb, 0x74493976, 0xb350e85b, 0x892a7042, 0xa21e1c04, 0x6edd1abd, 0xed9bb363, 0x1c9cb22c, 0x7f5e7300, 0xc9b59f79, 0xe996db95, 0x4eb0514e, 
    0x92a22136, 0xd107828a, 0x0a221545, 0xf6f0b3d2, 0x2d5ba8ab, 0x99afadcd, 0xe60d6713, 0x3a0ece28, 0xe8b55913, 0x598e1a7e, 0xc1b11cbc, 0x892b9527, 
    0x4ef73f02, 0x085783e3, 0xa90897dd, 0xb990333b, 0x69b1def0, 0x93354d0b, 0x55925104, 0x023fb095, 0x3ebd2e6b, 0x4e471dd2, 0x54fb66d2, 0x32cea8ad, 
    0x1dd35755, 0x625eeb39, 0xb08a55ca, 0x0f828320, 0x0e1b276a, 0x94b05171, 0x99414551, 0xd19c141d, 0xe8506ccd, 0xef5324ce, 0xd28d155d, 0xd78c5c20, 
    0x6a0e3234, 0x8fa4ee78, 0x54ec34a5, 0x7ba28e5d, 0xdcfdcad8, 0x48a83d57, 0x9ea8f81d, 0x031557f6, 0x34c93933, 0x4a39aedc, 0x342745e1, 0xea405273, 
    0x01a92829, 0xd687f7cb, 0xea36cdba, 0x9eb46b21, 0x358f5c71, 0x834cdc22, 0x4e53e40a, 0x3b5646c5, 0x0cc5b50f, 0x3507bb4d, 0xac126acb, 0xa59705b7, 
    0x3833d740, 0xcea839c3, 0x842b374d, 0xa46871a5, 0xd491a4a2, 0x21525152, 0xe4f0b56b, 0xe2159d91, 0x03312a33, 0x5213d71c, 0xc2a0f224, 0x8d3dca48, 
    0x51995d54, 0xad3b2b97, 0x324e173d, 0x247a714b, 0x73829353, 0x3cad7e5c, 0x92841a77, 0x6a523e44, 0xc34c5cab, 0xf511230d, 0xba72e8a8, 0xab2be5b0, 
    0x1525450b, 0x8aa20f24, 0xde15442a, 0x3f6b0d68, 0x25c5fe83, 0xabb450dc, 0x32cc1222, 0x669c27b9, 0xa32a3ab8, 0x7219152b, 0x9834d0b3, 0xc10cdf34, 
    0x09eb923c, 0x503e7038, 0x287a1c40, 0xefe19a27, 0x50f356ee, 0xda55b9b8, 0x07ccc8b2, 0xd1ab26a0, 0x87d59543, 0x28585d29, 0x0f928aa2, 0x0000d9ff, 
};

};
} // namespace BluePrint
