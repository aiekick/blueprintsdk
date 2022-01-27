#pragma once
#include <imgui.h>
namespace BluePrint
{
struct FilterEntryPointNode final : Node
{
    BP_NODE(FilterEntryPointNode, VERSION_BLUEPRINT, NodeType::EntryPoint, NodeStyle::Simple, "System")

    FilterEntryPointNode(BP& blueprint): Node(blueprint) { m_Name = "Start"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    Pin* GetAutoLinkOutputDataPin() override { return &m_MatOut; }
    FlowPin* GetOutputFlowPin() override { return &m_Exit; }

    FlowPin m_Exit = { this, "Start" };
    MatPin  m_MatOut = { this, "Out" };

    Pin* m_OutputPins[2] = { &m_Exit, &m_MatOut };
};
} // namespace BluePrint

