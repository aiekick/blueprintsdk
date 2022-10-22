#include <UI.h>
#include <imgui_json.h>
#include <imgui_extra_widget.h>
#include <ImVulkanShader.h>
#include <Histogram_vulkan.h>
//#include <ColorBalance_vulkan.h>

namespace BluePrint
{
struct ColorCurveNode final : Node
{
    BP_NODE_WITH_NAME(ColorCurveNode, "Color Curve", VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Default, "Filter#Video#Color")
    ColorCurveNode(BP& blueprint): Node(blueprint)
    {
        m_Name = "Color Curve";
        mKeyPoints.SetRangeX(0.f, 1.f, true);
        mMat_curve.create_type(1024, 1, 4, IM_DT_FLOAT32);
        ResetCurve();
    }

    ~ColorCurveNode()
    {
        if (m_histogram) { delete m_histogram; m_histogram = nullptr; }
    }

    void ResetCurve()
    {
        mKeyPoints.Clear();
        auto curve_index_y = mKeyPoints.AddCurve("Y", ImGui::ImCurveEdit::Smooth, IM_COL32(255, 255, 255, 255), true, 0.f, 1.f, 0.f);
        mKeyPoints.AddPoint(curve_index_y, ImVec2(0.f, 0.f), ImGui::ImCurveEdit::Linear);
        mKeyPoints.AddPoint(curve_index_y, ImVec2(1.f, 1.f), ImGui::ImCurveEdit::Linear);
        auto curve_index_r = mKeyPoints.AddCurve("R", ImGui::ImCurveEdit::Smooth, IM_COL32(255, 0, 0, 255), true, 0.f, 1.f, 0.f);
        mKeyPoints.AddPoint(curve_index_r, ImVec2(0.f, 0.f), ImGui::ImCurveEdit::Linear);
        mKeyPoints.AddPoint(curve_index_r, ImVec2(1.f, 1.f), ImGui::ImCurveEdit::Linear);
        auto curve_index_g = mKeyPoints.AddCurve("G", ImGui::ImCurveEdit::Smooth, IM_COL32(0, 255, 0, 255), true, 0.f, 1.f, 0.f);
        mKeyPoints.AddPoint(curve_index_g, ImVec2(0.f, 0.f), ImGui::ImCurveEdit::Linear);
        mKeyPoints.AddPoint(curve_index_g, ImVec2(1.f, 1.f), ImGui::ImCurveEdit::Linear);
        auto curve_index_b = mKeyPoints.AddCurve("B", ImGui::ImCurveEdit::Smooth, IM_COL32(0, 0, 255, 255), true, 0.f, 1.f, 0.f);
        mKeyPoints.AddPoint(curve_index_b, ImVec2(0.f, 0.f), ImGui::ImCurveEdit::Linear);
        mKeyPoints.AddPoint(curve_index_b, ImVec2(1.f, 1.f), ImGui::ImCurveEdit::Linear);
        SetCurveMat();
    }

    void ResetCurve(int index)
    {
        // clean points
        mKeyPoints.ClearPoint(index);
        // add begin/end point
        mKeyPoints.AddPoint(index, ImVec2(0.f, 0.f), ImGui::ImCurveEdit::Linear);
        mKeyPoints.AddPoint(index, ImVec2(1.f, 1.f), ImGui::ImCurveEdit::Linear);
        SetCurveMat();
    }

    void SetCurveMat()
    {
        if (mKeyPoints.GetCurveCount() == 4 && !mMat_curve.empty())
        {
            auto curve_index_y = mKeyPoints.GetCurveIndex("Y");
            auto curve_index_r = mKeyPoints.GetCurveIndex("R");
            auto curve_index_g = mKeyPoints.GetCurveIndex("G");
            auto curve_index_b = mKeyPoints.GetCurveIndex("B");
            mKeyPoints.SetCurveVisible(curve_index_y, mEditIndex == 0);
            mKeyPoints.SetCurveVisible(curve_index_r, mEditIndex == 1); 
            mKeyPoints.SetCurveVisible(curve_index_g, mEditIndex == 2); 
            mKeyPoints.SetCurveVisible(curve_index_b, mEditIndex == 3); 
            for (int i = 0; i < 1024; i++)
            {
                auto y_mat = mMat_curve.channel(curve_index_y);
                auto r_mat = mMat_curve.channel(curve_index_r);
                auto g_mat = mMat_curve.channel(curve_index_g);
                auto b_mat = mMat_curve.channel(curve_index_b);
                y_mat.at<float>(i, 0) = mKeyPoints.GetValue(curve_index_y, i / 1024.f);
                r_mat.at<float>(i, 0) = mKeyPoints.GetValue(curve_index_r, i / 1024.f);
                g_mat.at<float>(i, 0) = mKeyPoints.GetValue(curve_index_g, i / 1024.f);
                b_mat.at<float>(i, 0) = mKeyPoints.GetValue(curve_index_b, i / 1024.f);
            }
        }
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
        auto mat_in = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        if (!mat_in.empty())
        {
            int gpu = mat_in.device == IM_DD_VULKAN ? mat_in.device_number : ImGui::get_default_gpu_index();
            if (!m_Enabled)
            {
                m_MatOut.SetValue(mat_in);
                return m_Exit;
            }
            if (!m_histogram || gpu != m_device)
            {
                if (m_histogram) { delete m_histogram; m_histogram = nullptr; }
                m_histogram = new ImGui::Histogram_vulkan(gpu);
            }
            if (!m_histogram)
            {
                return {};
            }
            // TODO::Dicky filter init
            m_device = gpu;
            m_histogram->scope(mat_in, mMat_histogram, 256, mHistogramScale, mHistogramLog);

            m_MatOut.SetValue(mat_in); // for test
        }

        return m_Exit;
    }

    bool CustomLayout() const override { return true; }
    bool Skippable() const override { return true; }

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

    bool DrawCustomLayout(ImGuiContext * ctx, float zoom, ImVec2 origin, ImGui::ImCurveEdit::keys * key) override
    {
        ImGui::SetCurrentContext(ctx);
        bool changed = false;
        bool need_update_scope = false;
        ImGuiIO &io = ImGui::GetIO();
        // draw histogram and curve
        ImGui::BeginGroup();
        auto pos = ImGui::GetCursorScreenPos();
            
        if (!mMat_histogram.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
            ImGui::SetCursorScreenPos(pos);
            float height_scale = 1.f;
            float height_offset = 0;
            auto rmat = mMat_histogram.channel(0);
            auto gmat = mMat_histogram.channel(1);
            auto bmat = mMat_histogram.channel(2);
            auto ymat = mMat_histogram.channel(3);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 0.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 0.f, 0.f, 0.3f));
            ImGui::PlotLinesEx("##rh", &((float *)rmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 1);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 1.f, 0.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 1.f, 0.f, 0.3f));
            ImGui::PlotLinesEx("##gh", &((float *)gmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 2);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 2));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.f, 0.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.f, 0.f, 1.f, 0.3f));
            ImGui::PlotLinesEx("##bh", &((float *)bmat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 3);
            ImGui::PopStyleColor(2);
            ImGui::SetCursorScreenPos(pos + ImVec2(0, height_offset * 3));
            ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.f, 1.f, 1.f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.f, 1.f, 1.f, 0.3f));
            ImGui::PlotLinesEx("##yh", &((float *)ymat.data)[1], mMat_histogram.w - 1, 0, nullptr, 0, mHistogramLog ? 10 : 1000, ImVec2(scope_view_size.x, scope_view_size.y / height_scale), 4, false, mEditIndex == 0);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleColor();
        }
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImRect scrop_rect = ImRect(pos, pos + scope_view_size);
        draw_list->AddRect(scrop_rect.Min, scrop_rect.Max, IM_COL32(112, 112, 112, 255), 0);
        // draw graticule line
        draw_list->PushClipRect(scrop_rect.Min, scrop_rect.Max);
        float graticule_scale = 1.f;
        auto histogram_step = scope_view_size.x / 10;
        auto histogram_sub_vstep = scope_view_size.x / 50;
        auto histogram_vstep = scope_view_size.y * mHistogramScale * 10 / graticule_scale;
        auto histogram_seg = scope_view_size.y / histogram_vstep / graticule_scale;
        for (int i = 1; i <= 10; i++)
        {
            ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_step, 0);
            ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_step, scrop_rect.Max.y);
            draw_list->AddLine(p0, p1, IM_COL32(128,  96,   0, 128), 1);
        }
        for (int i = 0; i < histogram_seg; i++)
        {
            ImVec2 pr0 = scrop_rect.Min + ImVec2(0, (scope_view_size.y / graticule_scale) - i * histogram_vstep);
            ImVec2 pr1 = scrop_rect.Min + ImVec2(scrop_rect.Max.x, (scope_view_size.y / graticule_scale) - i * histogram_vstep);
            draw_list->AddLine(pr0, pr1, IM_COL32(128,  96,   0, 128), 1);
        }
        for (int i = 0; i < 50; i++)
        {
            ImVec2 p0 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 0);
            ImVec2 p1 = scrop_rect.Min + ImVec2(i * histogram_sub_vstep, 5);
            draw_list->AddLine(p0, p1, IM_COL32(255, 196,   0, 128), 1);
        }
        // draw curve
        bool _changed = false;
        float curses_pos = -1.f;
        ImGui::SetCursorScreenPos(pos);
        ImGui::ImCurveEdit::Edit(mKeyPoints,
                                scope_view_size, 
                                ImGui::GetID("##color_curve_keypoint_editor"), 
                                curses_pos,
                                CURVE_EDIT_FLAG_VALUE_LIMITED | CURVE_EDIT_FLAG_MOVE_CURVE | CURVE_EDIT_FLAG_KEEP_BEGIN_END | CURVE_EDIT_FLAG_DOCK_BEGIN_END, 
                                nullptr, // clippingRect
                                &_changed
                                );
        if (ImGui::IsItemHovered())
        {
            if (io.MouseWheel < -FLT_EPSILON)
            {
                mHistogramScale *= 0.9f;
                if (mHistogramScale < 0.002)
                    mHistogramScale = 0.002;
                need_update_scope = true;
                ImGui::BeginTooltip();
                ImGui::Text("Scale:%f", mHistogramScale);
                ImGui::EndTooltip();
            }
            else if (io.MouseWheel > FLT_EPSILON)
            {
                mHistogramScale *= 1.1f;
                if (mHistogramScale > 4.0f)
                    mHistogramScale = 4.0;
                need_update_scope = true;
                ImGui::BeginTooltip();
                ImGui::Text("Scale:%f", mHistogramScale);
                ImGui::EndTooltip();
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                mHistogramScale = mHistogramLog ? 0.1 : 0.005f;
                need_update_scope = true;
            }
        }
        if (need_update_scope || _changed)
        {
            changed = true;
        }
        draw_list->PopClipRect();
        // draw control bar
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.75, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x, 0));
        bool edit_y = mEditIndex == 0;
        if (ImGui::CheckButton(u8"\uff39", &edit_y))
        {
            if (edit_y) 
            {
                mEditIndex = 0;
                mKeyPoints.SetCurveVisible(0, true);
                mKeyPoints.SetCurveVisible(1, false); 
                mKeyPoints.SetCurveVisible(2, false); 
                mKeyPoints.SetCurveVisible(3, false); 
            }
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 24, 0));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_Y"))
        {
            ResetCurve(0);
        }
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0, 0, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.0, 0.0, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x, 24));
        bool edit_r = mEditIndex == 1;
        if (ImGui::CheckButton(u8"\uff32", &edit_r))
        {
            if (edit_r)
            {
                mEditIndex = 1; 
                mKeyPoints.SetCurveVisible(0, false);
                mKeyPoints.SetCurveVisible(1, true); 
                mKeyPoints.SetCurveVisible(2, false); 
                mKeyPoints.SetCurveVisible(3, false); 
            }
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 24, 24));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_R"))
        {
            ResetCurve(1);
        }
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0.5, 0, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0, 0.75, 0.0, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x, 48));
        bool edit_g = mEditIndex == 2;
        if (ImGui::CheckButton(u8"\uff27", &edit_g))
        {
            if (edit_g)
            {
                mEditIndex = 2; 
                mKeyPoints.SetCurveVisible(0, false);
                mKeyPoints.SetCurveVisible(1, false); 
                mKeyPoints.SetCurveVisible(2, true); 
                mKeyPoints.SetCurveVisible(3, false); 
            }
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 24, 48));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_G"))
        {
            ResetCurve(2);
        }
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0.5, 0.75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0, 0.0, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x, 72));
        bool edit_b = mEditIndex == 3;
        if (ImGui::CheckButton(u8"\uff22", &edit_b))
        {
            if (edit_b)
            {
                mEditIndex = 3;
                mKeyPoints.SetCurveVisible(0, false);
                mKeyPoints.SetCurveVisible(1, false); 
                mKeyPoints.SetCurveVisible(2, false); 
                mKeyPoints.SetCurveVisible(3, true); 
            }
        }
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 24, 72));
        if (ImGui::Button(ICON_RESET "##color_curve_reset_B"))
        {
            ResetCurve(3);
        }
        ImGui::PopStyleColor(2);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5, 0.5, 0.5, 0.5));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75, 0.75, 0.75, 0.75));
        ImGui::SetCursorScreenPos(pos + ImVec2(scope_view_size.x + 24, 96));
        if (ImGui::Button(ICON_RESET "##color_curve_reset"))
        {
            ResetCurve();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // check curve point type
        for (int i = 0; i < mKeyPoints.GetCurveCount(); i++)
        {
            auto pCount = mKeyPoints.GetCurvePointCount(i);
            if (pCount > 2)
            {
                // need change end point type to smooth
                auto point = mKeyPoints.GetPoint(i, pCount - 1);
                point.type = ImGui::ImCurveEdit::Smooth;
                mKeyPoints.EditPoint(i, pCount - 1, point.point, point.type);
            }
            else
            {
                // change end point type to linear
                auto point = mKeyPoints.GetPoint(i, pCount - 1);
                point.type = ImGui::ImCurveEdit::Linear;
                mKeyPoints.EditPoint(i, pCount - 1, point.point, point.type);
            }
        }
        if (_changed)
        {
            SetCurveMat();
        }
        ImGui::EndGroup();
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
        if (value.contains("histogram_scale"))
        {
            auto& val = value["histogram_scale"];
            if (val.is_number()) 
                mHistogramScale = val.get<imgui_json::number>();
        }
        if (value.contains("histogram_index"))
        {
            auto& val = value["histogram_index"];
            if (val.is_number()) 
                mEditIndex = val.get<imgui_json::number>();
        }
        if (value.contains("histogram_log"))
        {
            auto& val = value["histogram_log"];
            if (val.is_boolean()) 
                mHistogramLog = val.get<imgui_json::boolean>();
        }
        // load curve
        if (value.contains("Curve"))
        {
            auto& keypoint = value["Curve"];
            mKeyPoints.Load(keypoint);
            SetCurveMat();
        }
        return ret;
    }

    void Save(imgui_json::value& value, std::map<ID_TYPE, ID_TYPE> MapID) override
    {
        Node::Save(value, MapID);
        value["mat_type"] = imgui_json::number(m_mat_data_type);
        value["histogram_scale"] = imgui_json::number(mHistogramScale);
        value["histogram_index"] = imgui_json::number(mEditIndex);
        value["histogram_log"] = imgui_json::boolean(mHistogramLog);

        imgui_json::value keypoint;
        mKeyPoints.Save(keypoint);
        value["Curve"] = keypoint;
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }
    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkInputFlowPin() override { return &m_Enter; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkInputDataPin() override { return {&m_MatIn}; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOut}; }

    FlowPin   m_Enter   = { this, "Enter" };
    FlowPin   m_Exit    = { this, "Exit" };
    MatPin    m_MatIn   = { this, "In" };
    MatPin    m_MatOut  = { this, "Out" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };
    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };

private:
    const ImVec2 scope_view_size = ImVec2(256, 256);
    ImDataType m_mat_data_type {IM_DT_UNDEFINED};
    int m_device                {-1};
    ImGui::Histogram_vulkan *   m_histogram {nullptr};
    ImGui::ImMat                mMat_histogram;
    ImGui::ImMat                mMat_curve;
    float                       mHistogramScale {0.005};
    bool                        mHistogramLog {false};
    int                         mEditIndex {0};
    ImGui::KeyPointEditor       mKeyPoints;
};
} // namespace BluePrint

