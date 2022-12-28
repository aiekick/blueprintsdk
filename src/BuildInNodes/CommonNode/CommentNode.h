#pragma once
#include <imgui.h>
namespace BluePrint
{
struct CommentNode final : Node
{
    BP_NODE(CommentNode, VERSION_BLUEPRINT, NodeType::Internal, NodeStyle::Comment, "System")
    CommentNode(BP* blueprint): Node(blueprint) { m_Name = "Comment"; }
};
} // namespace BluePrint
