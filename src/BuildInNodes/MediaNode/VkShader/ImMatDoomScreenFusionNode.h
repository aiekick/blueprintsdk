#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <DoomScreen_vulkan.h>

namespace BluePrint
{
struct DoomScreenFusionNode final : Node
{
    BP_NODE_WITH_NAME(DoomScreenFusionNode, "DoomScreen Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Move")
    DoomScreenFusionNode(BP* blueprint): Node(blueprint) { m_Name = "DoomScreen Transform"; }

    ~DoomScreenFusionNode()
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
                m_fusion = new ImGui::DoomScreen_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_amplitude, m_noise, m_frequency, m_dripScale, m_bars);
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
        float _amplitude = m_amplitude;
        float _noise = m_noise;
        float _frequency = m_frequency;
        float _dripScale = m_dripScale;
        int _bars = m_bars;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Amplitude##DoomScreen", &_amplitude, 0.1, 10.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_amplitude##DoomScreen")) { _amplitude = 2.f; changed = true; }
        ImGui::SliderFloat("Noise##DoomScreen", &_noise, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_noise##DoomScreen")) { _noise = 0.1f; changed = true; }
        ImGui::SliderFloat("Frequency##DoomScreen", &_frequency, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_frequency##DoomScreen")) { _frequency = 0.5f; changed = true; }
        ImGui::SliderFloat("DripScale##DoomScreen", &_dripScale, 0.1, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_dripScale##DoomScreen")) { _dripScale = 0.5f; changed = true; }
        ImGui::SliderInt("Bars##DoomScreen", &_bars, 1, 100, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_bars##DoomScreen")) { _bars = 30; changed = true; }
        ImGui::PopItemWidth();
        if (_amplitude != m_amplitude) { m_amplitude = _amplitude; changed = true; }
        if (_noise != m_noise) { m_noise = _noise; changed = true; }
        if (_frequency != m_frequency) { m_frequency = _frequency; changed = true; }
        if (_dripScale != m_dripScale) { m_dripScale = _dripScale; changed = true; }
        if (_bars != m_bars) { m_bars = _bars; changed = true; }
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
        if (value.contains("amplitude"))
        {
            auto& val = value["amplitude"];
            if (val.is_number()) 
                m_amplitude = val.get<imgui_json::number>();
        }
        if (value.contains("noise"))
        {
            auto& val = value["noise"];
            if (val.is_number()) 
                m_noise = val.get<imgui_json::number>();
        }
        if (value.contains("frequency"))
        {
            auto& val = value["frequency"];
            if (val.is_number()) 
                m_frequency = val.get<imgui_json::number>();
        }
        if (value.contains("dripScale"))
        {
            auto& val = value["dripScale"];
            if (val.is_number()) 
                m_dripScale = val.get<imgui_json::number>();
        }
        if (value.contains("bars"))
        {
            auto& val = value["bars"];
            if (val.is_number()) 
                m_bars = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["amplitude"] = imgui_json::number(m_amplitude);
        value["noise"] = imgui_json::number(m_noise);
        value["frequency"] = imgui_json::number(m_frequency);
        value["dripScale"] = imgui_json::number(m_dripScale);
        value["bars"] = imgui_json::number(m_bars);
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
        // if show icon then we using u8"\ue077"
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
    float m_amplitude   {2.f};
    float m_noise       {0.1f};
    float m_frequency   {0.5f};
    float m_dripScale   {0.5f};
    int m_bars          {30};
    ImGui::DoomScreen_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4682;
    const unsigned int logo_data[4684/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xa3003f00, 0xdbf73fe6, 0x7fcca3f3, 0x4de7b7ef, 0x580ebda2, 
    0x1db233f6, 0xdbf73fe6, 0x7fcca3f3, 0x4de7b7ef, 0xffbd5aaa, 0x9b6ef600, 0x5ddac82d, 0x42371046, 0xc538a9dd, 0x8b2cd82b, 0x00ff63fe, 0x3c3abf7d, 
    0x7efbfec7, 0x7e68ce75, 0xbebfb922, 0x51ec2dfb, 0x47375dc4, 0xbfe74ab5, 0x0d5de973, 0x5d49a828, 0xf30ed921, 0xf9edfb1f, 0xf73fe6d1, 0xd1a6f3db, 
    0xc21ecb55, 0xaffd2cb2, 0xacd68ffd, 0xcd157556, 0xc35f243c, 0x7b9a0ea1, 0x2f55341a, 0xeffd4bb4, 0x89f651d0, 0x0afabd7f, 0xfba4fae7, 0xcb643fa3, 
    0x310f55b4, 0xdfbe00ff, 0x60ad369d, 0xb25756df, 0x89f92ff3, 0xeafcf5fd, 0xa327b53f, 0x582b4a55, 0x29b720e1, 0x158d4653, 0xc9355a9d, 0xb6b28f5a, 
    0xe194b9b7, 0x5d5114ca, 0x5df4846f, 0x67d35237, 0xcddbf69e, 0x3ba89474, 0x1e6060d8, 0x5d0956c4, 0x938b51d8, 0x2b2a39b2, 0xaff4d3b8, 0x2a89eb0b, 
    0x1bcb5bd9, 0xec928c47, 0xa44ecf08, 0xdcc4d58e, 0x4bdd30f9, 0x22924e12, 0x61755539, 0x15f50086, 0xdca08f5c, 0x156dd41a, 0xd193da0f, 0xff98b7aa, 
    0xce6fdf00, 0x721a86b6, 0x6daa41df, 0x9faae897, 0xab457f6a, 0x524a9d75, 0x65626ea7, 0x8ac21d17, 0x9ef0ad2b, 0x5aeaa68b, 0xdbdef36c, 0x9592ae79, 
    0x0c0c7b07, 0xcd8ad803, 0x6204bb2b, 0x4a8eece4, 0xfd34ee8a, 0xe2fac22b, 0xf256b64a, 0x24e3d1c6, 0xd33302bb, 0x71b523a9, 0x6b0b70b7, 0x6ec3edc9, 
    0xbbc94811, 0xa61907d7, 0x5a9460e3, 0x8aa22157, 0xe849ed87, 0x4f09e3d4, 0xbd4942e1, 0x4551948a, 0x5758677b, 0xe00be323, 0x125970d7, 0xe4615ade, 
    0xfa7fbe03, 0xeabacedf, 0x824487ce, 0x23f5592d, 0xf94cb22c, 0x9f81b1e1, 0xa8383d4e, 0x4356729a, 0xab7f2547, 0x6ba5beda, 0x24c5a779, 0x94150006, 
    0x4e073280, 0xee8ad387, 0x852795e1, 0x1de58c25, 0x0dea0343, 0xf0e9a841, 0xda8676ea, 0x0449c172, 0xf408ea15, 0x2dda58a7, 0xc9769c8d, 0x30c6ba23, 
    0xfc38e30b, 0x2d2e4629, 0x288a6281, 0x511404ad, 0x5e150045, 0x2c6cf1fe, 0xb871ba65, 0x9c0bb946, 0x6215f664, 0x5d189fb9, 0x7afab682, 0x6f2353b8, 
    0xe8009e7c, 0x547bfe33, 0xb8b15cce, 0x223ee812, 0x6bbd529f, 0x46e3a86b, 0x020ab929, 0x5d93f933, 0xfad7700d, 0xbd3a8c85, 0x2e9be69d, 0x75552556, 
    0xe30046d8, 0xdaaea8bf, 0x52785219, 0xd451ce58, 0xd4a03e30, 0x9bee9553, 0x14451f1b, 0x280aa256, 0xfb2680a2, 0x742dfa53, 0xbf8c0f3a, 0xb7e42cb1, 
    0xfcf3cffe, 0xcee737c9, 0xc7008edb, 0xcc953edd, 0x3b9a2653, 0x5b695e78, 0x98a5686c, 0xd8c70afa, 0x4ad85ad3, 0xf4a23b29, 0x0aa5b67a, 0xba508b95, 
    0x66a11e8c, 0xaad69f61, 0xf8a03d57, 0xf54a7d8a, 0x8da3aead, 0x28e4a618, 0x4de6cf08, 0x5aa43574, 0x4561e86a, 0x56005514, 0x5851678d, 0x6deda356, 
    0xe144acad, 0x77d768cc, 0x1a8d53e0, 0x9838e3e4, 0x0100fff1, 0x6b9fe715, 0xf56300ff, 0xc517ddad, 0x3cdba2af, 0x811cc510, 0x9c2f79df, 0x57fa03f4, 
    0x0e52a302, 0x38c119ed, 0x34ebecca, 0x621d7a1d, 0xd3236d59, 0x2494aa0d, 0xe6c74dbc, 0x13aec100, 0x4b4f9752, 0xc38d45f9, 0x514e6e4d, 0xa83f4899, 
    0xdb00ffaa, 0x05fdc7ba, 0x21f07faf, 0xa8c600ff, 0xa6c25a57, 0xe36ae6bd, 0xfb596675, 0xad1ffb5f, 0xde8aa256, 0x850fe314, 0x8b2d450d, 0x00ff643f, 
    0xeca3f46f, 0x95feed9f, 0x7ef38a62, 0xcfb957b3, 0xfb5724ed, 0xa57ffb27, 0x00ff641f, 0x15abf46f, 0xd443c37e, 0x5b5a4875, 0x15356f3b, 0x57bd93b6, 
    0xf43ef107, 0xeaab627d, 0x8cbd3935, 0xb77fb28f, 0x4ff651fa, 0x4a00fff6, 0xdac297df, 0xac5d28dc, 0xd7ae031b, 0x35903f56, 0x04074190, 0x221e8a60, 
    0x9483ddb2, 0xb27fe5d6, 0x51fab77f, 0xfff64ff6, 0x45b14a00, 0xdcab591f, 0x7f45d25e, 0xfab77fb2, 0xf64ff651, 0xb14a00ff, 0xab591f45, 0x8aa43ddc, 
    0xff6400ff, 0xacf46f00, 0x45089f5b, 0xed35ab7d, 0x199bd7dd, 0x2bec1018, 0x603e8e81, 0xfb55d1d5, 0xd5510f0d, 0xed6c6921, 0xda56d4bc, 0x1f5cf54e, 
    0x75d2fbc4, 0x8d4b4bea, 0x93636f4e, 0x63277cd4, 0x8961687b, 0x827cb621, 0x91018e24, 0xd8ae62f9, 0x67632be9, 0xee4eb21d, 0xe0028cb1, 0x74851f67, 
    0x5b5bf8f2, 0x83b50b85, 0xeada7560, 0xb206f2c7, 0x8ce02008, 0xaab64711, 0x730eee9d, 0xc9fe955b, 0x47e9dffe, 0xfddb3fd9, 0xfe14c52a, 0xbdb857b3, 
    0x00ff8aa4, 0x6f00ff64, 0x9feca3f4, 0x6295feed, 0x57b33e8a, 0x15497bb8, 0xdffec9fe, 0x4df554e9, 0x171bf519, 0x27af4bb5, 0xcb9b1b79, 0x51a7c7dd, 
    0xc37e955a, 0x4875d443, 0x6f3b5b5a, 0x93b61535, 0xf10757bd, 0xbdbdf43e, 0xa7c6a557, 0xc9c7b137, 0xd4fe45e1, 0xbbb6bdb7, 0x55853cf2, 0x3064e5d9, 
    0x71fa7a1c, 0xff641f5b, 0xa59fa600, 0x5bf81274, 0xb50b855b, 0xda756083, 0x06f2c7ea, 0xe02008b2, 0xb647118c, 0xe7a01eab, 0xec5fb935, 0x94feed9f, 
    0xbffd937d, 0x4f51acd2, 0x8b7b35eb, 0xf6af48da, 0x00fff64f, 0xfec93e4a, 0x2856e9df, 0x7b35eba3, 0x5f91b487, 0xfeed9fec, 0xfd937d94, 0x55acd2bf, 
    0x510f0dfb, 0x6c6921d5, 0x56d4bced, 0x5cf54eda, 0xd1fbc41f, 0xa8af8af5, 0x30f6e6d4, 0x52b9b2a0, 0x8ab968df, 0x122e4f4f, 0xd4d8fc98, 0x00ff64df, 
    0xf9adf46f, 0xc2ad2d7c, 0xb0c1da85, 0x6375ed3a, 0x045903f9, 0x08467010, 0x5715eba3, 0xdc9a7250, 0xf64ff6af, 0x3e4a00ff, 0xe9dffec9, 0xeba32856, 
    0xda8b7b35, 0xa7f6a948, 0x59b75af4, 0x625aa3d5, 0x955b18a1, 0x6b255115, 0x049e7705, 0x20f7e804, 0xc739131c, 0x832bf2fd, 0x37c1dfae, 0x1769f116, 
    0x71c43529, 0x3be09896, 0xaee87e80, 0xa9107f7a, 0x93c6467c, 0xf1e9a967, 0xa6d7d74e, 0x200a90f4, 0x7dee31ea, 0x1375ce6b, 0x5aaed421, 0xd22e8c58, 
    0xf0cf18b3, 0xd915e7e4, 0x9f66dae8, 0x52f74ea0, 0x0aca716b, 0xa310b491, 0xae3919f1, 0x8bbf5f53, 0xba67d652, 0x101b1184, 0x40829114, 0xec39cd18, 
    0xa3f0a98a, 0x44fba33a, 0x0dfdde5f, 0xfb8b681f, 0x55aaa1df, 0x3d3f1cb5, 0x8489eef9, 0xcc7fb92f, 0xe7afef4f, 0xd1eaac4e, 0x5245afa8, 0x9ce29eb5, 
    0xcfbb4279, 0x7b740202, 0x9c090e90, 0x15f9fee3, 0xe06fd7c1, 0xb4788b9b, 0xe29a948b, 0x704ccb38, 0x743fc01d, 0x21fe7456, 0x8d8df8d2, 0xd353cf26, 
    0xafaf9de2, 0x1420e94d, 0xdc63d441, 0xea9cd7fa, 0x5ca94326, 0x5d18b1b4, 0x9f3166a5, 0x2bcec9e1, 0xcdb4d1b3, 0xee9d403f, 0x94e3d6a4, 0x21682315, 
    0x7332e247, 0x7fbfa65c, 0xcfaca516, 0x36220875, 0x04232920, 0x739a3180, 0xe15315d9, 0x55147546, 0xfd4bb42f, 0xa553d0ef, 0x632f5546, 0x62cbc538, 
    0xed4b15ed, 0x7b00ff12, 0x8a7615f4, 0xd74e65b4, 0x70c74509, 0x80c0f3ae, 0x03e41e9d, 0xff386782, 0x7045be00, 0x26f8db75, 0x222ddee2, 0x8eb826e5, 
    0x071cd332, 0x15dd0f70, 0xa922fe34, 0x93c6467c, 0xf1e9a967, 0xa6d7d74e, 0x200a90f4, 0x7dee31ea, 0x1375ce6b, 0x5aaed421, 0xd22e8c58, 0xf0cf18b3, 
    0xd915e7e4, 0x9f66dae8, 0x52f74ea0, 0x0aca716b, 0xa310b491, 0xae3919f1, 0x8bbf5f53, 0xba67d652, 0x101b1184, 0x40829114, 0xec39cd18, 0xa3f0a98a, 
    0x3f2a8a3a, 0xeffd45b4, 0x3246d4d0, 0x26c9c896, 0xa3a224f6, 0xde5f44fb, 0x44490dfd, 0x81ee28a3, 0xef0ab7a6, 0xd109083c, 0x263840ee, 0xe4fb8f73, 
    0xbf5d0757, 0xe22d6e82, 0x6b522ed2, 0x312de388, 0xfd0077c0, 0xe24f55d1, 0x6cc4972e, 0x9e7a3669, 0x7ded149f, 0x00496f7a, 0x1ea30ea2, 0xe7bcd6e7, 
    0x4a1d3251, 0xc288a5e5, 0x8c312bed, 0x4e0e00ff, 0x8d9e5d71, 0x04fa69a6, 0xb72675ef, 0x1ba9a01c, 0x113f0a41, 0x35e59a93, 0x2db5f8fb, 0x41a87b66, 
    0x4901b111, 0x8c012418, 0xaac89ed3, 0xa8330a9f, 0xc4c88aa2, 0x6b3fabce, 0xf56300ff, 0x5c5714ab, 0xc59f11f1, 0xcdd1c41f, 0x4551d03d, 0x4766c815, 
    0xbdbf88f6, 0xe6a91afa, 0xf3dbf73f, 0x4557f4ab, 0xed85b03a, 0x01fec71f, 0x0c5d9271, 0x8a462bea, 0xdd9febdf, 0xbef64bfc, 0xd1687546, 0xb75a6145, 
    0xb196b6b5, 0x51309f13, 0x40ce1545, 0x01144551, 0xd193da0f, 0xd168a5aa, 0xc6eb545d, 0xf863f99d, 0x3b6a469a, 0x68b43a23, 0x6fb5a6a2, 0x612d6d6b, 
    0x45c17c4e, 0x01395714, 0x01501445, 0x45a3d559, 0xecad5174, 0x5ceea5af, 0x5667ca27, 0x5a51148d, 0x96b6b5b7, 0xc17c4eb0, 0x39571445, 0x50144501, 
    0xd1be5401, 0x41bff72f, 0xa9ad6857, 0x782f8c54, 0x2dc9a8dc, 0x154551d0, 0x6f5d2189, 0x375df484, 0x9e67d352, 0x74cddbf6, 0xd83ba894, 0xc41e6060, 
    0xdf5d2557, 0x6b344e81, 0x63e28c93, 0x5504fcc7, 0x539a5dc1, 0x9a51cb49, 0x7de1957e, 0x2b5b2571, 0xf1686379, 0x19815d92, 0xda91d4e9, 0x05b8dbb8, 
    0xe1f6e4b5, 0x64a408b7, 0x8c83ebdd, 0xeba077d7, 0xcb12ebd0, 0x6d981e69, 0xe225a154, 0x06303f6e, 0xf5fa710d, 0xb16e7a84, 0x594eb43d, 0x1c378214, 
    0x6a9e9191, 0x8aae95a6, 0xa3a95c9a, 0xac288a3a, 0x288a428c, 0xdfba02a0, 0x6ebae809, 0x3dcfa6a5, 0xe99ab7ed, 0xb0775029, 0x883dc0c0, 0xbfbb4aae, 
    0xd7689c02, 0xc7c41927, 0xab08f88f, 0xa634bb82, 0x34a39693, 0xfac22bfd, 0x56b64ae2, 0xe3d1c6f2, 0x3302bb24, 0xb523a9d3, 0x0b70b771, 0xc3edc96b, 
    0xc948116e, 0x1907d7bb, 0xd741efae, 0x9625d6a1, 0xda303dd2, 0xc44b42a9, 0x0c607edc, 0xebf5e31a, 0x62ddf408, 0xb29c687b, 0x396e0429, 0xd53c2323, 
    0x155d2b4d, 0x4653b934, 0x59511475, 0x51148518, 0xbe750540, 0xdd74d113, 0x7b9e4d4b, 0xd2356fdb, 0x61efa052, 0x117b8081, 0x7e77955c, 0xaed13805, 
    0x8f89334e, 0x5711f01f, 0x4d697605, 0x69462d27, 0xf58557fa, 0xad6c95c4, 0xc6a38de5, 0x67047649, 0x6a4752a7, 0x16e06ee3, 0x86db93d7, 0x939122dc, 
    0x330eae77, 0xaf83de5d, 0x2d4bac43, 0xb5617aa4, 0x89978452, 0x18c0fcb8, 0xd6ebc735, 0xc4bae911, 0x6439d1f6, 0x72dc0852, 0xaa794646, 0x2aba569a, 
    0x8ca67269, 0xb2a228ea, 0xa2280a31, 0x7ceb0a80, 0xbae9a227, 0xf73c9b96, 0xa46bdeb6, 0xc3de41a5, 0x22f60003, 0xfcee2ab9, 0x5ca3710a, 0x1e13679c, 
    0xae22e03f, 0x9ad2ec0a, 0xd38c5a4e, 0xeb0baff4, 0x5bd92a89, 0x8c471bcb, 0xcf08ec92, 0xd58ea44e, 0x2dc0ddc6, 0x0db727af, 0x262345b8, 0x661c5cef, 
    0x5e07bdbb, 0x5b965887, 0x6ac3f448, 0x132f09a5, 0x3080f971, 0xacd78f6b, 0x8975d323, 0xc872a2ed, 0xe4b811a4, 0x55f38c8c, 0x5474ad34, 0x194de5d2, 
    0x644551d4, 0x88f64762, 0x1afabdbf, 0x93fdab92, 0xacd2bffd, 0x3555d556, 0xa472476e, 0x6b2bd0a3, 0xc62df145, 0x43f06c8b, 0x7d077204, 0xe839b9e4, 
    0x5aacf407, 0xd8da242b, 0x3aaab549, 0xa8793c89, 0xd0961032, 0xc98d1e23, 0xdc5cebc7, 0x3b73cbf3, 0x919733cf, 0x738f59ce, 0x9343d151, 0xbdc98d7b, 
    0xb8298ac2, 0xfd2fef7f, 0xd700fff3, 0x3a922ba1, 0x7fc8878a, 0x35d5eef9, 0xb315a354, 0x025ddab8, 0x125fb4b6, 0xcfb668dc, 0x20473004, 0x934bde77, 
    0x4a7f809e, 0x36a5a2c5, 0xd5da04b6, 0x3c9e441d, 0x4b0819d4, 0x468f1168, 0xaef5e3e4, 0xb9e5796e, 0xcb99e79d, 0xc72ce7c8, 0xa1e8a8b9, 0xe4c6bdc9, 
    0x1445e1de, 0xa2282452, 0x6b2b008a, 0xc62df145, 0x43f06c8b, 0x7d077204, 0xe839b9e4, 0x5aacf407, 0xc3d6a629, 0xd451ad4d, 0x41cde349, 0x81b68490, 
    0x4e6ef418, 0xe7e65a3f, 0xde995b9e, 0x8ebc9c79, 0x9a7bcc72, 0x9b1c8a8e, 0xee4d6edc, 0x22455114, 0xa0288a42, 0x5fb4b602, 0xb668dc12, 0x473004cf, 
    0x4bde7720, 0x7f809e93, 0x9aa2c54a, 0xda346c6d, 0x9e441dd5, 0x0819d43c, 0x8f11684b, 0xf5e3e446, 0xe5796eae, 0x99e79db9, 0x2ce7c8cb, 0xe8a8b9c7, 
    0xc6bdc9a1, 0x45e1dee4, 0x28245214, 0x2a008aa2, 0xa88786fd, 0xb6b490ea, 0x2b6ade76, 0xae7a276d, 0xea7de20f, 0x049e7785, 0x20f7e804, 0xc739131c, 
    0xa22af2fd, 0x225cecae, 0x6aceeca4, 0x706b0b5f, 0x6cb076a1, 0x585dbb0e, 0x41d640fe, 0x82111c04, 0x9bf4d32b, 0x8a4f4f3d, 0x37bdbe76, 0x075180a4, 
    0xeb738f51, 0x99a8735e, 0xd272a50e, 0x957661c4, 0x877fc698, 0x28a73827, 0x51700ed9, 0x45512b57, 0x45610615, 0x57015014, 0x473d34ec, 0xb3a58554, 
    0x5b51f3b6, 0x70d53b69, 0x54ef137f, 0x20f0bc2b, 0x00b94727, 0x3fce99e0, 0x155591ef, 0x15e16277, 0x52736627, 0x855b5bf8, 0x6083b50b, 0xc7eada75, 
    0x08b206f2, 0x118ce020, 0xd9a49f5e, 0x537c7aea, 0xbde9f5b5, 0x3a880224, 0x5a9f7b8c, 0xc8449df3, 0x96962b75, 0xacb40b23, 0x39fc33c6, 0x4639c539, 
    0x8a8273c8, 0x288a5ab9, 0x280a33a8, 0xbf0a80a2, 0x3aeaa161, 0x9d2d2da4, 0xdb8a9ab7, 0x83abde49, 0xa17a9ff8, 0x0181e75d, 0x07c83d3a, 0xff71ce04, 
    0xa88a7c00, 0x0817bbab, 0x9a333ba9, 0xdcdac297, 0x1bac5d28, 0x56d7ae03, 0x9035903f, 0x60040741, 0x26fdf48a, 0xe2d353cf, 0x4dafaf9d, 0x411420e9, 
    0xfadc63d4, 0x26ea9cd7, 0xb45ca943, 0xa55d18b1, 0xe19f3166, 0xca29cec9, 0x149c4336, 0x51d4ca55, 0x51984145, 0x55001445, 0x510f0dfb, 0x6c6921d5, 
    0x56d4bced, 0x5cf54eda, 0xd5fbc41f, 0x083cef0a, 0x40eed109, 0x8f732638, 0x4555e4fb, 0x45b8d85d, 0xd49cd949, 0xe1d616be, 0xd860ed42, 0xb1ba761d, 
    0x82ac81fc, 0x04233808, 0x36e9a757, 0x149f9e7a, 0x6f7a7ded, 0x0ea20049, 0xd6e71ea3, 0x3251e7bc, 0xa5e54a1d, 0x2bedc288, 0x00ff8c31, 0x4e714e0e, 
    0xe01cb251, 0xa256aea2, 0xc20c2a8a, 0x02a0288a, 0xdc047fbb, 0x5ca4c55b, 0xc611d7a4, 0xee80635a, 0xb8a2fb01, 0xeca22a8a, 0xca6554ee, 0x369a77ee, 
    0x13e8a799, 0xdc9ad4bd, 0x6da48272, 0x46fc2804, 0xd7946b4e, 0xb5d4e2ef, 0x04a1ee99, 0x2405c446, 0x33069060, 0xe550d459, 0x2b376475, 0x511424ab, 
    0x45014945, 0x5d015014, 0x2d6e82bf, 0x522ed2e2, 0x2de3886b, 0x0077c031, 0x455cd1fd, 0x77765115, 0x77e5322a, 0x4c1bcd3b, 0xde09f4d3, 0x396e4dea, 
    0x82365241, 0x27237e14, 0xf76bca35, 0xcc5a6af1, 0x238250f7, 0x30920262, 0xac190348, 0xba7228ea, 0xd5951bb2, 0xa2280a92, 0x8aa280a4, 0xdfae0028, 
    0xf11637c1, 0x35291769, 0x989671c4, 0x7e803be0, 0x8a22aee8, 0x953bbba8, 0x9dbb7219, 0x69a68de6, 0x75ef04fa, 0xa01cb726, 0x0a411ba9, 0x9a93113f, 
    0xf8fb35e5, 0x7b662db5, 0xb11141a8, 0x24184901, 0x75d68c01, 0x595d3914, 0xc9eaca0d, 0x52511405, 0x14455140, 0xe06f5700, 0xb4788b9b, 0xe29a948b, 
    0x704ccb38, 0x743fc01d, 0x54451157, 0x8cca9d5d, 0xf3ce5db9, 0xfd34d346, 0x93ba7702, 0x54508e5b, 0x1f85a08d, 0x72cdc988, 0x5afcfd9a, 0xd43db396, 
    0x80d88820, 0x00128ca4, 0x8a3a6bc6, 0x86acae1c, 0x826475e5, 0x20a9288a, 0x0000d9ff, 
};
};
} // namespace BluePrint
