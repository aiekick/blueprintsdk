#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Brightness_vulkan.h>

namespace BluePrint
{
struct FadeFusionNode final : Node
{
    BP_NODE_WITH_NAME(FadeFusionNode, "Fade Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Mix")
    FadeFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Fade Transform"; }

    ~FadeFusionNode()
    {
        if (m_light) { delete m_light; m_light = nullptr; }
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
        auto percentage = context.GetPinValue<float>(m_Fade);
        if (!mat_first.empty() && !mat_second.empty())
        {
            int gpu = mat_first.device == IM_DD_VULKAN ? mat_first.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_first);
                return m_Exit;
            }
            if (!m_light || m_device != gpu)
            {
                if (m_light) { delete m_light; m_light = nullptr; }
                m_light = new ImGui::Brightness_vulkan(gpu);
            }
            if (!m_light)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            if (percentage <= 0.5)
            {
                float light = m_bBlack ? - percentage * 2 : percentage * 2;
                m_NodeTimeMs = m_light->filter(mat_first, im_RGB, light);
            }
            else
            {
                float light = m_bBlack ? (percentage - 1.0) * 2 : (1.0 - percentage) * 2;
                m_NodeTimeMs = m_light->filter(mat_second, im_RGB, light);
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
    bool Skippable() const override { return true; }

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        int black = m_bBlack ? 0 : 1;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(100, 8));
        ImGui::PushItemWidth(100);
        ImGui::BeginDisabled(!m_Enabled);
        ImGui::RadioButton("Black", &black, 0); ImGui::SameLine();
        ImGui::RadioButton("White", &black, 1);
        if ((m_bBlack && black != 0) || (!m_bBlack && black != 1)) { m_bBlack = black == 0; changed = true; };
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
        if (value.contains("black"))
        { 
            auto& val = value["black"];
            if (val.is_boolean())
                m_bBlack = val.get<imgui_json::boolean>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["black"] = imgui_json::boolean(m_bBlack);
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
    bool m_bBlack       {true};
    ImGui::Brightness_vulkan * m_light   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3506;
    const unsigned int logo_data[3508/4] =
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
    0xa08aa228, 0x8aa22831, 0xb3a22800, 0xb2e7abb5, 0x82295fb5, 0x7080edca, 0xf37f070e, 0x95b449ef, 0x56341ad8, 0xcfa88b46, 0x5db02c73, 0x465e6436, 
    0x38ee0354, 0x92517aad, 0xa200ba92, 0xc4802a8a, 0x00288aa2, 0x7dab8aa2, 0xbb46d070, 0xa78f190e, 0xaea44d6a, 0x5145b5c0, 0x5992bab1, 0x4e4e391a, 
    0x5ec53832, 0x2b2919a5, 0xa2280aa0, 0x283a0ca8, 0x280a80a2, 0xa5c0b1a6, 0xc0aea226, 0x39d51475, 0x19a54eeb, 0x0aa02b29, 0x0fa8a228, 0x288aa252, 
    0x5f2fae00, 0xc49361d4, 0x9e950971, 0x0503420b, 0xfd534fea, 0xa55de107, 0x2cda6967, 0xd7cc751a, 0xcbb22c29, 0x0869de37, 0x2be8c9f5, 0x44cbc539, 
    0xdbdf7234, 0xf000ff16, 0x6ea32693, 0x901347b2, 0xd7810f25, 0x43b7d383, 0xa35ad75d, 0xfa16e9aa, 0xec1c49bc, 0x19b2b1eb, 0x35a79f31, 0x98bc086e, 
    0xfbec2d12, 0xeae62e14, 0xe2224471, 0x147dc0dd, 0xc4085a51, 0x00288aa2, 0xbb5667ae, 0x775d5d8e, 0x4040d182, 0xfff57720, 0xaae90a00, 0x15979e95, 
    0xbb32ef9c, 0x2dc1b8bb, 0x3867ad8f, 0x8c8668b9, 0x3544a823, 0x3284bc95, 0x37bc1123, 0x55577ae4, 0xe95faad6, 0x8b086af1, 0x72286529, 0x10abf50a, 
    0x91c2e4c7, 0x40112c06, 0x8468eac9, 0x0fb85b5c, 0x412b8aa2, 0x45519418, 0x79911500, 0xcf79c932, 0xf5188728, 0xaf7aadf5, 0x21419a05, 0x8cc4cc70, 
    0x2ece5973, 0xce684856, 0xdf8b42fb, 0x9c51013d, 0x63ebfc91, 0x6c7143ad, 0x12772197, 0xa522f40a, 0xd5b83c8d, 0x8cd14e32, 0x1617219a, 0xa2a803ee, 
    0x1d46d08a, 0x05405114, 0x4fcd8f31, 0x59830ba4, 0x92958bd4, 0x9b9f1b1a, 0x3342fa34, 0x4214384a, 0x18dc2d2e, 0xa0154551, 0x13fe9f8e, 0x7f87fa7f, 
    0xc6fe6fe4, 0xff4df88f, 0xff1dea00, 0xfbbf9100, 0xae68e51a, 0xe52e695f, 0xe1bfea58, 0xa800ff37, 0xff46fe77, 0xff68ec00, 0xfedf8400, 0x1bf9dfa1, 
    0xaeb100ff, 0xa43d8a56, 0xab8e85bb, 0xfa7f13fe, 0x6fe47f87, 0xf88fc6fe, 0xea00ff4d, 0x9100ff1d, 0xe51afbbf, 0x4bdaa368, 0xbfea58b8, 0x00ff37e1, 
    0x46fe77a8, 0x68ec00ff, 0xdf8400ff, 0xf9dfa1fe, 0xb100ff1b, 0x3d8a56ae, 0x8d85bba4, 0xbf49f8cf, 0x00ffcfe9, 0xf500ff22, 0x8400ffa8, 0xff9cfe9b, 
    0xff2ff200, 0xa8b05a00, 0xeeb3f6a8, 0xe17f3716, 0xa700ff26, 0xff8bfc3f, 0xfea3d600, 0x73fa6f12, 0xbfc800ff, 0xa2c26afd, 0xe13e6b8f, 0x12fe7763, 
    0xff73fa6f, 0xfdbfc800, 0x26e13f6a, 0x3fa700ff, 0x00ff8bfc, 0x282aacd6, 0x16eeb3f6, 0x26e17f37, 0x3fa700ff, 0x00ff8bfc, 0x12fea3d6, 0xff73fa6f, 
    0xfdbfc800, 0x8fa2c26a, 0x63e13e6b, 0xfe57fb43, 0xe300ff98, 0x3f6afddf, 0x8fe97fb5, 0x00ff3dfe, 0x28faacd6, 0x16eeb3f6, 0x7fb53f34, 0x3dfe8fe9, 
    0xa3d600ff, 0x98fe57fb, 0xdfe300ff, 0xa2cf6afd, 0xe13e6b8f, 0x57fb4363, 0x00ff98fe, 0x6afddfe3, 0xe97fb53f, 0xff3dfe8f, 0xfaacd600, 0xeeb3f628, 
    0xb53f3416, 0xfe8fe97f, 0xd600ff3d, 0xfe57fba3, 0xe300ff98, 0xcf6afddf, 0x3e6b8fa2, 0xfddb61e1, 0x4deddfa8, 0x3e6b8fa2, 0xfddb61e1, 0x4deddfa8, 
    0x3e6b8fa2, 0xfddb61e1, 0x4deddfa8, 0x3e6b8fa2, 0xfddb61e1, 0x4deddfa8, 0x3e6b8fa2, 0xa26861e1, 0x0d29008a, 0x004b252d, 0x4b494bef, 0xa230ec42, 
    0x25842a8a, 0x0c595114, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 0x008aa228, 
    0x008aa228, 0xaa28295a, 0xa46801e6, 0x16608ea2, 0xe6284a8a, 0xa2a46801, 0x280a608e, 0x8a02a4a2, 0x8a02a028, 0x8a02a028, 0x8a02a028, 0x8a02a028, 
    0x8a02a028, 0x8a02a028, 0x8a02a028, 0x939b8a96, 0x4b4545cc, 0x620e1745, 0x285a2a2a, 0x511173b8, 0xc345d152, 0x288a8a98, 0x582a28aa, 0xeea23cd5, 
    0x54518333, 0x31b090d1, 0x283d2323, 0x22473601, 0x3d529992, 0x199d2a6a, 0xed360565, 0x0fa8781d, 0x280a8006, 0x280a80a2, 0xd20a80a2, 0x36b5b6b4, 
    0x2ca37906, 0x3fc13957, 0xafb559e3, 0x253de264, 0x1938909d, 0x00f49eca, 0xbcb2b5c4, 0xd100de81, 0xfef1fe15, 0xb62e6b7d, 0xfa8a662d, 0x18102039, 
    0x0f251953, 0x79229315, 0x3b673272, 0xa0f51949, 0x1445d106, 0x14455100, 0xa6415700, 0x687bfae9, 0x80ba6cdf, 0x88c55db9, 0xfd000962, 0xabae9f6b, 
    0xf061954a, 0x12e395d1, 0x0f25072a, 0x40f1e446, 0x69a6c510, 0x929c9d3a, 0x57c0a3d9, 0x074f2c23, 0x154ff21d, 0xa757d7cd, 0x5bac43dd, 0x0d2d6a4f, 
    0xe57360b4, 0x403f07b6, 0x88b8982b, 0x402c7343, 0xcf9523e4, 0x47000dae, 0x04501445, 0x414551b4, 0xa3595798, 0x9e4c3ae9, 0xed851a1e, 0x3b9491b1, 
    0x4860073b, 0xb906f604, 0x3974ee3a, 0xe6c1db96, 0xb14a8c77, 0xf4508cac, 0x34c5136c, 0x8b064134, 0x1216eba0, 0x901c614b, 0x2c6e2415, 0x90671cdc, 
    0xdab8e249, 0xf89b34ef, 0x8b3bfb35, 0xac35b15f, 0x6ff96040, 0x00ee7380, 0x01f22aae, 0xba3d7b6b, 0x8c14e1b6, 0xd0e0faa0, 0x455190c1, 0x45458414, 
    0xfd0ffb5a, 0xc700ff34, 0xfd0ffb68, 0xc700ff34, 0x0f91f668, 0x5ae50e69, 0x30c84a7a, 0x7f6afc00, 0xd300ffb0, 0x8f76fc4f, 0xd300ffb0, 0x8f76fc4f, 
    0x90f61069, 0x726742ee, 0x54fac031, 0x3fec6b75, 0x00ffd3f4, 0x3feca31d, 0x00ffd3f4, 0x44daa31d, 0x953ba43d, 0x615fab68, 0x9fa600ff, 0x611fedf8, 
    0x9fa600ff, 0xd21eedf8, 0xdc21ed21, 0x7f6c45ab, 0x00ff3fc2, 0x0ff95f4f, 0x47af00ff, 0x00ff23fc, 0x00fff5f4, 0xfa00ff90, 0x3dc473f4, 0xf5987ba4, 
    0x9a51df6a, 0x2a1a21da, 0x07790e10, 0xfcf76afc, 0xf400ff23, 0x9000fff5, 0xf4fa00ff, 0xff3fc27f, 0xf95f4f00, 0xaf00ff0f, 0xda433c47, 0x4d52b943, 
    0xa8d0e55a, 0xa3ee99d8, 0x1f5ba59a, 0x00ff8ff0, 0x43fed7d3, 0xd1eb00ff, 0xff0800ff, 0x7f3dfd00, 0xbdfe3fe4, 0x690ff11c, 0x153de60e, 0x0800ffb1, 
    0x3dfd00ff, 0xfe3fe47f, 0x8ff01fbd, 0xd7d300ff, 0x00ff43fe, 0x10cfd1eb, 0x63ee91f6, 0xfc375dd1, 0xfff4df21, 0x9000ff00, 0x3f2afb7f, 0x00ff0ee1, 
    0xfc00ffa7, 0xd900ff83, 0xf610cf51, 0x3573ee91, 0xdcae65a5, 0xbc2ddbd8, 0xa4cac471, 0xf31c5890, 0xfca7d6f8, 0xfff4df21, 0x9000ff00, 0x3f2afb7f, 
    0x00ff0ee1, 0xfc00ffa7, 0xd900ff83, 0xf610cf51, 0x9750ee90, 0x21d2d7c4, 0xcfa35855, 0x8f9c2af1, 0x2441d6cc, 0x3dc94992, 0xf0df744d, 0xffd37f87, 
    0xff41fe00, 0xffa8ec00, 0xfe3b8400, 0xf200ff9f, 0x6500ff0f, 0xda433c47, 0xd1ccb943, 0x21fc375d, 0x00fff4df, 0x7f9000ff, 0xe13f2afb, 0xa700ff0e, 
    0x83fc00ff, 0x51d900ff, 0x91f610cf, 0x57d473ee, 0x0800ff63, 0xff44fd07, 0xff07f200, 0xfc476500, 0x13f51f20, 0x1fc800ff, 0x3a4f95fd, 0x1d279e23, 
    0x8a6f7a5b, 0x24cbb42e, 0x9d088eb5, 0x670b9250, 0xd6fa9c3c, 0x1f20fcb7, 0x00ff13f5, 0x95fd1fc8, 0x7f80f01f, 0x00ff4fd4, 0x54f67f20, 0x8fe7a073, 
    0x197f3e73, 0x52c432ea, 0x9188e138, 0x2309ca8d, 0xfb5c93e9, 0xc42c6631, 0x3d494e92, 0x00ffb0eb, 0xa2fe0384, 0xff03f97f, 0xfea3b200, 0x89fa0f10, 
    0x0fe400ff, 0x748ecafe, 0xce3dd21e, 0x7fec8a3a, 0x00ff00e1, 0x40fe9fa8, 0xa8ec00ff, 0x038400ff, 0xf97fa2fe, 0xb200ff03, 0x3c079da3, 0x288a7a4e, 
    0xad700eac, 0xea0e3f3b, 0xdcb285ba, 0x91f9dada, 0x62de7036, 0xa1e3e08c, 0x875e9935, 0x9be5a8e1, 0x121ccbc1, 0x90b85279, 0x7400ff23, 0x503538ee, 
    0x8a70d98d, 0x0b39b393, 0x16eb0d9f, 0x59d3b490, 0x25194530, 0xf0035b59, 0xd4ebb226, 0x74d4212d, 0xb56f26dd, 0xe38cda4a, 0x317d5525, 0xe6b59ed3, 
    0xab58a50c, 0x20380802, 0xd838a7f6, 0x848d8a73, 0x0c2a8aa2, 0xa228cacd, 0xb3d20a80, 0xa8abf6f0, 0xadcd2d5b, 0x671399af, 0xce28e60d, 0x59133a0e, 
    0x1a7ee8b5, 0x1cbc598e, 0x9527c1b1, 0x3f02892b, 0x83e34ef7, 0x97dd0857, 0x333ba908, 0xdef0b98f, 0x4d0b69b1, 0x51049335, 0xb0955592, 0x326b023f, 
    0x1dd23ebd, 0x66d24d47, 0xa8ad54fb, 0x575532ce, 0xeb391dd3, 0x55ca625e, 0x8320b08a, 0x276a0f82, 0x51710e1b, 0x455194b0, 0x45b79941, 0x5a015014, 
    0xd51e7e56, 0xb9650b75, 0x22f3b5b5, 0xc5bce16c, 0x42c7c119, 0x0fbd366b, 0x37cb51c3, 0x24389683, 0x2071a5f2, 0xdce9fe47, 0x1be16a70, 0x2715e1b2, 
    0x3e177266, 0x212dd61b, 0x60b2a669, 0xb24a328a, 0x4de007b6, 0xdaa7d765, 0xbae9a843, 0x956adf4c, 0x4ac619b5, 0xa763faaa, 0x59cc6b3d, 0x0456b14a, 
    0xed417010, 0xce61e344, 0x8a12362a, 0x3633a828, 0x008aa2e8, 0xc3cf4a2b, 0x6ca1aeda, 0xbeb636b7, 0x379c4d64, 0x3838a398, 0xd7664de8, 0x396af8a1, 
    0xc772f066, 0xae549e04, 0x00ff0824, 0x0d8e3bdd, 0x5c76235c, 0xceeca422, 0x7ac3e742, 0x342da4c5, 0x46114cd6, 0xc0565649, 0xbaac09fc, 0x7548fbf4, 
    0x9b493b1d, 0xa3b652ed, 0x5f55c938, 0xade7744c, 0x56298b79, 0x0e82c02a, 0x9ca83d08, 0x46c5396c, 0x154551c2, 0xd1966706, 0x57001445, 0xac35a079, 
    0x14fb0ffe, 0xd2427197, 0x304b88ac, 0x719ee4ca, 0xaae8e09a, 0x6554ac8c, 0xd240cfca, 0x337cd320, 0xac4bf204, 0xf9c0e124, 0xe8710041, 0x876b9ea0, 
    0xf3562ed4, 0x55b9b850, 0xccc8b2da, 0xab26a007, 0xd59543d1, 0x585d2987, 0x928aa228, 0xa228ca0d, 0x34ef0a80, 0xc19fb506, 0x926200ff, 0x555a28ee, 
    0x19660991, 0x33ce935c, 0x51151d5c, 0xb98c8a95, 0x30e9df59, 0x8219be69, 0x12d62579, 0xa07ce070, 0x50f43880, 0x5fc4354f, 0xa1deaddc, 0xb5ab7271, 
    0x0f989165, 0xa2574d40, 0x0eab2b87, 0x51b0ba52, 0x1b241545, 0x00455174, 0x0d68de15, 0xfe833f6b, 0x50dc25c5, 0x1222abb4, 0x27b932cc, 0x3ab8669c, 
    0x152ba32a, 0xbfb37219, 0x7cd360d2, 0x4bf20433, 0xc0e124ac, 0x710041f9, 0x6b9ea0e8, 0x5bb9bf88, 0xe5e242cd, 0x23cb6a57, 0x9a801e30, 0x570e45af, 
    0x75a51c56, 0x2a8aa260, 0xa2e83648, 0xbc2b008a, 0x7fd61ad0, 0x4b8afd07, 0x5669a1b8, 0x65982544, 0xcd384f72, 0x46557470, 0xe5322a56, 0x3069a067, 
    0x8219be69, 0x12d62579, 0xa07ce070, 0x50f43880, 0xdfc3354f, 0xa1e6addc, 0xb5ab7271, 0x0f989165, 0xa2574d40, 0x0eab2b87, 0x51b0ba52, 0x1f241545, 
    0x0000d9ff, 
};
};
} // namespace BluePrint
