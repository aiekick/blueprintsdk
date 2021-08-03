#pragma once
#include <imgui.h>
namespace BluePrint
{
struct EntryPointNode final : Node
{
    BP_NODE(EntryPointNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Simple, "System")

    EntryPointNode(BP& blueprint): Node(blueprint) { m_Name = "Start"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }

    FlowPin m_Exit = { this, "Start" };

    Pin* m_OutputPins[1] = { &m_Exit };
};
} // namespace BluePrint

