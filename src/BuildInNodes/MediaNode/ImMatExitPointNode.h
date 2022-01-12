#pragma once
#include <imgui.h>
namespace BluePrint
{
struct ExitPointNode final : Node
{
    BP_NODE(ExitPointNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Simple, "System")

    ExitPointNode(BP& blueprint): Node(blueprint) { m_Name = "Output"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        m_Mat = context.GetPinValue<ImGui::ImMat>(m_MatIn);
        context.m_Callstack.clear();
        return {};
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }

    FlowPin m_Enter = { this, "End" };
    MatPin  m_MatIn = { this, "In" };

    Pin* m_InputPins[2] = { &m_Enter, &m_MatIn };

    ImGui::ImMat m_Mat;
};
} // namespace BluePrint
