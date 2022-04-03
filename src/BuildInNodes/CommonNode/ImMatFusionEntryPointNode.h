#pragma once
#include <imgui.h>
namespace BluePrint
{
struct FusionEntryPointNode final : Node
{
    BP_NODE(FusionEntryPointNode, VERSION_BLUEPRINT, NodeType::EntryPoint, NodeStyle::Simple, "System")

    FusionEntryPointNode(BP& blueprint): Node(blueprint) { m_Name = "Start"; }

    FlowPin Execute(Context& context, FlowPin& entryPoint, bool threading = false) override
    {
        return m_Exit;
    }

    span<Pin*> GetOutputPins() override { return m_OutputPins; }
    Pin* GetAutoLinkOutputFlowPin() override { return &m_Exit; }
    vector<Pin*> GetAutoLinkOutputDataPin() override { return {&m_MatOutFirst, &m_MatOutSecond, &m_FusionDuration, &m_FusionTimeStamp}; }
    FlowPin* GetOutputFlowPin() override { return &m_Exit; }

    FlowPin m_Exit = { this, "Start" };
    MatPin  m_MatOutFirst = { this, "Out First" };
    MatPin  m_MatOutSecond = { this, "Out Second" };
    Int64Pin m_FusionDuration = { this, "Fusion Duration" };
    Int64Pin m_FusionTimeStamp = { this, "Time Stamp" };

    Pin* m_OutputPins[5] = { &m_Exit, &m_MatOutFirst, &m_MatOutSecond, &m_FusionDuration, &m_FusionTimeStamp };
};
} // namespace BluePrint

