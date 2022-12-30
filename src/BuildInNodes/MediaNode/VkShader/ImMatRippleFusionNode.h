#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Ripple_vulkan.h>

namespace BluePrint
{
struct RippleFusionNode final : Node
{
    BP_NODE_WITH_NAME(RippleFusionNode, "Ripple Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    RippleFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Ripple Transform"; }

    ~RippleFusionNode()
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
                m_fusion = new ImGui::Ripple_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_amplitude, m_speed);
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
        float _speed = m_speed;
        float _amplitude = m_amplitude;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Speed##Ripple", &_speed, 1.f, 200.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_speed##Ripple")) { _speed = 100.f; changed = true; }
        ImGui::SliderFloat("Amplitude##Ripple", &_amplitude, 1.f, 100.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amplitude##Ripple")) { _amplitude = 50.f; changed = true; }
        ImGui::PopItemWidth();
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
        if (_amplitude != m_amplitude) { m_amplitude = _amplitude; changed = true; }
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
        if (value.contains("speed"))
        {
            auto& val = value["speed"];
            if (val.is_number()) 
                m_speed = val.get<imgui_json::number>();
        }
        if (value.contains("amplitude"))
        {
            auto& val = value["amplitude"];
            if (val.is_number()) 
                m_amplitude = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["speed"] = imgui_json::number(m_speed);
        value["amplitude"] = imgui_json::number(m_amplitude);
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
        // if show icon then we using u8"\ue4d3"
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
    float m_speed       {30.f};
    float m_amplitude   {30.f};
    ImGui::Ripple_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3830;
    const unsigned int logo_data[3832/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xaaa22820, 0xd9bf976a, 0x3cd76cda, 
    0x41f94516, 0x26b55be8, 0xba05aeec, 0xdac3172b, 0x9fafbae4, 0x1c69cc1d, 0x404010b1, 0x26f12340, 0x528ba8b6, 0x51144057, 0x280a3045, 0x280a80a2, 
    0x6a128fac, 0x70ba6972, 0xf2641f68, 0x0e700830, 0xffc15307, 0x6ed23c00, 0x7a0decca, 0xf6f0972b, 0xa8717db7, 0x935097fd, 0xe81e9b77, 0xfb7da1f2, 
    0x57f9e30e, 0x52324a51, 0x51144057, 0x14055045, 0x14054051, 0x35894756, 0x38dd3439, 0x79b20f34, 0x07380418, 0xffe0a903, 0x37699e00, 0xbd067665, 
    0x7bf8cb15, 0xd4b8be5b, 0x49a8cb7e, 0x748fcdbb, 0xfdbe5079, 0xabfc7187, 0x2919a5a8, 0x280aa02b, 0x8a02a8a2, 0x8a02a028, 0x7ba9a62a, 0xcda69dfd, 
    0x5f64c173, 0xbb851e94, 0xe0ca6e52, 0x7cb1a25b, 0xaa4bae3d, 0xc6dcf1f9, 0x0411cb91, 0x3f020404, 0x886a6b12, 0x017425b5, 0x00531445, 0x00288aa2, 
    0x17c64bae, 0xb1a0aee1, 0x8969f924, 0x4f7fc725, 0x759d3fd3, 0x890e9db5, 0xeab35a04, 0x99645946, 0x0363c3f3, 0x717a9c3e, 0xace43451, 0x35598e86, 
    0x48fc789b, 0xf1aaf6b7, 0x55022943, 0xfcf53b70, 0xbaee1a3a, 0x48571dd5, 0x48e2d5b7, 0x8d5d67e7, 0xfd8cc990, 0x4570ab39, 0x6f91c0e4, 0x77a1d867, 
    0x508a5337, 0x01778b8b, 0x684551f4, 0x288aa220, 0xc43fae00, 0xf8f2b617, 0x9f29da8e, 0x79c4366c, 0xea79828d, 0xb0abfc78, 0x0e2f5bac, 0xf7ec0d47, 
    0xe31daf57, 0x2e1ece4d, 0xa927e714, 0x7ad39c35, 0x5407a321, 0xb6bfacd4, 0x656cef2c, 0x0e05b12e, 0x53c01536, 0xe87645fd, 0xa7aee8ca, 0xd620c32a, 
    0xfbe1a35e, 0x0c43db2b, 0x266d4330, 0x40241241, 0xae62f911, 0x65afd669, 0x92b41561, 0x68434c89, 0xedc8b860, 0x77931444, 0x5114cd02, 0xa228085a, 
    0x8d2b008a, 0xbdd525f1, 0x1304bfce, 0x6fc3d14a, 0x72ae2381, 0xf4e3e409, 0x6cb1caae, 0x371c39bc, 0xbc5eddb3, 0x38378d77, 0x9c53b878, 0x75d6a49e, 
    0x1ab29213, 0x4a4d7530, 0xd266fbcb, 0xcb52c6fa, 0x52a8031e, 0xf6f7c1b8, 0xb72bfc38, 0x75455746, 0x06195639, 0x0f1ff5b2, 0x18da5ed9, 0x691b8261, 
    0x22910832, 0x15cb8f00, 0x7bb54e73, 0xa4ad082b, 0x1b624a94, 0x47c60543, 0x9ba4206a, 0xa26816b8, 0x4541d08a, 0x5c015014, 0xc32f8c97, 0x4962415d, 
    0x4b12d3f2, 0xa69ffe8e, 0x6beb3a7f, 0x08121d3a, 0x8cd467b5, 0xe733c9b2, 0x7d06c686, 0xa2e2f438, 0x0d59c969, 0x366bb21c, 0x6f91f8f1, 0x86e255ed, 
    0xe0aa0452, 0x74f8eb77, 0xaa75dd35, 0x6f91ae3a, 0xce91c4ab, 0x211bbbce, 0x73fa1993, 0xc98be056, 0xcfde2281, 0x6eee42b1, 0x17a114a7, 0xe803ee16, 
    0x41d08aa2, 0x01501445, 0xca8c1445, 0x15ecce88, 0x38b16454, 0xd1025000, 0x0d71c351, 0x688296ca, 0x4a7050e5, 0xe10f6030, 0xa2005252, 0xb880298a, 
    0x535114a3, 0x8a510c70, 0x332a2428, 0x32aa0ab1, 0x28009c58, 0xc52806b8, 0xb8621936, 0x1c4bf052, 0x47090eaa, 0x29fc010c, 0x14035cd4, 0xb8288a62, 
    0x367adf06, 0xa5a8d3fb, 0xdb8d2b76, 0x7adf46ef, 0x52793275, 0x569e06de, 0x139c1adb, 0x013397d6, 0xb7d1fb76, 0xbcb4a2de, 0x2d2cbeb7, 0x0c0af3a4, 
    0xce1fc1b0, 0xe19aa3a7, 0xf4bedda8, 0x51a7f76d, 0x262eec4e, 0x7b367acf, 0xe6a8a8d3, 0x9eddb862, 0xa7f76cf4, 0x40112151, 0xa911bfd3, 0x307334c1, 
    0xdeb31bbb, 0x96f49e8d, 0x61d9a5d2, 0x6450de01, 0x479f628c, 0xec460d3b, 0xbd67a3f7, 0x98398a3a, 0x288a8e5c, 0xc50a8aad, 0x91ef45f1, 0x0e99fda6, 
    0x403bb965, 0x00ff9d1e, 0xaaadf1c3, 0x6eec2fe5, 0x3211aff5, 0x31bc2d01, 0x8c31ab0d, 0xa7e38ee3, 0x76a3ce7a, 0xa11d1ab2, 0xd9b5d1ee, 0xeddcc9f4, 
    0x9e2c0f94, 0xc6fcf684, 0x6bea2a7f, 0x2bf4d48e, 0x6f483efd, 0xfa9aba23, 0xc2ce7155, 0xeec87159, 0x19baae78, 0x5d12e87c, 0x3ba877a5, 0x548a6058, 
    0x0c16afee, 0x5a51147d, 0xa2285a88, 0x3e5941a0, 0x34f2bd23, 0x3227b3cf, 0x07d0b6dc, 0xfcf07fa7, 0x9bb95a6b, 0x67ad2bcb, 0x6c1e645f, 0xb230c410, 
    0xb8e33894, 0x6da2dee9, 0x9368c8da, 0x6ba30f43, 0xb9b3e993, 0x591e282b, 0xf9ed093d, 0xd155fe8c, 0x3db9a69e, 0xd3be4347, 0x3bf286a4, 0x56a46fb9, 
    0x5712ee03, 0x8ae78e1c, 0xcf9763ea, 0x57da2589, 0x86b5837a, 0xad0ba508, 0x51d43118, 0xd4216845, 0x82484551, 0x5ed935b2, 0xc6a76d63, 0x49bee671, 
    0xd76b05fd, 0xb52e6e35, 0xec79667d, 0x270c8de6, 0x003c77e4, 0xfbe01838, 0x2cbd899a, 0x81b32552, 0xdd763df4, 0xd1accb19, 0xe0a96fa8, 0x17baa2fe, 
    0x8e91f3ce, 0xa933577a, 0xb49dbae9, 0x7979794b, 0x8123c214, 0xefb982b3, 0x054357d0, 0xc75b5dc3, 0x3223a333, 0x04c3ca8d, 0xa5a1f41e, 0x284a32d0, 
    0x5a24b4a2, 0x41a0a228, 0x5f57b555, 0x3184d5ec, 0x955f1ef3, 0xd53b7e47, 0x13b3c59a, 0xc495da6a, 0xbec4f296, 0x4ea67c53, 0xc70e1c98, 0xa52753eb, 
    0xdae48e8a, 0xdae8336c, 0x37698ae1, 0xed1f8583, 0x4a7ffe03, 0x6b3e93d6, 0x4f923612, 0x7b89951e, 0x2fd9dea5, 0x21af6edb, 0x53066697, 0x6b4e3f82, 
    0xd4c51b5a, 0xfe954b21, 0x8efa1935, 0xa069280d, 0x45d1873d, 0xd1916415, 0xed555153, 0x43cc873c, 0x1e454d45, 0x8839c8d3, 0xa3a8a968, 0x310779da, 
    0x1435150d, 0xe6204f7b, 0xa2a7a222, 0x0bf9daa3, 0x9e8a8298, 0x90af3d8a, 0x53511073, 0xf2b547d1, 0x2a0a620e, 0xbef6287a, 0x4545cc41, 0xdba3a258, 
    0x57cc1379, 0x4751aca2, 0x620ef2b6, 0x8a6215bd, 0x7390b73d, 0x14abe815, 0x83bced51, 0xb18a8298, 0xf2b64745, 0x805e3117, 0xab00e800, 0xbced5114, 
    0x8caf9883, 0x3a00a0f5, 0x1e45b10a, 0x8a39c8db, 0x288a55f4, 0xcc41def6, 0x56144543, 0x8d5a0585, 0x9a4d8f60, 0x32c358e6, 0xeb492e1c, 0xa855a5db, 
    0x41812e6e, 0x5fb1736e, 0x8adf8173, 0x1d15d94d, 0xcd257acb, 0x8c26b09e, 0x1ec9e5c5, 0x0747ee3e, 0x44324dea, 0xce64e5f2, 0xaa19b176, 0x13d45a36, 
    0xbd6d3e6b, 0x8f1341bb, 0x1e3f72de, 0x1277a595, 0xa7c872c3, 0x8b287220, 0x436c09ba, 0x24531445, 0x59511475, 0x67a31592, 0x66d3b105, 0xec309ab8, 
    0x11cb858d, 0xce1a7ad8, 0x1fb8e2a9, 0xb5733067, 0xdf01735f, 0x54ec4581, 0xef682c77, 0x64226ba7, 0x47724c58, 0x0787858e, 0x99534dea, 0x3c9ea93c, 
    0x9f1163e7, 0xdae9525c, 0xa95a17dc, 0xde5a2d68, 0x0f3e1f37, 0x543afd1e, 0xb52df0b7, 0x9584c4e3, 0x9dd08c1c, 0xb9b4e4d0, 0xa0288a5a, 0x4551f481, 
    0x7dad2066, 0x5dd2d22e, 0x888b6b36, 0x0736b2b7, 0xe86147cc, 0x34b5226b, 0x74b4c0b9, 0xdc6555c6, 0x4d81d7c3, 0x4b760515, 0xac91a6a4, 0x2d14c9ac, 
    0xe08cc71b, 0x26f5ebb0, 0xc98f67b2, 0xee2c92b8, 0xeb73c5d8, 0xbdf45783, 0x41dd0b72, 0xb7361b96, 0x2ae7978d, 0x06ecf478, 0x6ca5be2a, 0x42f297b6, 
    0x118c4a8c, 0xbad1bc9e, 0xd1b5921c, 0xa4288a56, 0x4551b440, 0x9a5b2141, 0xc69d8d3d, 0x10d72c9b, 0x544890ef, 0x6060c41d, 0xebb0c61e, 0x8be4f0a0, 
    0x7d7769f6, 0xc89f97d4, 0xc1e58e53, 0x41a68b5d, 0xe2e5eaa2, 0x55460786, 0x07e6361f, 0xb022a99f, 0x2ef222ee, 0xef368465, 0xd7e7ca2d, 0xa7bb1507, 
    0x8dfa766b, 0x990d95d7, 0xcdfc4281, 0xc0fcc019, 0x569dac18, 0x19f52cd5, 0x208b8d61, 0xe4f504c1, 0xa9d89b66, 0x9da26b25, 0x64525114, 0xe5494543, 
    0x51fab57f, 0xfab57fe5, 0x3dd2fe56, 0x7444e6cb, 0x57b64896, 0x2333724f, 0x3ce627b9, 0x952fd50e, 0x47e9d7fe, 0xe9d7fe95, 0x7187b447, 0xf7d721f3, 
    0x4725fe26, 0x434e8550, 0x77815a2f, 0xb9dc016d, 0x538f1b03, 0xed5ff952, 0x5f79947e, 0x7b947eed, 0x7b0ef748, 0x7952d191, 0x947eed5f, 0x7eed5f79, 
    0xf7487b94, 0x451b3217, 0xfefbe549, 0xff7e7994, 0xb447a500, 0xea88ae88, 0xec5f3e4d, 0x1e559ef9, 0xf3133b63, 0x9747ed76, 0xe551faef, 0x7b94fefb, 
    0xc54a8d48, 0x4b4edd9b, 0x6a34c2c8, 0x1d861c84, 0xb9599e6a, 0xd9dd2e77, 0x977f9aba, 0xe551faef, 0x7b94fefb, 0x2b07f748, 0x7952d191, 0xa500ff7e, 
    0xe9bf5f1e, 0xae88b447, 0xe59fa284, 0xfbe5d1fb, 0x15f17cd6, 0xa6b3cad0, 0xce9ab6de, 0xec4282c8, 0xc6ede65b, 0xe543a53f, 0xfbe5d1fb, 0x2988e7d3, 
    0xd7bad058, 0x2f8cba67, 0xd1111a95, 0x352b4e86, 0x488bdd9d, 0x63f5dce5, 0xbfbcd3d4, 0x7abf3c7a, 0x8dfa441d, 0xa28cfbce, 0xd1fbe59f, 0xe7d2fbe5, 
    0xd186ae88, 0x46efd94f, 0x649e7acf, 0xbdcad04d, 0xa66deaa5, 0x24918a2c, 0xdf71dbbb, 0xeb00fff4, 0xf49e4d55, 0x9da3f76c, 0x1adb4a0d, 0x7992f8d3, 
    0xf096da63, 0xa2bc7fc6, 0x499224b2, 0x972bc95c, 0xeab1e476, 0xd17b7669, 0x3487deb3, 0xc67de7c6, 0xefd94f51, 0x397acf46, 0x51423791, 0x2a801445, 
    0x7d93a6e5, 0x6d4d0b7d, 0xdb8a9807, 0x9fa3de49, 0xeb3ad5c4, 0x91ce2b7c, 0x6682833a, 0xbe00ff38, 0xb2ab3845, 0x63671ea1, 0x510db40d, 0x18d9b750, 
    0x65ede485, 0xb306f227, 0xebc1c1c8, 0x6995ce5d, 0x31cd69a8, 0x6437bbbd, 0x5f47b181, 0x7c1bd7cc, 0x34dcdf5c, 0xa49d1891, 0x877f2866, 0xd5a63827, 
    0x488a5391, 0x541445af, 0x14458b99, 0xf52a8854, 0x6aa88b8e, 0x96b63451, 0x6d2b62fe, 0x1f8c7a27, 0xed1ad5c4, 0xec9f177c, 0x090e807b, 0xf9fee398, 
    0x65575115, 0xc7ce3cc2, 0x7586273f, 0x59bb9098, 0xd7ae0331, 0x35903f56, 0x04074194, 0x6df48a60, 0xd352d326, 0x5eb77fd6, 0x2840c21b, 0xb9c7a883, 
    0x5007aef5, 0xe1461d32, 0xedc488a5, 0xff8c312b, 0x714e0e00, 0x3964a344, 0xa25724c5, 0xc74c2a8a, 0x02154551, 0xa261bd0a, 0x2d9c3a6a, 0x9abf9d2d, 
    0xde49db8a, 0x9af883ab, 0x81cf5da3, 0x907b74b2, 0xe39c090e, 0x7115f9fe, 0x112e7657, 0x39677652, 0xa8f58527, 0xb158bb90, 0x56d7ae03, 0x9235903f, 
    0x60040741, 0x26cdf48a, 0xe3d353cf, 0x4dafaf9d, 0x411420e9, 0xfadc63d4, 0x26ea9dd7, 0xb45ca943, 0xa55d18b1, 0xe19f3166, 0xca29cec9, 0x149c4336, 
    0x51f4ca55, 0x14666645, 0x15004551, 0xd443c37e, 0x5b5a4875, 0x15356f3b, 0x57bd93b6, 0xf53ef107, 0x02cfbb42, 0x907b7402, 0x719e0f0e, 0x8a7c00ff, 
    0x17bbaba8, 0x333ba908, 0xdac2979a, 0xac5d28dc, 0xd7ae031b, 0x35903f56, 0x04074190, 0xfdf48a60, 0xd353cf26, 0xafaf9de2, 0x1420e94d, 0xdc63d441, 
    0xea9cd7fa, 0x5ca94326, 0x5d18b1b4, 0x9f3166a5, 0x29cec9e1, 0x9c4336ca, 0xd4ca5514, 0x98414551, 0xb5a228ca, 0xf0a62b28, 0x0b1bd194, 0x8ce37995, 
    0xb0374e99, 0xcc156d1c, 0x323b4dd1, 0x73572ea3, 0xbbb4d3aa, 0xbb560e5d, 0x518a5897, 0x0a016eb7, 0x6b4e1e3f, 0x8fbe570b, 0xbaa5d452, 0x001b6184, 
    0x3306be67, 0xbda1a854, 0xeaca812c, 0x8aa228c8, 0x14458b44, 0xaf2b8854, 0x2ef074f0, 0x4f927099, 0x71ca641c, 0x11fe80b9, 0xd314855c, 0x9751b9b3, 
    0x69dab92b, 0x818e5d5a, 0xb5bedd33, 0x366ea31c, 0xb2fc28e4, 0xb7d65c73, 0xaba5167f, 0x2208754d, 0x462e2036, 0x6ac6c009, 0xb4dc1485, 0xd5951bb2, 
    0x15455190, 0x8aa20e24, 0xda15042a, 0xdee22a78, 0x25e5263d, 0x4c8b389e, 0xfe801d70, 0x155d5c11, 0x2a777651, 0x3b77e532, 0xd34b1f9d, 0xe9de09f4, 
    0x05e5b8f5, 0x5108da48, 0xd79c8cf8, 0xc55faf2d, 0xdd136ba9, 0x888d0842, 0x20c1480a, 0xe8b3660c, 0x6475e5a6, 0x20ab2b37, 0x082a8aa2, 0x80a2280a, 
    0x13fced0a, 0x91166f71, 0x475c9372, 0x038e6919, 0x8aee07b8, 0x8baa28e2, 0x9751b9b3, 0x68deb92b, 0xa09f66da, 0x6b52f74e, 0x910aca71, 0xf1a310b4, 
    0x53ae3919, 0x528bbf5f, 0x84ba67d6, 0x14101b11, 0x18408291, 0x435167cd, 0xdc90d595, 0x5190acae, 0x1f241545, 0x0000d9ff, 
};
};
} // namespace BluePrint
