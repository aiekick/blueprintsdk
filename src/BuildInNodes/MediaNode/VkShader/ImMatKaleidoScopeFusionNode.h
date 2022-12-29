#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <KaleidoScope_vulkan.h>

namespace BluePrint
{
struct KaleidoScopeFusionNode final : Node
{
    BP_NODE_WITH_NAME(KaleidoScopeFusionNode, "KaleidoScope Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    KaleidoScopeFusionNode(BP* blueprint): Node(blueprint) { m_Name = "KaleidoScope Transform"; }

    ~KaleidoScopeFusionNode()
    {
        if (m_fusion) { delete m_fusion; m_fusion = nullptr; }
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
                m_fusion = new ImGui::KaleidoScope_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_speed, m_angle, m_power);
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
        float _angle = m_angle;
        float _power = m_power;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Speed##KaleidoScope", &_speed, 0.1, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_speed##KaleidoScope")) { _speed = 1.f; changed = true; }
        ImGui::SliderFloat("Angle##KaleidoScope", &_angle, 0.0, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_angle##KaleidoScope")) { _angle = 1.f; changed = true; }
        ImGui::SliderFloat("Power##KaleidoScope", &_power, 0.0, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_power##KaleidoScope")) { _power = 1.5f; changed = true; }
        ImGui::PopItemWidth();
        if (_speed != m_speed) { m_speed = _speed; changed = true; }
        if (_angle != m_angle) { m_angle = _angle; changed = true; }
        if (_power != m_power) { m_power = _power; changed = true; }
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
        if (value.contains("angle"))
        {
            auto& val = value["angle"];
            if (val.is_number()) 
                m_angle = val.get<imgui_json::number>();
        }
        if (value.contains("power"))
        {
            auto& val = value["power"];
            if (val.is_number()) 
                m_power = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["speed"] = imgui_json::number(m_speed);
        value["angle"] = imgui_json::number(m_angle);
        value["power"] = imgui_json::number(m_power);
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
        // if show icon then we using u8"\uf2b0"
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
    float m_speed       {1.0f};
    float m_angle       {1.0f};
    float m_power       {1.5f};
    ImGui::KaleidoScope_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3992;
    const unsigned int logo_data[3992/4] =
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
    0x8a16a08a, 0xa200a928, 0xdda9ab8a, 0x964d8f7d, 0x00ff7060, 0xa1b90f75, 0x6a812bbb, 0xf5d0c28a, 0xe38b9b5b, 0xb7e5786f, 0x81cae4a6, 0x1577d8ef, 
    0xa33478ba, 0x01742525, 0x01531445, 0x928aa268, 0x2b2a8a42, 0x84b5c5d9, 0x8b7ef492, 0xb464a0f5, 0x7f915e56, 0x82d9c52d, 0x9bde89e5, 0xf30f2093, 
    0x82916ac5, 0xaeee2445, 0x14455102, 0x288a3ac4, 0x8aa200a9, 0x0d8bc465, 0x177f929c, 0x03d0fa45, 0x6d5aaae8, 0x62704fdb, 0x72b99398, 0xa4ab18b9, 
    0x49e99160, 0x4551803b, 0x8a8ec014, 0x0a8aad28, 0x7a617ce4, 0x646f79af, 0x9e3fb273, 0x00ff7b4c, 0x759dbff5, 0x890e9dd5, 0xeab35a04, 0x99645946, 
    0x0363c3f3, 0x717a9c3e, 0xace43451, 0x514f8e86, 0xd4a62dd6, 0xa36c6fed, 0x00802696, 0x001980c1, 0x64fad0f1, 0xa90c7557, 0x672c293c, 0x1f18ea28, 
    0x470d6a50, 0xb453874f, 0x0a96d336, 0x50af2048, 0xc63aa547, 0xe36c6cd1, 0xd61d49b6, 0x195f8031, 0x314ae1c7, 0x58e06e71, 0x412b8aa2, 0x5414450b, 
    0xe23b5780, 0x886bb54b, 0x27764e6d, 0xfff7f8ce, 0x3a7feb00, 0x3a94aae8, 0x2fea5064, 0x56de5d7c, 0x0f8c0dcf, 0x4d3315a7, 0x3d4720ab, 0xcddbaa79, 
    0x69756f7f, 0x80989114, 0x810c1c04, 0x75b5a7d3, 0x2c22eb88, 0x03ae7288, 0x3b6aec03, 0x0bf558eb, 0x0ab90463, 0xa05e3948, 0x2d6c69d1, 0x7605922d, 
    0x6e014e75, 0x609aa2b4, 0x5514454d, 0x4551b480, 0xfa582149, 0xb1b4cacd, 0x43ed9cdb, 0x7dfef1b9, 0xc3ab622b, 0xad371c65, 0x469eb977, 0x05c6decf, 
    0xc36a934c, 0xa5c63546, 0xcf90d704, 0x63c6136d, 0x9ce31c00, 0x0ca12b7d, 0xe5d0a1ae, 0x360d0258, 0x80bc25ee, 0x9c0027c3, 0xdb1475e4, 0x0bbc0578, 
    0xbd607610, 0x26a1750b, 0x45511298, 0x451d4215, 0x56805414, 0x2e38a17e, 0x53ed9cb0, 0x155ab1b9, 0x0dc9360c, 0x666e5ccb, 0x29e8f576, 0x74467035, 
    0x754df1b7, 0xc846b01c, 0x15fe0c50, 0x159c4aae, 0x29325239, 0x84b9402e, 0x9c00e3c6, 0xc81475e4, 0x1162f262, 0x73072c86, 0x639a84d6, 0x45519464, 
    0xefd90d31, 0x7d7acf46, 0xbb307314, 0x8ddeb319, 0x30faf49e, 0x11fe1776, 0xc2ccd1d4, 0x7acf66ec, 0xc6d27b36, 0x39ec5cc2, 0x1c755ac7, 0x6cc62ecc, 
    0xbd67a3f7, 0x98398a3e, 0xefd98d5d, 0x7d7acf46, 0x57cc3c15, 0xd17b3663, 0x4b9fdeb3, 0xd82dedb4, 0x8599a375, 0x7acf1ed9, 0xd3d37b36, 0x71decee7, 
    0x2ecc1c45, 0xa3f76cc6, 0x8a3ebd67, 0x895d9839, 0x9e8ddeb3, 0x792aeaf4, 0xecc6ae98, 0xbd67a3f7, 0x8204973a, 0x98390a7b, 0xefd98c5d, 0x707acf46, 
    0x4573ba1b, 0xc62ecc1c, 0x67a3f7ec, 0x398a3abd, 0xb3895d98, 0xf49e8dde, 0x98792aea, 0xf7ecc6ae, 0x3abd67a3, 0xc2cc518c, 0x7acf6eec, 0xa8d37b36, 
    0xd88599a3, 0x6cf49edd, 0x4751a7f7, 0x94b10b33, 0x50684551, 0x15327754, 0x57c7f2b7, 0x08ab926a, 0x86e5e6de, 0x18f02a4a, 0x58684c8a, 0xa41b7ba3, 
    0x2883dbde, 0xf1be6515, 0x5235c7c8, 0x3c85075b, 0xa434b4cf, 0x00ffc776, 0x8624ac5e, 0x07410e54, 0x8562e41c, 0x28ea18d8, 0xa28598a2, 0x14442a8a, 
    0xda213e92, 0xa8a5b13a, 0x48695a95, 0x123a50c9, 0x1a1d1a28, 0xb9ebb435, 0x231506b7, 0x8e91e37d, 0x31b5166a, 0x7271fe0d, 0xfc81543b, 0x0f8218c0, 
    0x5a0c24bd, 0x8598a228, 0x442a8aa2, 0x3ce2a414, 0x9192e60e, 0xa0237641, 0xa262a0f5, 0x9c0c52b4, 0xa69e52e4, 0x37d86198, 0xa9bd9b79, 0x4073b7db, 
    0x40511405, 0x2a8aa285, 0xa9bd1444, 0x069a1c28, 0xd13c1500, 0x9c73dc46, 0x01d09cd1, 0x21501445, 0x42d55394, 0x031855a8, 0xe7aaa5a0, 0xa9e8c81d, 
    0x2e9ca328, 0x45494547, 0x3a72e11c, 0xe7284a2a, 0xa6a28c0b, 0x8dfbdff2, 0x796a53f9, 0x5474e4c2, 0x2bce5194, 0x5152d191, 0x8e5c3847, 0x398a928a, 
    0xad28e3c2, 0x0f499f45, 0x2b0349f6, 0x24e38d81, 0xb53ed367, 0x40bafd0d, 0xa32461ea, 0x97da731c, 0x8a46d938, 0xe25c8a92, 0x25151db9, 0xc8857314, 
    0xa328a9e8, 0x8a322e9c, 0x79b7bdda, 0x06372c14, 0x03d0cd3e, 0xdf05d3f8, 0xb3bcadd9, 0x1d38e773, 0xf4f5f37a, 0x8c7369fc, 0x19a4a2c8, 0x033449ee, 
    0x7a2008ce, 0xe426e7d2, 0xe4805474, 0x82339ae3, 0xf11d0932, 0x8e5c3847, 0x6cb4988a, 0xfa20a8a0, 0x51e47452, 0xa22317ce, 0x14862a8a, 0xa5a418a3, 
    0x830b00cd, 0x9e6c4049, 0x5a4f2906, 0xa2284a00, 0xf2352e80, 0xcb3ec0db, 0x80a4a4e6, 0xb7a77272, 0x9454ebf9, 0xc647c612, 0xfec0ebf7, 0x422aea94, 
    0xa92252e1, 0xcd39928c, 0xf2434f47, 0x9040ef91, 0x127279b1, 0x60846930, 0x594ee991, 0x85cab783, 0x62051c69, 0x36064a07, 0xa982198a, 0x5a077062, 
    0x8fe4d558, 0x66df58cc, 0x1a1c1b70, 0x3d8a5904, 0xbce0d23a, 0xc029e45b, 0xc5f1dfdd, 0xeee9e636, 0xaa36125c, 0x543baaf0, 0x61835414, 0x368e2056, 
    0x24d79985, 0xbd6a0e1c, 0x76bf8158, 0x10a0a147, 0x959c2040, 0x162a8e00, 0x5caf5c1b, 0x5cd1541c, 0x8885cfac, 0x8e6a07a6, 0x00210964, 0x6480daf1, 
    0x25535174, 0xa18cc4ad, 0x73373f62, 0x62b102c5, 0xb0cdc86b, 0x3d480286, 0x737afa58, 0x0ac95b50, 0x7a8b2612, 0x54e4fc9e, 0x90e07434, 0xc24db54f, 
    0xd286c7e5, 0x4c904c68, 0x7a6247a8, 0x4beb74fe, 0xdadd5e78, 0xb055dc45, 0x27514d0e, 0xfd6aa603, 0xaf84a983, 0x27279b4f, 0xd51a9ada, 0xa51d6f89, 
    0x1e2ab2cb, 0x534d5007, 0x41d9b571, 0x6a46d2ce, 0x5ceaadf5, 0x4c38b2cb, 0x78901c43, 0x7ab729fd, 0x31b1b064, 0x101c9015, 0x47337203, 0xa175b541, 
    0xfcf69c47, 0x2a7468e9, 0xf7e0ba4a, 0x4b919c06, 0x6e523e20, 0x41a4f023, 0x6970150d, 0x66f340fb, 0xc119b111, 0x537afd1f, 0x353f85a7, 0x7dfe01c2, 
    0x59645eeb, 0x4ec5814a, 0xd876e3f6, 0xf63da599, 0xb38602af, 0x08590699, 0x7329be03, 0x9a931021, 0x838c794a, 0x40e1c783, 0xfe9c9e8c, 0xb8207314, 
    0x11432aca, 0x1953bc5e, 0xc186d30e, 0x1ccd48ec, 0x55142ec8, 0xd20eb0cf, 0xfff63de7, 0xb2e9f500, 0x99f18859, 0x33d1e718, 0x90796afd, 0xb7d16915, 
    0xe61c4996, 0x718025ad, 0x541f2991, 0x554b32c5, 0x24ac7f8c, 0x8ab900ff, 0x9a08c87c, 0x1518c362, 0xa4e6d41f, 0x93338ef2, 0x4120a58f, 0xb994fedc, 
    0x5746ae90, 0xa8e0a42e, 0xfe8cbcea, 0xc4dd7855, 0x12b79491, 0x86aaa7c2, 0xe35935c8, 0x27d74650, 0x9b1423d7, 0x43e7e83d, 0x4c451be6, 0x3781c460, 
    0x538aef75, 0x5498dd6a, 0x416e22f3, 0x87294352, 0xcfc91938, 0xe36ee334, 0x90e599de, 0xa23e7270, 0x923b648e, 0xe3badbbd, 0x9e5f1568, 0x6aea93e4, 
    0xc9a88056, 0x1f424261, 0x947ef0f8, 0x67b92073, 0x7677454b, 0x3845be0b, 0x8a7a6f5f, 0x4b5a2de6, 0x134862ec, 0x0378eafb, 0x22bd23d7, 0xa4f781a1, 
    0x0e7a58c5, 0xb9be566a, 0xdcfc1831, 0x9a23758c, 0x4ab1d17c, 0x502ad64a, 0x142c6d43, 0x3ddb2990, 0xbddc280d, 0x2273d418, 0x90fb882e, 0x23760441, 
    0xd3b53515, 0x6e6ae75a, 0xda9373dc, 0x8ddeb399, 0x2073f49e, 0x4e5d66b9, 0x2a0a1b56, 0xab06df93, 0x67d13cc7, 0x9393876c, 0x7b36699e, 0xeac118d2, 
    0x41e7284e, 0xdd315acc, 0x9bb42a5e, 0x36c38142, 0x0cb98a17, 0x4cdee83f, 0xd7e5dc4f, 0xb7ac6a9e, 0x92074d31, 0xc6186560, 0xe947f055, 0x964adb51, 
    0xdc6844aa, 0xfefa25f5, 0x52e2f914, 0x2adf4292, 0x3907f9ac, 0x15e73c19, 0x5b61d927, 0x21df2171, 0x29d24ec6, 0x0329162d, 0xeb96cfb4, 0x2c6b54f3, 
    0x82b2a4b6, 0xc38d2763, 0x59e6058a, 0x8631264b, 0xf6235986, 0x86a4f06f, 0x88bc53c3, 0x2a4d14cb, 0x004bc621, 0x2d56533f, 0x98a732a5, 0xa01b02d2, 
    0x9a5b8103, 0x390bc255, 0x33030e22, 0x050e7b9e, 0xa2964952, 0x8d358399, 0x3694cadd, 0x094639c6, 0x5c3c55e4, 0x1a7344da, 0x1f187fe0, 0xe9b07435, 
    0xbb5cf157, 0x2425cedc, 0x3e1db80d, 0x0d7b8795, 0x332b0dc4, 0xa324ef9c, 0x8a93dc81, 0x364ab01a, 0x8018a7d4, 0x4c9264c6, 0x09b5e8b9, 0xe270bb63, 
    0xd2b624ad, 0x0edf1a2e, 0x3b6a1b78, 0x0680578b, 0x52e90f3d, 0x37112d43, 0x2f8f319c, 0x32ed24bd, 0xfa4c62ae, 0x8fcc05d3, 0xa884d828, 0x9dc22aea, 
    0xf6303fc7, 0x64801423, 0xe95efe2c, 0xa7afa7b0, 0xc2d27215, 0x9216fae2, 0x1bc804dd, 0x3d5e8604, 0x2d39d5ba, 0x99b9adc4, 0xa7108c97, 0x1ade74ad, 
    0xab5cba0a, 0x4f69c010, 0xc529ec41, 0x7715975d, 0xb7e82063, 0xb5494290, 0x0224073b, 0x9515f41b, 0x58fe8871, 0xc7f5e082, 0xd9db3527, 0x24595e5b, 
    0xae9d699f, 0x83ca2fb7, 0x956be6af, 0xfc2588b9, 0x9d6814d2, 0x999e929c, 0x58c9293d, 0x2a231b25, 0x55fefb30, 0xd28c7bcb, 0x7b874c10, 0xd29a7a72, 
    0x49e5607c, 0x7c565ccf, 0x1342a68a, 0x0c2aea27, 0xf99815da, 0x9fe2d8e3, 0xc1f86c2c, 0x79d4d0e3, 0x2527688f, 0x8a4843a9, 0x2a3d460e, 0x8b200b44, 
    0xf1fa0980, 0x35da165a, 0x656ea7f6, 0x14deb7b5, 0x30ea92e0, 0x4435f77a, 0xb82e384a, 0xbfaeb83e, 0xec8f73c2, 0xe44c85ab, 0x017f70ca, 0xc5ee2a4e, 
    0xceec2641, 0x557bf878, 0x25d66d44, 0xa9c3f3b6, 0xc6ac01fd, 0xcbb08d56, 0xb4ef6a8f, 0xcb4a4dcb, 0x96bb7bcd, 0x453e2cbb, 0xae89df5e, 0x5f9cf92e, 
    0x34f0345c, 0xc58c3472, 0x7a72f85b, 0x876c9453, 0x2a86a428, 0x0d80c3c6, 0x62675c32, 0xa8b0cfa8, 0xecce18c0, 0x0b39b563, 0x472e58a6, 0xd106927a, 
    0x85041545, 0xf5d0b05f, 0x9616521d, 0x45cddbce, 0x55efa46d, 0xbd4ffcc1, 0xc0f3ae50, 0xe41e9d80, 0x9ce78303, 0xaa22df7f, 0xc2c5ee2a, 0xe6cc4e2a, 
    0xb7b6f0a5, 0x066b170a, 0xd5b5ebc0, 0x640de48f, 0x18c14110, 0x493fbd22, 0xf8f4d4b3, 0xd3eb6ba7, 0x1005487a, 0x3ef71875, 0x893ae7b5, 0x2d57ea90, 
    0x6917462c, 0xf8678c59, 0x728a7372, 0x05e7908d, 0x14b57215, 0x30665051, 0x5a470e12, 0x58bc66b8, 0x53d8c63c, 0xfe53cfa0, 0x0d54a57f, 0xe16f0786, 
    0x0e19324f, 0xe8407264, 0x2ad1722b, 0xfe58967d, 0xa03f3166, 0xb5d2e73f, 0x0f2e4f34, 0x62eaca30, 0xf6d870dc, 0x8da689ac, 0xaa4262d3, 0x74fe7b3d, 
    0x21af6beb, 0x5a063276, 0xeca4d413, 0xecd98ece, 0x2fccecb4, 0x6f63bcc4, 0xaace1fdd, 0x77c353ea, 0x0b1bd770, 0xe70e8c81, 0xc578acf0, 0xb09019a1, 
    0x0a1cb027, 0x6de75244, 0x3ca39aaf, 0x5505b0e5, 0xb1ba72ca, 0xd1a4b03c, 0x539ec879, 0xc708cea8, 0xbe4751e7, 0x708e40f6, 0xde7132aa, 0xc34c30a4, 
    0x9e43976c, 0x43d23cd9, 0xcb9c6d6e, 0x47558124, 0x4952ef38, 0x39494159, 0x26ab483b, 0x0bc629f4, 0x4c955e0f, 0xc8481060, 0x19e0a91d, 0xa78fed5c, 
    0xb220a965, 0xbaef3c92, 0xc48edb36, 0xc9e14fd7, 0xa432ec17, 0xbc4d2cd2, 0x073de2fd, 0xe62cc935, 0xa7408421, 0x466aeeb9, 0x608c66bb, 0x3b4db361, 
    0xdc591917, 0xf8b4adea, 0xa6d3c2f4, 0x639c1ba7, 0xe963e004, 0xd4c39a93, 0xd94b162e, 0x548c8c2e, 0xe0bb618c, 0x418df57f, 0x822a8d1c, 0xe0399ed1, 
    0xae00a6d3, 0x941e6f0a, 0x2ba7d0db, 0x3cc506ad, 0x4ecafc72, 0x94da837b, 0x82e37c18, 0xd814f737, 0x46048271, 0x4aeba83f, 0x55977711, 0x5a7f8f1b, 
    0x95f89444, 0xeec055fe, 0x72186868, 0x4612690e, 0xd4008551, 0xe7b0e68b, 0x18082a0a, 0x9cc3df24, 0x475d610f, 0x4bb76685, 0xd1499219, 0xf6c62933, 
    0xe58af0c7, 0x278742a4, 0x28ad2707, 0x5197d31b, 0xb3d314df, 0xce9d5d1c, 0xecd34cd3, 0xa67b5674, 0x0b55d2d5, 0x3f80a08d, 0x35d79c2c, 0xa94beaab, 
    0x144f936a, 0x016cca67, 0x190327c7, 0x10ca11aa, 0xd88e3c6d, 0x9c243ba7, 0xca4d2b07, 0x576ec8ea, 0xe3984256, 0x7dc6ed7c, 0x330b468d, 0x9c712cec, 
    0xa9229154, 0x1469f2e1, 0x1537b40f, 0x45519124, 0x76850415, 0xb7b809fe, 0x49b9488b, 0xb48c23ae, 0x03dc01c7, 0x147145f7, 0xdcd94555, 0xdc95cba8, 
    0x336d34ef, 0x7b27d04f, 0xe5b835a9, 0x08da4805, 0x9c8cf851, 0xdfaf29d7, 0x336ba9c5, 0x8d0842dd, 0xc1480a88, 0xb3660c20, 0xeacaa1a8, 0x56576ec8, 
    0x8aa22848, 0xd9ff0f92, 
};
};
} // namespace BluePrint
