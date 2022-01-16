#pragma once
#include <imgui.h>
namespace BluePrint
{
struct EntryPointNode final : Node
{
    BP_NODE(EntryPointNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Simple, "System")

    EntryPointNode(BP& blueprint): Node(blueprint) { m_Name = "Input"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkOutputPin() override { return &m_MatOut; }

    FlowPin m_Exit = { this, "Start" };
    MatPin  m_MatOut = { this, "Out" };

    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };
};
} // namespace BluePrint

