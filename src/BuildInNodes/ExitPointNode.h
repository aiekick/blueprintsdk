#pragma once
#include <imgui.h>
namespace BluePrint
{
struct ExitPointNode final : Node
{
    BP_NODE(ExitPointNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Simple, "System")

    ExitPointNode(BP& blueprint): Node(blueprint) { m_Name = "End"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        context.m_Callstack.clear();
        return {};
    }

    span<Pin*> GetInputPins() override { return m_InputPins; }

    FlowPin m_Enter = { this, "End" };

    Pin* m_InputPins[1] = { &m_Enter };
};
} // namespace BluePrint
