#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <CircleBlur_vulkan.h>

namespace BluePrint
{
struct CircleBlurFusionNode final : Node
{
    BP_NODE_WITH_NAME(CircleBlurFusionNode, "CircleBlur Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Shape")
    CircleBlurFusionNode(BP* blueprint): Node(blueprint) { m_Name = "CircleBlur Transform"; }

    ~CircleBlurFusionNode()
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
                m_fusion = new ImGui::CircleBlur_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_smoothness, m_open);
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
        float _smoothness = m_smoothness;
        bool _open = m_open == 1;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Smoothness##CircleBlur", &_smoothness, 0.0, 1.f, "%.1f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_smoothness##CircleBlur")) { _smoothness = 0.3f; changed = true; }
        if (ImGui::Checkbox("Open##CircleBlur", &_open))
        {
            m_open = _open ? 1 : 0;
            changed = true;
        }
        ImGui::PopItemWidth();
        if (_smoothness != m_smoothness) { m_smoothness = _smoothness; changed = true; }
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
        if (value.contains("smoothness"))
        {
            auto& val = value["smoothness"];
            if (val.is_number()) 
                m_smoothness = val.get<imgui_json::number>();
        }
        if (value.contains("open"))
        {
            auto& val = value["open"];
            if (val.is_number()) 
                m_open = val.get<imgui_json::number>();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["smoothness"] = imgui_json::number(m_smoothness);
        value["open"] = imgui_json::number(m_open);
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
        // if show icon then we using u8"\uf140"
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
    float m_smoothness  {0.3f};
    int m_open          {0};
    ImGui::CircleBlur_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 4488;
    const unsigned int logo_data[4488/4] =
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
    0x280aa08a, 0x280a80a2, 0xf1dd1aaa, 0xa7a9b4d3, 0x3626db46, 0x3dda7fc6, 0x7693c63f, 0x15fd0257, 0x5b7bf8cb, 0x7ed4b8be, 0xbb49a8cb, 0x79748fcd, 
    0x87fdbe50, 0xa8abfc71, 0x2b2919a5, 0xa2280aa0, 0x933307a8, 0x498aa24d, 0x1405b025, 0xf86e0d55, 0xd354dae9, 0x1b936da3, 0x1eed3f63, 0x7643e39f, 
    0x15fd0257, 0x5b7bf8cb, 0x7ed4b8be, 0xbb49a8cb, 0x79748fcd, 0x87fdbe50, 0xa8abfc71, 0x2b2919a5, 0xb9720aa0, 0xb429da14, 0xf34ce09e, 0x2ca2d213, 
    0x2a4a7a4f, 0x68c50863, 0x280a9224, 0xf1dd1aaa, 0xa7a9b4d3, 0x3626db46, 0x3dda7fc6, 0xbba9c63f, 0x45bf8c2b, 0xd61efe72, 0x1f35aeef, 0x6e12eab2, 
    0x1edd63f3, 0x61bf2f54, 0xea2a7fdc, 0x4a4a4629, 0x470c05e8, 0x2326957a, 0x294543ad, 0x895a3242, 0x288ac2a4, 0x235718ab, 0x7bd50be3, 0x9d237bcb, 
    0x63f2fc91, 0xad00ffdf, 0xacaeebfc, 0x224874e8, 0x32529fd5, 0x9ecf24cb, 0xf419181b, 0x898ad3e3, 0x346425a7, 0xb18e7a72, 0x6ba7366d, 0xb11c657b, 
    0x0c060034, 0x8e07c800, 0xbb22d387, 0xe14965a8, 0x47396349, 0x83fac050, 0x7c3a6a50, 0xb6a19d3a, 0x4152b09c, 0x3d827a05, 0x8b36d629, 0xb21d6763, 
    0x8cb1ee48, 0x3fcef802, 0x8b8b510a, 0x14c50277, 0x28085a51, 0x2b008aa2, 0xea85f191, 0x91bde5bd, 0x79fec8ce, 0x00ffef31, 0xd775fed6, 0x243a7456, 
    0xa9cf6a11, 0x67926519, 0x0c8c0dcf, 0xc5e971fa, 0xb292d344, 0x473d391a, 0x539bb658, 0x8eb2bdb5, 0x03009a58, 0x03640006, 0x91e943c7, 0xa432d45d, 
    0x9cb1a4f0, 0x7d60a8a3, 0x1d35a841, 0xd04e1d3e, 0x29584edb, 0x41bd8220, 0x1beb941e, 0x8eb3b145, 0x587724d9, 0x677c01c6, 0xc528851f, 0x6281bbc5, 
    0x04ad288a, 0x00455114, 0xc2f8c815, 0xdef25ef5, 0x7f64e7c8, 0xfff7983c, 0x3a7feb00, 0x1d3aabeb, 0x67b50812, 0xc9b28cd4, 0xc686e733, 0xf4387d06, 
    0xc969a2e2, 0x9e1c0d59, 0x4d5baca3, 0xd9dedaa9, 0x004d2c47, 0x32008301, 0xf4a1e301, 0x19eaaec8, 0x58527852, 0x30d451ce, 0x1ad4a03e, 0xa70e9f8e, 
    0x2ca76d68, 0x5e419014, 0x754a8fa0, 0xd9d8a28d, 0x3b926cc7, 0xbe0063ac, 0x94c28f33, 0xc0dde262, 0x561445b1, 0x4a290a82, 0x3b494991, 0xe3235780, 
    0xcb7bd50b, 0x919d237b, 0xdf63f2fc, 0xfcad00ff, 0xe8acaeeb, 0xd5224874, 0xcb32529f, 0x1b9ecf24, 0xe3f41918, 0xa7998ad3, 0x72346425, 0x6db18e7a, 
    0x7b6ba736, 0x34b11c65, 0x000c0600, 0x878e07c8, 0xa8bb22d3, 0x49e14965, 0x50473963, 0x5083fac0, 0x3a7c3a6a, 0x9cb6a19d, 0x054152b0, 0x293d827a, 
    0x638b36d6, 0x48b21d67, 0x028cb1ee, 0x0a3fcef8, 0x778b8b51, 0x5214c502, 0xbbad2685, 0x01db9808, 0x3f2d00ff, 0xa346634a, 0x4a00fff8, 0xd4242559, 
    0x4d63cd59, 0x64f26a54, 0x14ab4b45, 0x4549eb81, 0xb7a36932, 0xb695e685, 0x8f598ac6, 0x1f8bada0, 0xaaf5004a, 0x8eb53a56, 0x93b6cea2, 0x580685f9, 
    0x981f236d, 0xb36f94ab, 0x4f8d4502, 0xf194fef1, 0xb4fc0f6c, 0xe20c2afd, 0x0d528ea5, 0x4a545163, 0x4b4964f1, 0x27fb24a3, 0x34a57ffb, 0x00ff81db, 
    0x924a3f2d, 0x99557149, 0x7b69acc9, 0x31d96a69, 0x18477de6, 0xe91f7fd4, 0x00ffc24d, 0xd3a6f47b, 0x783b9a26, 0x6c5b695e, 0xfa98a568, 0xad49ea0a, 
    0x2e99a5d9, 0xa5dffb17, 0xf8a74638, 0xcf4a00ff, 0x75acd5b1, 0x9fb47516, 0xc33228cc, 0xc5fc1869, 0xadcda25c, 0x8b935918, 0xa7e57f70, 0x27fb4ee9, 
    0x40a57ffb, 0x8e5945ae, 0x5f2dd75c, 0x33531d6d, 0x21ea3297, 0xb1f807b5, 0xcb7e53f8, 0x3f4d00ff, 0x689b9d4a, 0x7b4cf2aa, 0x5ae75456, 0xe662325b, 
    0xffa8dbc0, 0xa59f9600, 0xf95fc937, 0x0b53fae9, 0x9aa2de93, 0xe685b774, 0x8ac6f695, 0xaea08f59, 0x659d1ac5, 0x2ea51af9, 0x7f258fe5, 0x4ee9a7e7, 
    0xe57fea16, 0xb659e9a7, 0xa296b53a, 0xf993b6ce, 0x6d580685, 0xab981f23, 0x49efc8a1, 0xb27b6dc6, 0x77a981fc, 0xfd2ffb26, 0xc229fd34, 0xcfe20fd4, 
    0x638e4ce1, 0x6e2bacde, 0x7552c915, 0x9332bba3, 0xabd01a9a, 0x1fd46654, 0xc52afde2, 0xac85f244, 0xb4abd4a9, 0xe8723159, 0xfa4ff647, 0xf651fa69, 
    0x3d00ff61, 0x4e634a3f, 0x7b864a4f, 0xe685b7a5, 0x8ac6f695, 0xaea08f59, 0xd6f61ac5, 0xb94fa946, 0xfec3ec67, 0x7d947e7a, 0x7e9afe93, 0xad639d95, 
    0xeb2cea59, 0x14e67169, 0x8cb46119, 0x2caf627e, 0xaaa175e4, 0xe497bad6, 0x20b94f0d, 0xbff803b5, 0x5561914a, 0x544349a4, 0xa552c7b5, 0xa4cca45d, 
    0x3dced4e5, 0x8361a568, 0x77b25e49, 0x07564847, 0x2d1aaf8b, 0xfb232d3d, 0x778c2df7, 0xeb00ffc0, 0x92ebb7e2, 0xf56e6cd4, 0x029411bf, 0x5d62785b, 
    0xe38c31ab, 0x7aa7e38e, 0xd9f28d8a, 0x01c1100d, 0x2d88eff0, 0xcfe51303, 0x736f5718, 0x6b70fdc1, 0x573faeb3, 0x56f4ddf0, 0x7cfb68bf, 0x1819cfd7, 
    0x0f908442, 0xaef6c971, 0x5b9e4b9b, 0x2609368d, 0xe1764946, 0x918394d5, 0xd3d4e0c1, 0x19586dba, 0x8ad6816a, 0xab351955, 0x211d4276, 0x4a5399e6, 
    0x450d3587, 0x26749c36, 0x0fac402f, 0x5a345e17, 0xf7475a7a, 0xef185bee, 0xd700ff81, 0x25d76fc5, 0xebddd8a8, 0x0428237e, 0xbbc4f0b6, 0xc7196356, 
    0xf44ec71d, 0xc8966f54, 0x070443b4, 0xb420bec3, 0x3c974f0c, 0xcfbd5d61, 0xaec1f507, 0x5ffdb8ce, 0x5ad177c3, 0xf3eda3fd, 0x61643c5f, 0x3d40120a, 
    0xbada27c7, 0x6e792e6d, 0x9924d834, 0x87db2519, 0x470e5256, 0x4e538307, 0x6560b5e9, 0x9a677caa, 0x1c124d65, 0x476954d5, 0x1ff42452, 0xf5583531, 
    0x6aa692ab, 0xb10629ab, 0xd872c9c3, 0xb5613598, 0xc6ebe281, 0x484b4f8b, 0x63cbfdfe, 0xfa3ff01d, 0xe4faadf8, 0xbd1b1bb5, 0x0065c46f, 0x9718de96, 
    0x3863cc6a, 0xdee9b8e3, 0x2ddfa8b6, 0x8221aa91, 0x10dfe103, 0xcb27065a, 0xdeae309e, 0xe0fa83e7, 0x7e5c67d7, 0xe8bbe1af, 0xf6d17ead, 0x329eaff9, 
    0x20098530, 0xed93e31e, 0x3c97365d, 0x126c1ab7, 0xed928c4c, 0x0729abc3, 0xa9c18323, 0xb0da74a7, 0x56ebd032, 0x55a02661, 0x93a8d524, 0x4b229e15, 
    0x6a53c696, 0x55e0dcc3, 0xae260939, 0xd2aac8b0, 0x0d9f6b46, 0x14a9a728, 0x0fac20da, 0x5a345e17, 0xf7475a7a, 0xef185bee, 0xd700ff81, 0x110474c5, 
    0x63a38e5c, 0x8cf8ad77, 0xc3db12a0, 0x8c59ed12, 0x1d771c67, 0x4775d73b, 0x3a75d9ee, 0x1f100c11, 0xd082f80e, 0xf15c3e31, 0x3cf77685, 0xbb06d71f, 
    0x7ff5e33a, 0x6b45df0d, 0xcdb78ff6, 0x8491f17c, 0xf7004928, 0xe96a9f1c, 0xb9a5b9b4, 0x6892a0d3, 0x872b25d9, 0x470e5256, 0x4f538307, 0x013d6ddd, 
    0x1a246397, 0xab2287b8, 0x2aab19c7, 0x138f2b30, 0xcfa1b728, 0x8727a251, 0x45440335, 0x86b3a25d, 0x4d255022, 0x1da93ca3, 0xb0402ba9, 0x55a6e91d, 
    0x8a85aef5, 0x9e26fb93, 0xd228f6d1, 0x5db52785, 0x38adaf12, 0x78e83d30, 0x1eb22fa9, 0xac29f6d1, 0x0ee9d444, 0x9e2b6a2a, 0xcd4c8978, 0x81acb1cd, 
    0xd28317aa, 0xc25951ac, 0x94b0a1ac, 0x5818299a, 0xa5377653, 0xf5c8a45f, 0x978b95ae, 0x28a3fd62, 0x704a6fec, 0xc85c8d85, 0xe2a1a5f5, 0xa33dd8e5, 
    0xb51ea42b, 0x755aa032, 0xb23acd15, 0x3639e49e, 0x3588c614, 0x26958a3e, 0x2b7612b6, 0x30a5bf35, 0x8a72d5c0, 0x8b6662dd, 0x404c1955, 0xf5b7f5d4, 
    0xf15034ab, 0x31ea6033, 0x7d5a108b, 0x4d0e5614, 0x0a7743ee, 0xa2a50142, 0x0a6d44a4, 0x15b7619a, 0xabaa153d, 0x4929b335, 0x296e01a2, 0x928a0aeb, 
    0xbd591d8a, 0x203639d8, 0x45d15200, 0x52544864, 0xd5948a49, 0xd4d69479, 0x2ab59ca2, 0xd2423629, 0x5b3b6b12, 0x4a3f6dbf, 0xb26d759a, 0xfd676c63, 
    0x6afcd3a3, 0x1fb9e2f1, 0xdeab5e18, 0xec1cd95b, 0x1e93e78f, 0x6ffd00ff, 0xad9c5ee7, 0x2475e808, 0xae3dfc89, 0x3f6adc5f, 0x2d24d465, 0x3cbac7e6, 
    0xc37e5fa8, 0xd555feb8, 0xf31a842c, 0xb65847fd, 0xbdb5539b, 0x9a588eb2, 0x00060300, 0x43c70364, 0xdc5d91e9, 0xa4f02c32, 0xa8ab9cb1, 0xa9a14760, 
    0x7a9aa485, 0xe4e84583, 0xe1554bcd, 0x79055653, 0xb49462d5, 0x61926439, 0x9602c748, 0xb322e398, 0x48d4bb8a, 0xe9884986, 0xacceda59, 0x692add74, 
    0xdd671ec0, 0xf4dc938c, 0xb91a00ff, 0x8e6b9022, 0xbdea85f1, 0xcf99bddd, 0x26cf1f97, 0xfa00ff3d, 0x25bdcedf, 0x47174618, 0xfe8b624c, 0xd52df11e, 
    0x4df6a1ce, 0x63f30141, 0xa7fb19dd, 0xca1ff5d7, 0x9a9b84ba, 0x5643adf3, 0x2dd5b8b7, 0x96222c2f, 0x43008436, 0xe3013200, 0x15993e18, 0xc47c40df, 
    0xbaee1749, 0x53431f86, 0x981607cb, 0xf26215e5, 0x625a8a9c, 0x797d0a0c, 0x1c7a56f2, 0x83a228ac, 0x208f2152, 0xae7dad5a, 0x4c5a6959, 0x895bdcb3, 
    0xf984dd64, 0xa1d3eb88, 0xa192d4fa, 0xb478c735, 0x16c43dcd, 0x31b413cb, 0x1d15f29d, 0x5b00ffcf, 0x3875d7f9, 0x4dfd4241, 0x261dc5e9, 0x3fbde283, 
    0x7716be51, 0x91166676, 0x88db4637, 0x95bebe3e, 0x8c4459a8, 0x200172aa, 0xe9f35a1f, 0xb3963a65, 0xdaa67569, 0x00421bcf, 0x0007ca60, 0x64fad0f1, 
    0xedd67e57, 0x754f810c, 0xd28cf90d, 0xc7ca219c, 0x1fd72a52, 0xe28a0245, 0x288ac239, 0x0a2404a0, 0x68a96fbb, 0x35937e7a, 0x9100a9ce, 0x644298c4, 
    0x8e03e365, 0x2eaaf708, 0xfce78aa4, 0xea5fa243, 0xf1b644f1, 0xe61343bb, 0xffcfbd4d, 0xd7f9ab00, 0xabc52845, 0x37695ab3, 0xda68daa9, 0xc97585d7, 
    0xc9d9aacd, 0x6528236f, 0x0767633e, 0x15494690, 0x2b16e146, 0xdc566299, 0xf586ca11, 0xde5cebc1, 0xaacfa691, 0xd8f3d66b, 0xe5455cdb, 0x81910d20, 
    0xb7a7d381, 0x566ad515, 0xeff7d93f, 0x0040926f, 0x6664d4f3, 0x5a8a918a, 0x5d2b5115, 0x8aa26815, 0x420a30e7, 0x4dd1d2a0, 0xc003013b, 0xe145660d, 
    0x57b578eb, 0x24c98bd4, 0x46bc9fcf, 0xf4387d06, 0x652baaad, 0x36b5685e, 0x26fdcb8c, 0xdbd44a1d, 0x2069a7ce, 0x8a200f82, 0x4b4ec79a, 0x46b7541b, 
    0x05185466, 0xd1aee2ba, 0x40ceab44, 0x502036ea, 0xb1a26829, 0x1405e46e, 0xb4064851, 0xbeadaa61, 0xeb056797, 0x6c1e345e, 0xc7dc49cc, 0x94ab18a7, 
    0x5b92aa55, 0xd89a9432, 0xaeed984b, 0xa43d31a0, 0xf520c878, 0x81e1a8c1, 0x24208c61, 0xd152eb81, 0x2d25554e, 0xb83739d8, 0x99154551, 0x50144521, 
    0x67354000, 0xc082b94f, 0x298a7db3, 0xf7769b9d, 0xfe1847ce, 0x6a8a5a95, 0xda346c4d, 0x5e93d6d8, 0xd3d51edf, 0x04f594ed, 0x2d6b7082, 0xee72a4dd, 
    0x921c3377, 0x7253b469, 0x4d6edc72, 0x455114ee, 0x14452149, 0xfa5a0150, 0xd2248847, 0xb514de2d, 0xbfef5d86, 0x8023e773, 0x5164a53f, 0x1ab6364d, 
    0x958e6a6d, 0x50786ffc, 0xa3edec88, 0x7182cd6e, 0x3c3dd7fa, 0xf3cedcf2, 0x73e4e5cc, 0xd4dc6396, 0x6f725374, 0xb837b971, 0x24154551, 0x40511485, 
    0xd0b05f05, 0x16521df5, 0xcddbce96, 0xefa46d45, 0x4ffcc155, 0x3fab50bd, 0xe85d94f0, 0xa4a51b1a, 0x394bf26c, 0x07bddec6, 0xae5a00ff, 0xc265e611, 
    0xbdccce3c, 0xb8b5852f, 0x3658bb50, 0xacae5d07, 0x206b207f, 0xc1080e82, 0x8de1bb15, 0xf3825a7f, 0x6b96beca, 0xa81491a5, 0x09bec76e, 0x41adacf7, 
    0xe1467da4, 0xefc288e5, 0x8f8c312b, 0x559c93bb, 0x9ca31c4e, 0x7a855614, 0x33b2a228, 0x80a2280a, 0xa161bf0a, 0x2da43aea, 0x9ab79d2d, 0xde49db8a, 
    0x9ff883ab, 0x575da17a, 0xd337f586, 0x8e860b74, 0x739a9516, 0x91fda380, 0xd8dd0855, 0xd94945b8, 0x5bf89299, 0xb50b855b, 0xda756083, 0x06f2c7ea, 
    0xe02008b2, 0x855e118c, 0x4835c9e1, 0xcfdf98a3, 0x0565d125, 0xdc831c13, 0xa80dd764, 0x72a50e99, 0x7661c4d2, 0x7fc69895, 0xaa382787, 0x3847799c, 
    0xa895ab28, 0xcccc8aa2, 0x008aa228, 0x8786fd2a, 0xb490eaa8, 0x6ade76b6, 0x7a276d2b, 0x7de20fae, 0x5e7785ea, 0x5b742507, 0xce2701a2, 0xa2fbc738, 
    0xc5ee2aaa, 0xcc4e2ac2, 0xb6f025e7, 0x6b170ab7, 0xb5ebc006, 0x0de48fd5, 0xc1411064, 0x3bbd2218, 0xecd4b447, 0xafef9961, 0x0a90f41a, 0xee31ea20, 
    0xb5ce6b7d, 0xaed42113, 0x2e8c585a, 0xcf18b3d2, 0x15e7e4f0, 0xe7908d52, 0xb5721505, 0x99595114, 0x40511485, 0xd0b05f05, 0x16521df5, 0xcddbce96, 
    0xefa46d45, 0x4ffcc155, 0xf3ae50bd, 0x1e9d80c0, 0xe78303e4, 0x22df7f9c, 0xc5ee2aaa, 0xcc4e2ac2, 0xb6f0a5e6, 0x6b170ab7, 0xb5ebc006, 0x0de48fd5, 
    0xc1411064, 0x3fbd2218, 0xf4d4b349, 0xeb6ba7f8, 0x05487ad3, 0xf7187510, 0x3ae7b53e, 0x57ea9089, 0x17462c2d, 0x678c5969, 0x8a7372f8, 0xe7908d72, 
    0xb5721505, 0x66505114, 0x00455114, 0x5ef8d415, 0xd2bc4bc3, 0x12cf17e6, 0xdeed98b8, 0x8f3610e0, 0xaaa8e55a, 0xca5db98c, 0xce5db98c, 0xeef045cf, 
    0x96fbe191, 0x8e599bbc, 0x1600297c, 0xe6d40f00, 0xfa7addb9, 0x9e58472d, 0x6c4400e6, 0x648c5c40, 0x75d68c01, 0xab2be514, 0xb0ba530e, 0x04154551, 
    0x40511405, 0x0bbe7605, 0x8b74d596, 0x8926ee88, 0x1d704c0b, 0x7a743f80, 0x544515d7, 0x8cca9d5d, 0xefce5db9, 0xeff4d246, 0x4ff7ce0f, 0x2820c5ad, 
    0x8f42d046, 0xca3527cb, 0x6af1f7eb, 0x50f7cc5a, 0x02622382, 0x03483092, 0x29eaac19, 0x72585db9, 0x5190d59d, 0x05041545, 0x05405114, 0xb809fe76, 
    0xb9488bb7, 0x8c23ae49, 0xdc01c7b4, 0x7145f703, 0xd9455514, 0x95cba8dc, 0x6d34efdc, 0x27d04f33, 0xb835a97b, 0xda4805e5, 0x8cf85108, 0xaf29d79c, 
    0x6ba9c5df, 0x0842dd33, 0x480a888d, 0x660c20c1, 0xcaa1a8b3, 0x576ec8ea, 0xa2284856, 0x8a02928a, 0xbb02a028, 0x5bdc047f, 0xa45ca4c5, 0x5ac611d7, 
    0x01ee8063, 0x8ab8a2fb, 0xeeeca22a, 0xeeca6554, 0x99369a77, 0xbd13e8a7, 0x72dc9ad4, 0x046da482, 0x4e46fc28, 0xefd7946b, 0x99b5d4e2, 0x4604a1ee, 
    0x602405c4, 0x59330690, 0x75e550d4, 0xab2b3764, 0x45511424, 0xd9ff0749, 
};
};
} // namespace BluePrint
