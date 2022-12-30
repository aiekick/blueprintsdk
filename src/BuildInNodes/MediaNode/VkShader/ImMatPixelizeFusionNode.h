#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Pixelize_vulkan.h>

namespace BluePrint
{
struct PixelizeFusionNode final : Node
{
    BP_NODE_WITH_NAME(PixelizeFusionNode, "Pixelize Transform", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Fusion#Video#Mix")
    PixelizeFusionNode(BP* blueprint): Node(blueprint) { m_Name = "Pixelize Transform"; }

    ~PixelizeFusionNode()
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
                m_fusion = new ImGui::Pixelize_vulkan(gpu);
            }
            if (!m_fusion)
            {
                return {};
            }
            m_device = gpu;
            ImGui::VkMat im_RGB; im_RGB.type = m_mat_data_type == IM_DT_UNDEFINED ? mat_first.type : m_mat_data_type;
            m_NodeTimeMs = m_fusion->transition(mat_first, mat_second, im_RGB, progress, m_size, m_steps);
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
        float _size = m_size;
        int _steps = m_steps;
        static ImGuiSliderFlags flags = ImGuiSliderFlags_NoInput;
        ImGui::Dummy(ImVec2(200, 8));
        ImGui::PushItemWidth(200);
        ImGui::SliderFloat("Min Size##Pixelize", &_size, 0.0, 50.f, "%.0f", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_size##Pixelize")) { _size = 20; changed = true; }
        ImGui::SliderInt("Steps##Pixelize", &_steps, 1, 100, "%d", flags);
        ImGui::SameLine(320);  if (ImGui::Button(ICON_RESET "##reset_steps##Pixelize")) { _steps = 50; changed = true; }
        ImGui::PopItemWidth();
        if (_size != m_size) { m_size = _size; changed = true; }
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
        if (value.contains("size"))
        {
            auto& val = value["size"];
            if (val.is_number()) 
                m_size = val.get<imgui_json::number>();
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
        value["size"] = imgui_json::number(m_size);
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
        // if show icon then we using u8"\ue3ea"
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
    int m_size          {20};
    int m_steps         {50};
    ImGui::Pixelize_vulkan * m_fusion   {nullptr};
    mutable ImTextureID  m_logo {nullptr};
    mutable int m_logo_index {0};

    const unsigned int logo_size = 3473;
    const unsigned int logo_data[3476/4] =
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
    0x51805414, 0xafd45545, 0x59b54e93, 0xf61912dc, 0x00ff84fe, 0x16b01b4a, 0x0fadaca8, 0x279c3a57, 0x6fb30acf, 0x0606a12a, 0xfab90700, 0xe2e0a9d6, 
    0xa0ab6992, 0x98a2280a, 0x5414450b, 0x59511492, 0x489a207e, 0x91a2112c, 0x192863a3, 0x34f42056, 0xcdb8b29b, 0x460b2b4a, 0x21591ed6, 0xd85991b2, 
    0x26adccee, 0x7e7aea49, 0x324abb95, 0xd5405752, 0xaa288a82, 0xa228ea10, 0x298a02a4, 0x7fa1d6ea, 0xfd887079, 0xa018b8d4, 0xd6585106, 0x7797d41a, 
    0x4edec0b1, 0x39773f1b, 0x5beb13e0, 0xd8939434, 0x4551b01a, 0x8a8ec014, 0x0a8aad28, 0x7c617ce4, 0x220bee1a, 0x3c4ccb5b, 0xffcf7780, 0xd7f95b00, 
    0xe8d0595d, 0x3eab4590, 0x499665a4, 0x30363c9f, 0xa7c7e933, 0x4a4e1315, 0xafe468c8, 0xd4577bf5, 0xf8346fad, 0x02c080a4, 0x400690b2, 0x71fad0e9, 
    0xa432dc5d, 0x9cb1a4f0, 0x7d60a8a3, 0x1d35a841, 0xd04e1d3e, 0x29584edb, 0x41bd8220, 0x1beb941e, 0x8eb3b145, 0x587724d9, 0x677c01c6, 0xc528851f, 
    0x512cb0c5, 0x85a01545, 0x402a8aa2, 0x3ef1972b, 0xa793b2a4, 0x52346479, 0x07cfee2b, 0xec00ffe5, 0x3faaa8ab, 0xa88d16d9, 0x092a7bcb, 0x520a459a, 
    0xa7a30c45, 0x15af6320, 0x20ab4d13, 0xa96f3f47, 0x2be24dd9, 0xa051986b, 0x79ab1085, 0xdd700e84, 0xac2beef3, 0x64115947, 0xc8b00a42, 0x4615f520, 
    0xbab043ef, 0x2081a08c, 0x8ac41cc8, 0xabf473a4, 0xf6b64290, 0x95a4c2f1, 0x7a82028d, 0x82280690, 0x4906f76a, 0x80551445, 0x494551b4, 0xcd9a5821, 
    0xa2e595dd, 0xe2762ec6, 0xc9f2d940, 0xb6c67fec, 0xd04c27eb, 0xa5e5ca56, 0x215e98b9, 0x2339a242, 0x798e1ce0, 0xa8f5d327, 0x6888de95, 0xb17cd1c8, 
    0xfb88c2af, 0xe3c3e693, 0x577bc21f, 0xc103c05c, 0xe755e1cf, 0x9878b5b2, 0x6dc5db5b, 0xc7b2fb29, 0x4cbf0c18, 0xb614b763, 0x116f4990, 0x89692749, 
    0xde6eee6c, 0x5c8468dd, 0x26771b74, 0xc92a8aa2, 0x5414451d, 0xaa3b5780, 0xcaa8ebca, 0x760c6003, 0x54d115f6, 0x236f6d4f, 0x66237816, 0x26204b3d, 
    0x34b449a6, 0xb26862ec, 0xd06af3e8, 0x2a6b6dc7, 0x666d774a, 0x39e50327, 0xbada23fe, 0xa011a169, 0xaab80e20, 0xd2565657, 0xdb593a5b, 0xc63965c5, 
    0x1940e2d9, 0x6004e739, 0xf73a32f2, 0x1736f6aa, 0xb817b376, 0x52bae6d4, 0x382109b8, 0x499e2307, 0x942600ff, 0xec808e53, 0x2a8aa25f, 0xbd6737c4, 
    0xf4e93d1b, 0xecc2cc51, 0x367acf66, 0x1bead37b, 0x8cb6848b, 0x9c20863b, 0x0b3b4771, 0xe83dfbb1, 0xe155efd9, 0x559e21d4, 0x6fc1558d, 0xadc63f50, 
    0x606f53d2, 0x7acf66d4, 0xe8d37b36, 0xecc2cca7, 0x367acf6e, 0xa9e8d37b, 0x19bb62e6, 0x9e8ddeb3, 0x2009f0f4, 0x017a9201, 0x7f9e7d52, 0xfd27e3f9, 
    0x61e668f2, 0xdeb34176, 0x3ff59e8d, 0x00ffe7d9, 0xdf7f329e, 0x48d19a26, 0x281b2f83, 0x731423f5, 0xb6c82e48, 0xdeb3d17b, 0xcc1c459f, 0xefd9c42e, 
    0x757acf46, 0x57cc3c15, 0xd17b7663, 0x519ddeb3, 0xae58dbbc, 0x93dee3dc, 0x6357439b, 0xb3d17bb6, 0x6e939ade, 0x9f71bb60, 0x3685927a, 0xd90dddc1, 
    0x7acf46ef, 0x98f91475, 0xefd9c4ae, 0x757acf46, 0x57cc3c15, 0xd17b7663, 0x459ddeb3, 0xc62ecc1c, 0x67a3f7ec, 0x00ff69bd, 0x00ffdc62, 0xffcc2f7e, 
    0xd81f8500, 0x8bdf3fb7, 0x4ee13ff3, 0x99991df2, 0x9e8ddeb3, 0x7329eaf4, 0xa28c5d31, 0x82422b8a, 0x25c6dfaa, 0xfb895581, 0x6ae8e3d9, 0x3aaa38c9, 
    0x9dba84ad, 0x220bc3aa, 0x5c62b8b2, 0x3b823190, 0x604b697d, 0xe8767644, 0xb6c04897, 0x2bfd7a46, 0xeff4b04e, 0xdfd85d0f, 0x902c7347, 0x2be44c94, 
    0xf408f21c, 0x98caadf7, 0x51d8b06d, 0x16621545, 0x10a9288a, 0xd68520a9, 0x1daa0456, 0xab781dc7, 0x5991f05f, 0x39cf00ff, 0xfe91efbf, 0xf56f4635, 
    0x7fba7f12, 0xca546595, 0x03696c4d, 0x82568bab, 0x48aac358, 0xfc03f501, 0xb8db2b6a, 0x062284e7, 0x15793604, 0x91e41581, 0x48551146, 0x5baca21e, 
    0x853c49dd, 0x67002858, 0xf49d4281, 0x68165a1b, 0x21a38aa2, 0x918aa268, 0x4e8b5905, 0xac847886, 0x117a0bce, 0xb75a15fe, 0x6153e534, 0xe7092a19, 
    0x95a6b9b7, 0x64e68ecb, 0x443074ba, 0x57725665, 0xaaf08fd4, 0xaaa8b9d5, 0x5008098b, 0xc5bd1d0f, 0x82d5d061, 0x5114855b, 0xa2688148, 0x1405918a, 
    0x571d4051, 0xa5bf9e98, 0xfe7a621e, 0x30ed4f95, 0xff68cf7f, 0x47a1ef00, 0x00ff60da, 0x00ffd19e, 0xb9b442df, 0x140d86be, 0x0c985951, 0xaba2a4a2, 
    0xc522779c, 0x23a7922a, 0x398a928a, 0xb10be3c2, 0xa4922618, 0xb9708ea2, 0x1425151d, 0x51c68573, 0xce535152, 0x0810912b, 0x9b0619c1, 0x00ffc5e4, 
    0xf9fed33c, 0x73143d15, 0x83980ff9, 0x79fe8bc9, 0x29f2fda7, 0x95433456, 0x54d80745, 0xe4cf51d4, 0x5474c41c, 0x2bce5194, 0xa2a4a28c, 0x472e9ca7, 
    0x1c454945, 0x2a3a72e1, 0x0be7284a, 0x5152d191, 0x655c3847, 0x3c152d15, 0x1515b9e2, 0x8573142d, 0x68a9a8c8, 0x452e9ca3, 0x1c454b45, 0x283a72e1, 
    0x4561a8a2, 0x59075014, 0xf95f3bf6, 0xfdfe87f6, 0x6bc73e8a, 0xd03e00ff, 0xb1df00ff, 0xad37e64b, 0xd67a631e, 0xe4d81bba, 0xc88aa2a8, 0xa2285ac0, 
    0xd65a41a4, 0x88d64851, 0x798e1855, 0x6bb2de03, 0x2da3d1a9, 0x1f4142a5, 0xa100ffc5, 0xba5c691a, 0xb197997b, 0x598eb648, 0x23390e18, 0xa96bb2de, 
    0xa52b63d6, 0xc31f494c, 0x0aa100ff, 0xc16aa8e5, 0x8aa27053, 0x142d1029, 0x6b225251, 0xb87b9658, 0xc9426261, 0xae23a922, 0x9f74c509, 0xffd88bf0, 
    0x8f5bcf00, 0xc27fe9fb, 0x4552bbb9, 0x111c24dc, 0xd7f8f322, 0xffcde749, 0xfe5f3d00, 0xb75135fa, 0xd6245853, 0x0800ffa1, 0xf5fc8fbd, 0xbe00ffb8, 
    0x862afc97, 0x695ba3b3, 0x2f3489d6, 0x1748332b, 0xc118410e, 0xafd59e3e, 0x8de5ed14, 0xb5fb9cd2, 0x6ff26b67, 0x77bf41b1, 0xcf1947ef, 0x62ed5463, 
    0xcf9114a5, 0x99154551, 0xa228ea80, 0xf45741a4, 0xd8ae8575, 0x96af104e, 0x670cbc71, 0x52a8d623, 0xfeb1bf1b, 0xfcfc66f3, 0xcf3fceb8, 0x6617074a, 
    0xd971ea6f, 0x89299fae, 0x0ac60f20, 0xb0a29e81, 0xf6ad0669, 0x1bf9dc9f, 0x9fdf7777, 0x2e5a4a7f, 0xa64dc59e, 0xa2280af4, 0xa7e80882, 0x67a3f76c, 
    0x8acc69bd, 0x3b451bba, 0x3d1bbd67, 0x7441e6e8, 0xb9dafe5e, 0x915fe4fe, 0x3f1a00ff, 0xb97faeb6, 0xc67fe417, 0xa3f7eca8, 0x753ebd67, 0xb8ef7cdc, 
    0x3ddb29da, 0x4befd9e8, 0x4ae80a99, 0xe83dfb29, 0xcc53efd9, 0xb5327485, 0xb1df75ec, 0xd9bfa5d9, 0xcdcfecb7, 0x27c919bf, 0xf6b2ded3, 0xdeb3d17b, 
    0x52439a9a, 0xf6a5c6b6, 0x37cbf6bb, 0xcd36fbb7, 0xe7b7f9f8, 0x7bfa2018, 0x673f4556, 0xe83d1bbd, 0xaf1c4c73, 0xfb29cab8, 0xefd9e83d, 0xe80a994b, 
    0x9efd146d, 0xa9f76cf4, 0x08ba42e6, 0x6e34475c, 0x4e608573, 0xfdabb53e, 0xc9fd6faf, 0x35fe213f, 0x8ddeb395, 0xa273f49e, 0xabb1ed94, 0xfd6faffd, 
    0xfe213fc9, 0xfafa5a35, 0x2242a82b, 0xf3b321b8, 0x9e6ae801, 0x7b367acf, 0xea86ced1, 0x29caa037, 0xd9e83dfb, 0x223247ef, 0xdb294ae8, 0xefd9e83d, 
    0x7485cc53, 0xf2b6a532, 0x85f6c37c, 0x19705c56, 0x6737cd73, 0xe83d1bbd, 0x62b943e6, 0x4d79b16f, 0x7e18c7e4, 0xfec163c5, 0xb39d5655, 0xf49e8dde, 
    0xb872a073, 0x3ddb29da, 0x47efd9e8, 0x94d01532, 0x15004551, 0x9317563c, 0xd28a2446, 0x65f41079, 0x1a7f908c, 0x0f4fbb82, 0xb74453ce, 0x7f378e51, 
    0x2aaa46e8, 0x9947b8ec, 0xb364e4d8, 0xc90b89ba, 0xea282a6d, 0x500184cc, 0xfce7f5d6, 0xaf165724, 0x0984cd15, 0x2b736313, 0xfe788e0c, 0xe6b1de23, 
    0x40b7b8b3, 0x49b5c7f2, 0x340f72c6, 0xdb46099a, 0x1445d110, 0x288a1688, 0x5aab00a9, 0xa7369d76, 0xc8c2d070, 0x97b8a9ac, 0x0e640c24, 0xd6aadec3, 
    0x7f3785df, 0x19b3c969, 0xf75f4ff2, 0x8ed59a96, 0x57c4ee2a, 0xb1bc1cbe, 0x64a97bb3, 0x2163a280, 0x8f93e758, 0xaec97a4f, 0x6f3ec4db, 0xedc61df6, 
    0x677af998, 0x9ab8c2fb, 0x4e655672, 0xa2402f2a, 0x16482a8a, 0x10a9288a, 0xfbb48855, 0x0d0f84db, 0xe8a9f29c, 0xfe2019c9, 0x7dd7ab42, 0x00ff53e1, 
    0xa37f5b12, 0x66e800ff, 0x2ebb31aa, 0x3876e611, 0xf8db34a9, 0x654932a3, 0xb33a8870, 0xc70fc044, 0x0e825815, 0x0abd060f, 0xa3657711, 0x18a3fd4f, 
    0x84e07dba, 0x281fc173, 0x9a7c4fee, 0x3d58efe0, 0xc488c3f5, 0x1062a561, 0x517a5e74, 0x1e27d828, 0xa2281a52, 0xa26881a4, 0x5805918a, 0xbbbcb0b7, 
    0x93d6f642, 0xb4e0a0cc, 0xfe009671, 0xcfbb5e15, 0x7b740202, 0xe7f93a90, 0xaac8f71f, 0x50b1bb8a, 0xc7b1338f, 0xea6300ff, 0xb761fe83, 0xbfe1f77f, 
    0x4190a9c2, 0xbd2218c1, 0xd5b7493f, 0xfb9fe2ac, 0x72ee4846, 0xec448c01, 0xcd477dcc, 0x735ee981, 0x2b3582a8, 0xcc61c29c, 0xb8b27cac, 0x31ce092a, 
    0x2ab2514e, 0x2b57e570, 0x06154551, 0x8aa22863, 0xcbaea0d8, 0x459b8940, 0x778e20b7, 0xb846e87f, 0x62a729da, 0x73572ee3, 0x7d352c7d, 0xaaa38611, 
    0x38ab9bdf, 0xb344d67f, 0xcacf963b, 0xc9036c38, 0xdaaaf007, 0xeaa7bdb6, 0xa505ab96, 0xb8ba91bc, 0x0c145972, 0xfac48e60, 0x5f144dd6, 0x2c2d07a2, 
    0xa4288a82, 0x4551b440, 0x786b0548, 0xabc63d40, 0x4400232a, 0xf2fef304, 0xd6144dd6, 0xce4ee38e, 0x037773e7, 0xa4fa8e69, 0xc6e368f8, 0xfcdc7242, 
    0x7a070ebd, 0x4acdf5e5, 0x27f5521d, 0x63555d85, 0x0e804308, 0xef634f72, 0x7253d459, 0xc29d52b9, 0x20a9288a, 0xa4a2285a, 0x85bf5d41, 0x4b8f60ee, 
    0x89b86485, 0x9ce15608, 0xc435e20f, 0xcc2e4e51, 0xdc95cba8, 0x6148b4ed, 0xeddfacd0, 0xb0bb8d5a, 0xa7d8b690, 0x1d401004, 0xa5965cf1, 0xdda9cd22, 
    0xf32a874a, 0x823b023b, 0x4d51abc6, 0x29c1eaca, 0xa228585d, 0xa245928a, 0x15442a8a, 0xe226f8db, 0xe5222dde, 0x328eb826, 0x70071cd3, 0xc415dd0f, 
    0x67175551, 0x572ea372, 0xb4d1bc73, 0x9d403fcd, 0xe3d6a4ee, 0x68231594, 0x32e24721, 0xbfa65c73, 0xaca5167f, 0x220875cf, 0x23292036, 0x9a318004, 
    0x2b87a2ce, 0x5db921ab, 0x8aa22059, 0xff3f482a, 0x000000d9, 
};
};
} // namespace BluePrint
