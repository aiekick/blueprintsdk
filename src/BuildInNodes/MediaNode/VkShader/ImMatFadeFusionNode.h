#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Fade_vulkan.h>

namespace BluePrint
{
struct FadeFusionNode final : Node
{
    BP_NODE_WITH_NAME(FadeFusionNode, "Fade Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Mix")
    FadeFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Fade Transform"; }

    ~FadeFusionNode()
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
        auto progress = context.GetPinValue<float>(m_Fade);
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
                m_fusion = new ImGui::Fade_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_type, m_color);
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
        int _type = m_type;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::RadioButton("Normal", &_type, 0); ImGui::SameLine();
        ImGui::RadioButton("Colorful", &_type, 1); ImGui::SameLine();
        ImGui::RadioButton("Gray", &_type, 2);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_type##Fade")) { m_type = 1; changed = true; }
        ImPixel _color = m_color;
        if (ImGui::ColorEdit4("##Fade", (float*)&_color, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar))
        {
            m_color = _color; changed = true;
        } ImGui::SameLine(); ImGui::TextUnformatted("Fade Color");
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_color##Fade")) { m_color = {0.0f, 0.0f, 0.0f, 1.0f}; changed = true; }
        if ((m_type != _type)) { m_type = _type; changed = true; };
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
        if (value.contains("type"))
        { 
            auto& val = value["type"];
            if (val.is_boolean())
                m_type = val.get<imgui_json::boolean>();
        }
        if (value.contains("color"))
        {
            auto& val = value["color"];
            if (val.is_vec4())
            {
                ImVec4 val4 = val.get<imgui_json::vec4>();
                m_color = ImPixel(val4.x, val4.y, val4.z, val4.w);
            }
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["type"] = imgui_json::boolean(m_type);
        value["color"] = imgui_json::vec4(ImVec4(m_color.r, m_color.g, m_color.b, m_color.a));
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
        // if show icon then we using u8"\ue3e8"
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
    FloatPin  m_Fade = { this, "Fade" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[4] = { &m_Enter, &m_MatInFirst, &m_MatInSecond, &m_Fade };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device        {-1};
    ImPixel m_color {0.0f, 0.0f, 0.0f, 1.0f};
    int m_type {1};
    ImGui::Fade_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3568;
    const unsigned int logo_data[3568/4] =
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
    0xd12aa08a, 0x14201545, 0x8bb55451, 0xcba661d3, 0xc624b632, 0x1aed3fd4, 0x655cd94d, 0xd09c2bda, 0xefd98bb5, 0xfb7ccdfe, 0x1eddc4bc, 0x61bf2f54, 
    0x4a4757dc, 0x40575232, 0x50455114, 0x4551148a, 0x2a8ac240, 0x6c74a7a6, 0x4e9524ec, 0xa94f181f, 0xb780dda4, 0x1ae96145, 0x9ed7d49d, 0xdeeedb45, 
    0x072a939b, 0xdc6a6fbf, 0x707517a5, 0xa8a2280a, 0xa228ea0c, 0x455118b3, 0xec9fbe57, 0x38208fd6, 0xa0f58b6e, 0x5e56140b, 0x973c7d9d, 0x67c3553e, 
    0x0072e572, 0xdc494aad, 0xa6288a02, 0x14455507, 0x5c214957, 0x552f8c8f, 0x8eec2def, 0xc9f34776, 0xb7fe7f8f, 0xb3baaef3, 0x8b20d1a1, 0xcb487d56, 
    0x783e932c, 0xd367606c, 0x262a4e8f, 0xd190959c, 0xc53aeac9, 0xad9ddab4, 0xc47294ed, 0x301800d0, 0x3a1e2003, 0xee8a4c1f, 0x852795a1, 0x1de58c25, 
    0x0dea0343, 0xf0e9a841, 0xda8676ea, 0x0449c172, 0xf408ea15, 0x2dda58a7, 0xc9769c8d, 0x30c6ba23, 0xfc38e30b, 0x2d2e4629, 0x51140bdc, 0xad226845, 
    0x01525114, 0xef8ab75c, 0xb4a0ee15, 0xce1fd939, 0x00fff7f8, 0xea3a7feb, 0x3c5a846a, 0xa86f6a31, 0x5b792419, 0x3330363c, 0x44a5c7e9, 0x67c86ad3, 
    0xdbaa7f31, 0x5edba8cb, 0x6624c759, 0x000c0110, 0x1f3a1dc8, 0x8ab32b4e, 0x95249a45, 0x30d4550e, 0xbfa1c63e, 0xb5518bb1, 0x1412f336, 0xa05e4190, 
    0x56edacd3, 0xd12dd5ca, 0x0106d5d9, 0xa314676c, 0x7a029b16, 0x11b4a228, 0xa8288a42, 0xe23d5718, 0x886b952b, 0xa7768ead, 0xfff7f8cc, 0x3a7feb00, 
    0x7a9c6ae8, 0xb5df5164, 0x1b7977e9, 0x2a30363c, 0x40569b64, 0x8c9af773, 0x73c15e12, 0x20868e6c, 0x90810d01, 0x1c55573a, 0x226b2c8b, 0x4100ab1c, 
    0x232dafa8, 0xcb30b7be, 0x23070990, 0xdb5aa7a8, 0x81745b8b, 0x80539959, 0xa628ad5b, 0x45d11298, 0x51676015, 0xc2981545, 0xc199f5b1, 0x764e389a, 
    0xadd8ccaf, 0x3196ac8a, 0x965cdba5, 0x8ceb7366, 0x5657520a, 0xf2e62203, 0x788aba27, 0x00a09415, 0xe85bed83, 0x535744e2, 0xa322c390, 0xa84bb7b9, 
    0x33904f4c, 0x96a28e9c, 0xac104408, 0xe8058b41, 0x34495a4f, 0x288a92c0, 0xfcef02aa, 0xe7f4df24, 0x3f9100ff, 0xc27fd4fa, 0x4e00ff4d, 0xff13f97f, 
    0x5458ad00, 0x1624ed51, 0x26e17f37, 0x3fa700ff, 0x00ff89fc, 0x12fea3d6, 0xff73fa6f, 0xfd9fc800, 0x66a4c26a, 0xe098a508, 0xb4479301, 0xbdb17097, 
    0x370900ff, 0x00ff39fd, 0xb5fe4fe4, 0x7f93f01f, 0x44fe9fd3, 0x57eb00ff, 0x4cf11c3d, 0x8eb38d48, 0x7b9454bc, 0x1b0b7749, 0x7f93f0bf, 0x44fe9fd3, 
    0x51eb00ff, 0x370900ff, 0x00ff39fd, 0xb5fe4fe4, 0xb4475161, 0xffd15890, 0xfea38400, 0xf200ff9c, 0x5a00ff2f, 0x3f4af88f, 0x00ffcfe9, 0xf500ff22, 
    0x8f8a02ab, 0x63e12e69, 0x8f12fe7f, 0x00ff73fa, 0x6afdbfc8, 0xff28e13f, 0xfc3fa700, 0xd600ff8b, 0x40420aac, 0x8f829e04, 0x63e12e69, 0x8400ffa0, 
    0xff9cfea3, 0xff2ff200, 0xf88f5a00, 0xcfe93f4a, 0xff2200ff, 0x9eabf500, 0xe9fe1559, 0x7bd429ce, 0x1b0b7749, 0x94f000ff, 0xfe9fd37f, 0xeb00ff45, 
    0x0900ff51, 0xff39fd47, 0xfe5fe400, 0x475160b5, 0xb17097b4, 0x0900ffa7, 0x7f3afd27, 0xb5fe4fe4, 0x7f92f01f, 0x44fea7d3, 0x56eb00ff, 0xf23c151d, 
    0xf09f1b0b, 0xa7d37f92, 0x00ff44fe, 0x00ff51eb, 0x3afd2709, 0xfe4fe47f, 0x51d261b5, 0x371624ed, 0xff24e17f, 0xfc4fa700, 0xd600ff89, 0x4f12fea3, 
    0x00ff74fa, 0x6afd9fc8, 0x941e04c2, 0x05497bb4, 0x49f8cf8d, 0xffd3e93f, 0xf57f2200, 0x8400ffa8, 0x3f9dfe93, 0x00ff27f2, 0xa3e8b05a, 0x4763419e, 
    0x9dfe77fb, 0x00fff1bf, 0xdb1fb5fe, 0xffedf4bf, 0x00ff8f00, 0x8a26abf5, 0xb766ec5c, 0x3bfdeff6, 0x00ffe37f, 0xb73f6afd, 0xffdbe97f, 0x00ff1f00, 
    0x144d56eb, 0xbf35b073, 0xdbe97fb7, 0xff1f00ff, 0xfd51eb00, 0x4e00ffbb, 0x00fff8df, 0xb25a00ff, 0x819da368, 0xffbbfdad, 0xf8df4e00, 0x00ff00ff, 
    0xdfed8f5a, 0x00ff76fa, 0xfa00ffc7, 0x1c4593d5, 0x28ca0dec, 0x5541aca2, 0x10dbe46f, 0x5673758c, 0x49d184aa, 0x998f7973, 0x8a0d7855, 0x7e550396, 
    0x7a1e77c9, 0xa335c130, 0xc8d9f359, 0x69cdbfa9, 0xbb8aed08, 0xa5c23303, 0x418e0d81, 0x451f2014, 0x46015514, 0x00a8288a, 0x081baea8, 0x2d357514, 
    0x4dd2ea40, 0xf11d408e, 0x3cd01843, 0x8ad05799, 0x85e455b3, 0x6c71e782, 0x9290a976, 0x49839180, 0xa228ea00, 0x51d48a98, 0x9ac24845, 0xa9538ce7, 
    0x006a1684, 0x7d0aeb3e, 0x5cc7a930, 0x004a87d3, 0x80a2285a, 0x298aa222, 0x14455100, 0x14455100, 0x14455100, 0x6145b901, 0x23cc57d1, 0xa2c38a72, 
    0x2837608e, 0xe6283aac, 0xc38a7203, 0x2f608ea2, 0xa9a85551, 0x15cd02e6, 0x80398a5a, 0xa25645b3, 0xd12c608e, 0x98a3a855, 0x45b98a02, 0x3ae5c24d, 
    0x5c14e52a, 0xaea2532e, 0xe5c24551, 0x14e52a3a, 0x51672e5c, 0xb9140d5d, 0xd473cc85, 0x1c454357, 0xd473ccc1, 0x1c454357, 0xd473ccc1, 0x1c454357, 
    0xd473ccc1, 0x41514551, 0x01501445, 0x01501445, 0x04501445, 0x214551b4, 0x11143d05, 0x9154a6c8, 0x604cd3fd, 0x4c41b702, 0xb67a1a3c, 0x8c29c678, 
    0x04a0d830, 0x08298aa2, 0x521445b9, 0xbcc52a24, 0x2e4be848, 0xbd6a7d70, 0x00ffb55a, 0x105adf56, 0x4b7024d0, 0x558ca09d, 0x8f581b66, 0x14b58a43, 
    0x265442a2, 0x9557a5df, 0x74544802, 0x14650ca6, 0x85464851, 0x48525114, 0x680bfb55, 0x449a8566, 0x9a7a43dc, 0x679a5aa1, 0xf7feef16, 0x5b8aa6f4, 
    0x57161489, 0xc0688421, 0xd259738f, 0x3319c92e, 0xd68ca49d, 0xdcd2ada5, 0xd8214633, 0x674dd531, 0xb82144dd, 0x3d905364, 0x107bb069, 0x241545d1, 
    0xb515f59c, 0xef0800ff, 0xe47f3dfd, 0x1fbdfe3f, 0x00ff8ef0, 0x43fed7d3, 0xd5eb00ff, 0x91f92273, 0x7f6d458b, 0x00ff3bc2, 0x0ff95f4f, 0x47af00ff, 
    0xf4bf23fc, 0x9000fff5, 0xf4fa00ff, 0x46e62073, 0xffb5152d, 0xfdef0800, 0x3fe47f3d, 0xf01fbdfe, 0xd300ff8e, 0xff43fed7, 0xccd1eb00, 0xb4189983, 
    0x23fcd756, 0xfff5f4bf, 0x00ff9000, 0xc27ff4fa, 0x4f00ff3b, 0xff0ff95f, 0x3247af00, 0x5165640e, 0x22fc175d, 0xfffbf47f, 0xfabf9000, 0x27c27ff4, 
    0xbf4f00ff, 0x00ff0bf9, 0x0b994baf, 0x4e3b279e, 0xa02bda56, 0x4f8400ff, 0xf27f9ffe, 0x5e00ff17, 0xff44f88f, 0xfff7e900, 0xf57f2100, 0xe720f3e9, 
    0x1fe68189, 0xe85a6f4a, 0xff13e17f, 0xfcdfa700, 0xd700ff85, 0x3f11fea3, 0x00ff7dfa, 0x7afd5fc8, 0xc4739039, 0x8bae68e7, 0xfa3f11fe, 0xc800ff7d, 
    0x3f7afd5f, 0x00ff13e1, 0x85fcdfa7, 0xa5d700ff, 0x269e83cc, 0xffd2154d, 0xff1fc200, 0x00ff4f00, 0x00ff07f9, 0x10fea3b2, 0x7ffa00ff, 0x3fc800ff, 
    0x642e95fd, 0x6a4e3c4f, 0x12738aa4, 0x35390090, 0xc200ffd0, 0x4f00ff1f, 0x07f900ff, 0xa3b200ff, 0x00ff10fe, 0x00ff7ffa, 0x95fd3fc8, 0xe239e81c, 
    0xc7b79b60, 0x4942050a, 0x26394962, 0x43f84fba, 0xffe900ff, 0x2000ff00, 0x54f600ff, 0xff1fc27f, 0x00ff4f00, 0x00ff07f9, 0x079da3b2, 0xd59c7b3c, 
    0x00ffd215, 0x00ff1fc2, 0xf900ff4f, 0xb200ff07, 0xff10fea3, 0xff7ffa00, 0xfd3fc800, 0x39c81c95, 0x5d5165e2, 0x5f22fc1f, 0x00fffdf4, 0xf4fabf90, 
    0xff25c27f, 0xf9df4f00, 0xaf00ff0b, 0xe789cc53, 0x9b55ce89, 0x42b6c76b, 0xe7e4a0aa, 0x00ffda9a, 0x9ffe4b84, 0xff17f2bf, 0xf88f5e00, 0xfbe9bf44, 
    0x7f2100ff, 0x41e7e8f5, 0xaaa38cce, 0xa008474a, 0x6636d5fa, 0x24c72c76, 0xffd035f5, 0xff25c200, 0xf9df4f00, 0xaf00ff0b, 0x5f22fc47, 0x00fffdf4, 
    0xf4fabf90, 0x457bd0f9, 0xae28e7dc, 0x2f11fe8f, 0x00ff7efa, 0x7afd5fc8, 0xff12e13f, 0xfcefa700, 0xd700ff85, 0x9e83cca5, 0xa2280a25, 0xb7580581, 
    0x42bab9b1, 0x41bdc7f0, 0xe7803bc1, 0xbff5aaf1, 0x2e6573a0, 0x05fdbf3f, 0x8acaae34, 0x49db32bb, 0x6d2c45be, 0x03a307ce, 0xe9548dfc, 0x37a44d5d, 
    0x79e6ea56, 0xd5fde4cc, 0xe67afd1f, 0x991b77ae, 0x2e46a14b, 0x9adb5349, 0xac24586d, 0x54144547, 0x45516a92, 0x5b052415, 0x6fefd2b4, 0x435b32a3, 
    0x9de054bd, 0x9af873c0, 0x855f5da9, 0xc0d97472, 0xe49fe6eb, 0x955dc529, 0x11637715, 0x354e1df4, 0x818dd62c, 0x917fcae8, 0x7030f2ac, 0xedd3b96b, 
    0x5312adaf, 0x896e7275, 0x8f3aaafb, 0x7819d7c4, 0x64ccde5c, 0x21cdc68c, 0xe4f00f25, 0xace414e7, 0x34c44639, 0x06494551, 0x50511495, 0x1deb5548, 
    0x62d4501b, 0xfc2d6d69, 0x4eda56c4, 0x26fe1cf5, 0xe06bd7a8, 0x70934eb0, 0x8ef3c101, 0x5591ef3f, 0xa1627715, 0x9e63671e, 0xc45ac393, 0x9bac5d48, 
    0x56d7ae03, 0x9635903f, 0x83070741, 0xdaa58d5e, 0xcc31366a, 0xf066b7d7, 0xea200a90, 0x0d5c133f, 0x5ca873f9, 0x9d189134, 0x7f2866a4, 0xa7382787, 
    0x8a73d828, 0x1445af48, 0xa25c1054, 0x1512298a, 0xd443c37e, 0x5b5a4875, 0x15356f3b, 0x57bd93b6, 0xf53ef107, 0x02cfbb42, 0x907b7402, 0xe39c090e, 
    0x5115f9fe, 0x112e7657, 0x35677652, 0xb8b5852f, 0x3658bb50, 0xacae5d07, 0x206b207f, 0xc1080e82, 0x4dfae915, 0xc5a7a79e, 0x9b5e5f3b, 0x832840d2, 
    0xf5b9c7a8, 0x4cd439af, 0x69b95287, 0x4abb3062, 0xc33f63cc, 0x94539c93, 0x2838876c, 0xa2a895ab, 0x8d33838a, 0x632b8aa2, 0x0ecdad70, 0x55ced648, 
    0x7e491579, 0x61851de7, 0x0d3b4dd1, 0xca7e333b, 0x914fdbd6, 0x61e56ba5, 0x0f20638c, 0x853a59e7, 0xc9de5cc2, 0xf0940f2a, 0x14b5ea3d, 0xb0f43636, 
    0x89144551, 0x8aa22835, 0x7fba0292, 0x690a4d0c, 0xa2922cf3, 0xccfb2113, 0xc55c6107, 0x2ab3d314, 0x57e7ce2e, 0xe9d9d8a6, 0xa9efb432, 0xdbb88da3, 
    0x39f30790, 0xee5a0dac, 0x594add3b, 0xc0211fe2, 0xd681ef19, 0x7a43d1a9, 0xacae1c58, 0x22455114, 0xa2282a4d, 0xc7ae90a0, 0x1ec1f3c1, 0xcf927097, 
    0x38a5651c, 0x087fc00c, 0xa88a3aae, 0x19953bbb, 0xae9dbb72, 0x6858a793, 0xebcb3d73, 0x910ac811, 0xf2a310b4, 0x3a73cdc9, 0x867aecdd, 0x20cc35ad, 
    0xa480d888, 0xcd18b88e, 0x959ba250, 0xeaca81d5, 0x8aa228c8, 0x51944b82, 0xbb422245, 0x5bdc047f, 0xa45ca4c5, 0x5ac611d7, 0x01ee8063, 0x8ab8a2fb, 
    0xeeeca22a, 0xeeca6554, 0x99369a77, 0xbd13e8a7, 0x72dc9ad4, 0x046da482, 0x4e46fc28, 0xefd7946b, 0x99b5d4e2, 0x4604a1ee, 0x602405c4, 0x59330690, 
    0x75e550d4, 0xab2b3764, 0x45511424, 0xd9ff0749, 
};
};
} // namespace BluePrint
