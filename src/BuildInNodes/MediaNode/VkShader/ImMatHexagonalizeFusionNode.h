#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Hexagonalize_vulkan.h>

namespace BluePrint
{
struct HexagonalizeFusionNode final : Node
{
    BP_NODE_WITH_NAME(HexagonalizeFusionNode, "Hexagonalize Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    HexagonalizeFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Hexagonalize Transform"; }

    ~HexagonalizeFusionNode()
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
                m_fusion = new ImGui::Hexagonalize_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_horizontalHexagons, m_steps);
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
        float _horizontalHexagons = m_horizontalHexagons;
        int _steps = m_steps;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("X Hexagons##Hexagonalize", &_horizontalHexagons, 0.0, 50.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_horizontalHexagons##Hexagonalize")) { _horizontalHexagons = 20.f; changed = true; }
        ImGui::SliderInt("Steps##Hexagonalize", &_steps, 1, 100, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_steps##Hexagonalize")) { _steps = 50; changed = true; }
        ImGui::PopItemWidth();
        if (_horizontalHexagons != m_horizontalHexagons) { m_horizontalHexagons = _horizontalHexagons; changed = true; }
        if (_steps != m_steps) { m_steps = _steps; changed = true; }
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
        if (value.contains("horizontalHexagons"))
        {
            auto& val = value["horizontalHexagons"];
            if (val.is_number()) 
                m_horizontalHexagons = val.get<imgui_json::number>();
        }
        if (value.contains("steps"))
        {
            auto& val = value["steps"];
            if (val.is_number()) 
                m_steps = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["horizontalHexagons"] = imgui_json::number(m_horizontalHexagons);
        value["steps"] = imgui_json::number(m_steps);
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
        // if show icon then we using u8"\uf20e"
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
    float m_horizontalHexagons  {20.f};
    int m_steps         {50};
    ImGui::Hexagonalize_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3951;
    const unsigned int logo_data[3952/4] =
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
    0xe6e5e4e3, 0xeae9e8e7, 0xf5f4f3f2, 0xf9f8f7f6, 0x00dafffa, 0x0001030c, 0x11031102, 0xcf003f00, 0xd22b8aa2, 0xa9a22820, 0x00fff76a, 0xb76cbad9, 
    0x18756923, 0x760bdd40, 0xc095dda4, 0xda7345bb, 0x6fae881f, 0x7bcbbeef, 0x4d177114, 0xb952edd1, 0x57fadcef, 0x52324a43, 0x51144057, 0x450b5045, 
    0x51805414, 0xafd65145, 0x794cc396, 0xcd1c5116, 0xfea47ec4, 0x52fafc47, 0x5ee0ca6e, 0x39fcb0a2, 0x4bdfdca9, 0xcc8e372c, 0x53a643fb, 0xfd801f03, 
    0x23caad49, 0x01742525, 0x01531445, 0x928aa268, 0xbe2a8a42, 0x7e9a78a5, 0x0624f79a, 0x7340ed7c, 0x4fce7f82, 0x2bbb49e1, 0x6545b18c, 0x6aab1ae8, 
    0x95522c6d, 0x5c6e14f3, 0xfd333202, 0xd53a3f32, 0xb5070723, 0x82aea609, 0x144551c2, 0x288a3ac4, 0x8aa200a9, 0xb647fe86, 0x35e2dad3, 0x8693eb0c, 
    0x4d1d6d3c, 0xa226400f, 0x505d74b3, 0x0b4db7ea, 0xd5a95005, 0x7ebbe541, 0x5ae5cf18, 0x1e241544, 0xab6992a2, 0x4551b0a0, 0x8a8ec014, 0x0a8aad28, 
    0x7c617ce4, 0x220bee1a, 0x3c4ccb5b, 0xffcf7780, 0xd7f95b00, 0xe8d0595d, 0x3eab4590, 0x499665a4, 0x30363c9f, 0xa7c7e933, 0x4a4e1315, 0xafe468c8, 
    0xd4577bf5, 0xf8346fad, 0x02c080a4, 0x400690b2, 0x71fad0e9, 0xa432dc5d, 0x9cb1a4f0, 0x7d60a8a3, 0x1d35a841, 0xd04e1d3e, 0x29584edb, 0x41bd8220, 
    0x1beb941e, 0x8eb3b145, 0x587724d9, 0x677c01c6, 0xc528851f, 0x512cb0c5, 0x85a01545, 0x402a8aa2, 0x05f1982b, 0x906a52e4, 0xc8e3ca5b, 0x9c938688, 
    0x478efb8c, 0xaaa72be6, 0x3064fa76, 0x165f126a, 0x857e6669, 0x2f7b76f0, 0x4d541ca7, 0x1c81ac36, 0x66a3c6dd, 0x9d7bbb35, 0x01a0c23d, 0x002a285c, 
    0x5e18631c, 0xaef4a73e, 0x548020c3, 0x8e200f82, 0xd2514bf5, 0x3e25f5ac, 0x12924a6c, 0x1f069509, 0xd5f8eff3, 0x21f22298, 0x268ff911, 0x49ce893f, 
    0xeea629a2, 0x288a920c, 0xa26801ab, 0xb942928a, 0x16ea7eed, 0xbf3982bd, 0xb23c1c79, 0x9207e729, 0x6ae81a3f, 0xb4451a8c, 0x4cdd9277, 0x9c4bc30d, 
    0x8c32a0ac, 0xb11dc39e, 0xda345181, 0x5b311ab2, 0xd7b8b656, 0x0db5be20, 0x9b791c13, 0x79727afa, 0x98ea8a03, 0xed74dc00, 0x9b346f54, 0x6944a81b, 
    0x0db91d6f, 0xa9a2d09d, 0xabf774fa, 0x6118f116, 0x218fcc58, 0x9273e25f, 0xa688a67e, 0x25d9b89b, 0x48565114, 0xa4a228ea, 0x5b35b202, 0xac5896a5, 
    0xb89491c0, 0x58c68832, 0xff1ec0f3, 0xd76a5f00, 0x4f3ffdac, 0x75d30b96, 0x63b9307b, 0xc16d20c8, 0x7b3c6d04, 0x5d00ff0f, 0x217a934c, 0x2dd5a9a2, 
    0x9f5b57d7, 0x0a465cb3, 0xf6682891, 0xf88104b7, 0x63f871e5, 0x9e486eb5, 0x95be8c5c, 0xd408ad06, 0x915f44a0, 0xf3b8b3d5, 0x7dfe71c7, 0x5096e93d, 
    0x14676fcb, 0x91244853, 0xc0800517, 0xa52300ff, 0x96c13409, 0xa18aa228, 0x46efd90d, 0x147d7acf, 0x19bb3073, 0x9e8ddeb3, 0x6c9dfaf4, 0x325895dd, 
    0xccd18ce0, 0xb3abecc2, 0xf49e8dde, 0xdbf64db6, 0xd0069d67, 0xa71e8c84, 0x2300ff1c, 0x869da34e, 0xd17b36a3, 0x459fdeb3, 0xc62ecc1c, 0x67a3f7ec, 
    0x9e8a3ebd, 0x9bb12b66, 0xefd9e83d, 0x668ea24f, 0x7b366317, 0x9fdeb3d1, 0xacd8f310, 0x2403395f, 0xcc51342f, 0xcf66ecc2, 0xd37b367a, 0x8599a3e8, 
    0xe83d9bd8, 0xa24eefd9, 0xec8a99a7, 0x367acf6e, 0xa3a8d37b, 0xddd88599, 0xf76cf49e, 0x334751a7, 0x3dbbb10b, 0x4eefd9e8, 0x17668ea2, 0xa3f76c62, 
    0x8a3abd67, 0xb12b669e, 0xd9e83dbb, 0x8ea24eef, 0xbbb13b66, 0xefd9e83d, 0xe9346d57, 0x4677236f, 0x37700a55, 0xd1593577, 0x184676a3, 0xde233865, 
    0xe80e668e, 0x1bbd678f, 0x51d4e93d, 0x3276c5cc, 0x0aad288a, 0x9757870a, 0x612d986e, 0xea679994, 0xdf0bc9ab, 0xcdefe5a7, 0x51ac2349, 0xa8708eb4, 
    0x05e8130b, 0xde2dda64, 0xb6b931ad, 0x14e5d0c6, 0xd75101be, 0xa9f507f4, 0x4754e893, 0x9bb49be9, 0x912990e4, 0x6e31765d, 0xdbd79331, 0x7752ada0, 
    0x1b00c01d, 0x8f958e91, 0x9f8e1a2c, 0xe1a94b1a, 0x92f6f3a3, 0xe1876584, 0x43c701ea, 0x96b5daeb, 0xc000a448, 0xde061e73, 0x4aef31bd, 0x3aec813d, 
    0x24ab288a, 0xa4a2285a, 0x54b94f41, 0x0dcfc782, 0x9e00e396, 0x00fff409, 0xadca943e, 0xd46f7f1c, 0xa27cb663, 0xa78ee4e4, 0x4a00ff8c, 0xc81d954d, 
    0xac4dbdf4, 0x471871e4, 0x275b2157, 0x00ff19a1, 0x5decbc1a, 0x2456cfd9, 0xe173559c, 0x6bdbb478, 0x98edd6a9, 0x9055b901, 0x099e010c, 0x3fa727cf, 
    0x7fc636a5, 0xbb1dd83e, 0x7abdfc76, 0xba510ae0, 0x9a458ed1, 0x81a8a228, 0x918aa268, 0xdbd96805, 0x4685c2b6, 0x59e9e749, 0xa72f61d5, 0x1c56818d, 
    0x05ade375, 0x443b7347, 0x35d1b8b4, 0xf9515b99, 0x6038e355, 0xf01c83db, 0x5e8fbdde, 0x72f7ab79, 0x3c52b724, 0x8e27e563, 0xb17a9f31, 0x451224ae, 
    0xa4e1e81c, 0xb395dfce, 0xce9fe795, 0x8092d0b3, 0x5b495ab7, 0x1db61cdd, 0x20531445, 0xa4a2285a, 0x944c4b41, 0xfdc28931, 0x5a1fa7e2, 0xf8de9ac8, 
    0x9804e1c2, 0x7503dcc9, 0xc9d4faf5, 0x74661795, 0x968c7ab6, 0x1daae868, 0x630f905b, 0xda5d55eb, 0x96636747, 0xea7d9263, 0xf11c978e, 0x709f6dda, 
    0xc750be3b, 0x7d0c5c38, 0x9c72953e, 0x7752b15b, 0x8aa228d4, 0xa9286366, 0x1de7aa28, 0x5523e9c8, 0xa2440285, 0x46151d35, 0xa368a900, 0x27472e9c, 
    0xcb31a3ef, 0x2a39a1f3, 0x90ca8fdc, 0xb50e3a00, 0x8573142d, 0x28a9e8c8, 0x322e9ca3, 0x9e8a928a, 0x938e5c71, 0x35c73903, 0x8573142d, 0x8c71f2c8, 
    0x00527bf0, 0x00000014, 0xa2a50274, 0x1db9708e, 0x73142515, 0x5251c685, 0x17ce5351, 0xa2a4a223, 0x1db9708e, 0x73142515, 0xa9e8c885, 0x2e9ca328, 
    0x8a968a32, 0x8a5c719e, 0x398a968a, 0x5454e4c2, 0x17ce51b4, 0xa2a5a222, 0x1db9708e, 0x30545114, 0xb65c60a9, 0x3a7fe048, 0x16229885, 0x43cd003d, 
    0x3014b3fd, 0x2039d19d, 0x9f63a09d, 0xec26a5af, 0x4e7d6d54, 0xd08e4a9b, 0x639131c3, 0xe36d792e, 0xd91e7ff8, 0xbffc49ac, 0xfd2b4f3e, 0xe9b3e35e, 
    0x6e2d299e, 0x2eb55097, 0xa49d538a, 0x340d5e1f, 0x293d128c, 0x1273e8de, 0x81a0288a, 0x918aa268, 0xd3b25e05, 0x9007f61a, 0xc069134a, 0x1f276704, 
    0x9fae51a5, 0x10cd924e, 0x64449908, 0xbfafd7f1, 0x9a5e00ff, 0xec2a2a57, 0xf86f74e6, 0xb2bd5e9c, 0x172ae8fe, 0x20e379dc, 0x4d45fc74, 0x2e6d6c79, 
    0xc560069e, 0x8cdf510f, 0x74bad4d6, 0xb9d9551a, 0xe64380b2, 0x232f338c, 0x7a30ce39, 0x7b9c6e0f, 0x2cef1456, 0x49cebb93, 0x9a989f90, 0xe5186d2d, 
    0x283a221b, 0xd10291a2, 0x0a221545, 0xd4b4d3d1, 0x6e498aba, 0x5ce3c83c, 0x3dc0716d, 0xd51ec973, 0xb7d65b9d, 0x8669a86f, 0xb4dba2cd, 0xee1c072a, 
    0x013f4ec7, 0x9b04174d, 0xb145d316, 0x3bcbb390, 0xcb13c208, 0xe73a9e01, 0x4c62addb, 0x46f2148b, 0x625524ae, 0x018c8e03, 0x6b3457eb, 0xb9606f99, 
    0x17139eb6, 0xa01e7692, 0x9e83e78c, 0xb8a806e3, 0xc1aaec0a, 0x1d062480, 0x68bae8fd, 0xa1c94a73, 0x234551b4, 0x4551d431, 0x8bba8248, 0x4457ad09, 
    0xac648f17, 0x698fa1d0, 0x48ec9641, 0x3f27873e, 0xebe55a8f, 0xbcb9b5a2, 0x8a04246d, 0x11052a33, 0x0397f7af, 0xaad6cf93, 0x25f79426, 0x383548b2, 
    0xd26d89e4, 0xde582413, 0x603a1254, 0xfac8187e, 0x792a3557, 0x957bbc72, 0x941bb1f6, 0x07771ce4, 0x59f4b5d2, 0x688e509f, 0x2943fddb, 0x8631ca52, 
    0x3ff6993e, 0xadafb3ca, 0xc1d3a585, 0x9c7bcacf, 0x28fd769c, 0xa99ea07a, 0x5114bd32, 0x45476652, 0x1bbd673b, 0x644eeb3d, 0x01c6d055, 0x11e4a994, 
    0x4fdb5583, 0xca2a75b7, 0xf41c7a4e, 0xe83d7bab, 0x3447efd9, 0xa876e442, 0x707971b6, 0xe7e4298c, 0x795a9f9c, 0xe93d3939, 0xb3d17b76, 0x87688ede, 
    0xdb29da30, 0xefd9e83d, 0xa10b3247, 0xf7eca728, 0x4fbd67a3, 0xcad01532, 0xf6786fb1, 0x525594f1, 0xb53ecf09, 0x367acf16, 0x86ccd17b, 0xe34263a5, 
    0x11e59e5d, 0x07c0281e, 0xf3e41927, 0xf25971f9, 0xbb906548, 0x46e91360, 0x7b367acf, 0x3998ced1, 0xfd14655c, 0xf76cf49e, 0xe80a99a3, 0x9efd146d, 
    0xa9f76cf4, 0x19ba42e6, 0xd77ab65a, 0xc12c16d9, 0xdec7a0bc, 0x151823c0, 0x8ddeb39b, 0xa173f49e, 0x6a635ba9, 0x6a9fc4ef, 0xb6059eb1, 0x4ca9f2d8, 
    0x2000dc97, 0x6b0e7a82, 0xb43c899f, 0x53ef39c1, 0x67a3f76c, 0xf73437bd, 0x32ee2b07, 0x7acf7e8a, 0xe6d27b36, 0x8a12ba42, 0x367acf76, 0x21f3d47b, 
    0x95200c5d, 0x701c1c20, 0x455b2b7d, 0xb2b0a2d5, 0x4912eff2, 0x18c10fe5, 0xc9384ec0, 0x59417ffc, 0xd9e83d9b, 0x224d4def, 0xadb1ad94, 0xc7a7a936, 
    0xadd3636a, 0x9867b5a4, 0x633cd8c8, 0x0e3a9edb, 0xe5f90307, 0x33b73c4f, 0xe636d3bc, 0x851fd763, 0x367acf26, 0x98e6d07b, 0x146ddc39, 0x6cf49eed, 
    0x89cca5f7, 0x288a12ba, 0x8ea402a0, 0x345479da, 0x5bea4870, 0x82842a68, 0x717d9c71, 0x565d47cd, 0x7ef8b885, 0x724c8944, 0xc656b03b, 0xb0e1d872, 
    0xda633a7d, 0x312a579a, 0xa63976e6, 0xee5cb8b5, 0x386d94b7, 0x19c74339, 0x2afd5cfe, 0x3834ed2a, 0x675a0875, 0x32a49dbe, 0x0532b011, 0xc7830120, 
    0x7bb6a76f, 0x02381dd7, 0x302240e2, 0x64f7e78d, 0x9c9e5392, 0xc86a53fa, 0x51868d72, 0x0b241545, 0x80541445, 0x366d9b55, 0x9626f2f6, 0x1149dade, 
    0xef280b0e, 0xfe907f82, 0x57aba25e, 0xcdbde161, 0xb723f9e1, 0xd8590994, 0x433e086e, 0xeefd338e, 0x55edf4c7, 0x23547615, 0x3273eccc, 0x45d077e9, 
    0x226bcbe6, 0x3d923326, 0xcfbffe81, 0xef5ad5d0, 0x82bc23ac, 0x3aab41da, 0xcc4f90cf, 0xc7b88d8b, 0xce9fed41, 0xdb955bb8, 0x18112873, 0xc9887340, 
    0x677acece, 0x28513abe, 0x888d73d8, 0x928aa2e8, 0x2a8aa205, 0xcf7e1544, 0x0875d443, 0x0458fb3c, 0x8904e7b1, 0x82fb6014, 0x76852a72, 0x2d1718fe, 
    0x0616b9e1, 0x79b39455, 0x1f60b745, 0xd3d7f17c, 0x2aaae6f9, 0x4945b8ec, 0xcec59cd9, 0xc259a983, 0x11b7b865, 0xa94ceaa0, 0x5e9f35fe, 0x9dba6895, 
    0x7fcd9cb5, 0xed527724, 0x76856325, 0xf99ef1fa, 0x5ee7b7e3, 0x48ba226f, 0xbab222c9, 0xc1301892, 0xca69be07, 0xa3e21c36, 0xa228dab0, 0x285acca0, 
    0x5641a4a2, 0x2e2fec2d, 0xa4b5bdd0, 0x2d3828f3, 0x3f80651c, 0xf3ae5785, 0x1e9d80c0, 0x79be0ee4, 0x2af2fdc7, 0x54ecaea2, 0x71eccc23, 0xfad800ff, 
    0x9800ffa0, 0xf8fddf6d, 0x64aaf06f, 0x08467010, 0x6dd24faf, 0xa7386bf5, 0x3b92d1fe, 0x1163809c, 0x511f333b, 0x577a60f3, 0x8d20ea9c, 0x9830e74a, 
    0x2c1f2b73, 0x73820aae, 0x6c94538c, 0x55399c8a, 0x4551d4ca, 0x28ca9841, 0x2b28b6a2, 0x17b8f0a9, 0x95236e3a, 0x4e0e59c8, 0xfcb0ef7e, 0xe51a7f30, 
    0xc623c9a9, 0x0d654708, 0x78c629d4, 0xf913f923, 0x97d9699a, 0x9dbb7219, 0xba6c5aac, 0x3569a274, 0xda95ab3b, 0xce565651, 0xfae4c432, 0xd867609c, 
    0x470e20f6, 0xf78aba51, 0x05e69e50, 0xcbb71b0b, 0xf8b3fd8e, 0x136afcf5, 0x3200ac3c, 0x656000b9, 0xfc011c8f, 0x51411f80, 0x4d818181, 0x9512ecbb, 
    0x8aa285d5, 0x8a16482a, 0xae00a928, 0x14afc3ab, 0x35b024ba, 0xcccc92cf, 0x910279b9, 0xfedcbeee, 0x9c56ae79, 0x0883c8b2, 0xe2d8a3ec, 0x4665769a, 
    0x53e7ae5c, 0x0c8d59a2, 0xa5de2d4d, 0x0180e16a, 0xc6119423, 0xe739407d, 0x0656faf5, 0xde1d73af, 0x20d13ca7, 0x06802d54, 0x7fc67130, 0x558dbf1e, 
    0x87712469, 0x73ee6176, 0x96dea64d, 0x05ab2b07, 0x48525114, 0x484551b4, 0x0b1fbb82, 0x65d009c9, 0x8122ee81, 0x6906989d, 0xb88e3c00, 0xe7cc1f04, 
    0x3aae18df, 0x6576718a, 0xe7ae5c46, 0x435aa469, 0x92344ba0, 0xc0d236eb, 0xc05de5c3, 0xf67e7d12, 0x57f8eb07, 0xc75dab2f, 0x734da97d, 0x728e6010, 
    0xa91f7503, 0x953e00ff, 0x576e8a4e, 0xd5951b56, 0xa9288a82, 0xa2285a20, 0xbf5d41a4, 0xe22d6e82, 0x6b522ed2, 0x312de388, 0xfd0077c0, 0x15455cd1, 
    0x2a777651, 0x3b77e532, 0xd34c1bcd, 0xeade09f4, 0x41396e4d, 0x14823652, 0x3527237e, 0xf1f76bca, 0xf7cc5a6a, 0x62238250, 0x48309202, 0xeaac1903, 
    0xb2ba7228, 0x92d5951b, 0xa4a2280a, 0x00d9ff83, 
};
};
} // namespace BluePrint
