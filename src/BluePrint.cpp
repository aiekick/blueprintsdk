#include <BluePrint.h>
#include <Node.h>
#include <imgui_helper.h>
#include <BuildInNodes.h> // Which is generated by cmake
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

namespace BluePrint
{
// -----------------------------
// -------[ IDGenerator ]-------
// -----------------------------
ID_TYPE IDGenerator::GenerateID()
{
    return m_State ++;//= ImGui::get_current_time_long(); // TODO::Dicky
}

void IDGenerator::SetState(ID_TYPE state)
{
    m_State = state;
}

ID_TYPE IDGenerator::State() const
{
    return m_State;
}

// ---------------------------
// ----------[ BP ]-----------
// ---------------------------
# pragma region BP
BP::BP(shared_ptr<NodeRegistry> nodeRegistry, shared_ptr<PinExRegistry> pinexRegistry)
    : m_NodeRegistry(std::move(nodeRegistry))
    , m_PinExRegistry(std::move(pinexRegistry))
{
    if (!m_NodeRegistry)
        m_NodeRegistry = make_shared<NodeRegistry>();
    if (!m_PinExRegistry)
        m_PinExRegistry = make_shared<PinExRegistry>();
}

BP::BP(const BP& other)
    : m_NodeRegistry(other.m_NodeRegistry)
    , m_PinExRegistry(other.m_PinExRegistry)
    , m_Context(other.m_Context)
{
    imgui_json::value value;
    other.Save(value);
    Load(value);
}

BP::BP(BP&& other)
    : m_NodeRegistry(std::move(other.m_NodeRegistry))
    , m_PinExRegistry(std::move(other.m_PinExRegistry))
    , m_Generator(std::move(other.m_Generator))
    , m_Nodes(std::move(other.m_Nodes))
    , m_Pins(std::move(other.m_Pins))
    , m_Context(std::move(other.m_Context))
{
    for (auto& node : m_Nodes)
        node->m_Blueprint = this;
}

BP::~BP()
{
    Clear();
}

BP& BP::operator=(const BP& other)
{
    if (this == &other)
        return *this;

    Clear();

    m_NodeRegistry = other.m_NodeRegistry;
    m_PinExRegistry = other.m_PinExRegistry;
    m_Context = other.m_Context;

    imgui_json::value value;
    other.Save(value);
    Load(value);

    return *this;
}

BP& BP::operator=(BP&& other)
{
    if (this == &other)
        return *this;

    m_NodeRegistry  = std::move(other.m_NodeRegistry);
    m_PinExRegistry = std::move(other.m_PinExRegistry);
    m_Generator     = std::move(other.m_Generator);
    m_Nodes         = std::move(other.m_Nodes);
    m_Pins          = std::move(other.m_Pins);
    m_Context       = std::move(other.m_Context);

    for (auto& node : m_Nodes)
        node->m_Blueprint = this;

    return *this;
}

Node* BP::CreateNode(ID_TYPE nodeTypeId)
{
    if (!m_NodeRegistry)
        return nullptr;

    auto node = m_NodeRegistry->Create(nodeTypeId, *this);
    if (!node)
        return nullptr;

    m_Nodes.emplace_back(node);

    return node;
}

Node* BP::CreateNode(std::string nodeTypeName)
{
    if (!m_NodeRegistry)
        return nullptr;

    auto node = m_NodeRegistry->Create(nodeTypeName, *this);
    if (!node)
        return nullptr;

    m_Nodes.emplace_back(node);

    return node;
}

void BP::DeleteNode(Node* node)
{
    auto nodeIt = std::find(m_Nodes.begin(), m_Nodes.end(), node);
    if (nodeIt == m_Nodes.end())
        return;

    if (node->m_GroupID)
    {
        auto id = node->m_GroupID;
        auto it = std::find_if(m_Nodes.begin(), m_Nodes.end(), [id](const Node* node)
        {
            return node->m_ID == id;
        });
        if (it != m_Nodes.end())
        {
            (*it)->OnNodeDelete(node);
        }
    }

    delete *nodeIt;

    m_Nodes.erase(nodeIt);
}

Node* BP::CloneNode(Node* node)
{
    if (node->GetStyle() == NodeStyle::Dummy)
        return nullptr;

    auto clone_node = CreateNode(node->GetTypeID());
    if (node->GetStyle() == NodeStyle::Comment)
    {
        auto groupSize  = ed::GetGroupSize(node->m_ID);
        ed::SetGroupSize(clone_node->m_ID, groupSize);
    }
    else
    {
        auto nodeSize  = ed::GetNodeSize(node->m_ID);
        ed::SetNodeSize(clone_node->m_ID, nodeSize);
    }
    return clone_node;
}

void BP::InsertNode(Node* node)
{
    if (node)
        m_Nodes.emplace_back(node);
}

void BP::ForgetPin(Pin* pin)
{
    auto pinIt = std::find(m_Pins.begin(), m_Pins.end(), pin);
    if (pinIt == m_Pins.end())
        return;

    m_Pins.erase(pinIt);
}

void BP::Clear()
{
    m_Context.Stop();
    m_IsOpen = false;
    for (auto node : m_Nodes)
    {
        //if (node->GetStyle() != NodeStyle::Group)
        //    node->OnNodeDelete(nullptr);
        node->OnClose(m_Context);
        delete node;
    }
    m_Nodes.resize(0);

    for (auto pin : m_Pins)
    {
        pin->m_Node = nullptr;
    }
    m_Pins.resize(0);
    m_Generator = IDGenerator();
    m_Context = Context();
}

span<Node*> BP::GetNodes()
{
    return m_Nodes;
}

span<const Node* const> BP::GetNodes() const
{
    const Node* const* begin = m_Nodes.data();
    const Node* const* end   = m_Nodes.data() + m_Nodes.size();
    return make_span(begin, end);
}

span<Pin*> BP::GetPins()
{
    return m_Pins;
}

span<const Pin* const> BP::GetPins() const
{
    const Pin* const* begin = m_Pins.data();
    const Pin* const* end   = m_Pins.data() + m_Pins.size();
    return make_span(begin, end);
}

Node* BP::FindNode(ID_TYPE nodeId)
{
    return const_cast<Node*>(const_cast<const BP*>(this)->FindNode(nodeId));
}

const Node* BP::FindNode(ID_TYPE nodeId) const
{
    for (auto& node : m_Nodes)
    {
        if (node->m_ID == nodeId)
            return node;
    }

    return nullptr;
}

Pin* BP::FindPin(ID_TYPE pinId)
{
    return const_cast<Pin*>(const_cast<const BP*>(this)->FindPin(pinId));
}

const Pin* BP::FindPin(ID_TYPE pinId) const
{
    for (auto& pin : m_Pins)
    {
        if (pin->m_ID == pinId)
            return pin;
    }

    return nullptr;
}

shared_ptr<NodeRegistry> BP::GetNodeRegistry() const
{
    return m_NodeRegistry;
}

shared_ptr<PinExRegistry> BP::GetPinExRegistry() const
{
    return m_PinExRegistry;
}

const Context& BP::GetContext() const
{
    return m_Context;
}

bool BP::GetStyleLight()
{
    return m_StyleLight;
}

void BP::SetStyleLight(bool light)
{
    m_StyleLight = light;
}

void BP::SetContextMonitor(ContextMonitor* monitor)
{
    m_Context.SetContextMonitor(monitor);
}

ContextMonitor* BP::GetContextMonitor()
{
    return m_Context.GetContextMonitor();
}

const ContextMonitor* BP::GetContextMonitor() const
{
    return m_Context.GetContextMonitor();
}

void BP::OnContextRunDone()
{
    LOGI("Execution: Thread Done");
    for (auto node : m_Nodes)
        node->OnStop(m_Context);
}

void BP::OnContextPause()
{
    LOGI("Execution: Thread Paused");
    for (auto node : m_Nodes)
        node->OnPause(m_Context);
}

void BP::OnContextResume()
{
    LOGI("Execution: Thread Resumed");
    for (auto node : m_Nodes)
        node->OnResume(m_Context);
}

void BP::OnContextStepNext()
{
    LOGI("Execution: Step Next to %" PRIu32, StepCount());
    for (auto node : m_Nodes)
        node->OnStepNext(m_Context);
}

void BP::OnContextStepCurrent()
{
    LOGI("Execution: Step Current %" PRIu32, StepCount());
    for (auto node : m_Nodes)
        node->OnStepCurrent(m_Context);
}

void BP::OnContextPreStep()
{
    LOGI("Execution: PreStep %" PRIu32, StepCount());
}

void BP::OnContextPostStep()
{
    LOGI("Execution: PostStep %" PRIu32, StepCount());
}

StepResult BP::Stop()
{
    return m_Context.Stop();
}

StepResult BP::Execute(Node& entryPointNode)
{
    auto nodeIt = std::find(m_Nodes.begin(), m_Nodes.end(), static_cast<Node*>(&entryPointNode));
    if (nodeIt == m_Nodes.end())
        return StepResult::Error;

    if (!m_Context.m_Executing)
        ResetState();
    auto entry_pin = entryPointNode.GetOutputFlowPin();
    if (!entry_pin)
        return StepResult::Error;
#if defined(__EMSCRIPTEN__)
    return m_Context.Start(entryPointNode.m_Exit);
#else
    return m_Context.Execute(*entry_pin);
#endif
}

StepResult BP::Run(Node& entryPointNode)
{
    auto nodeIt = std::find(m_Nodes.begin(), m_Nodes.end(), static_cast<Node*>(&entryPointNode));
    if (nodeIt == m_Nodes.end())
        return StepResult::Error;

    if (!m_Context.m_Executing)
        ResetState();

    auto entry_pin = entryPointNode.GetOutputFlowPin();
    if (!entry_pin)
        return StepResult::Error;
    return m_Context.Run(*entry_pin);
}

StepResult BP::Pause()
{
    return m_Context.Pause();
}

StepResult BP::Next()
{
#if defined(__EMSCRIPTEN__)
    return m_Context.Step();
#else
    return m_Context.ThreadStep();
#endif
}

StepResult BP::Current()
{
    return m_Context.ThreadRestep();
}

bool BP::IsExecuting()
{
    return m_Context.m_Executing;
}

bool BP::IsPaused()
{
    return m_Context.m_Paused;
}

void BP::ShowFlow()
{
    if (!IsExecuting() && CurrentNode() == nullptr)
    {
        ed::PushStyleVar(ed::StyleVar_FlowMarkerDistance, 30.0f);
        ed::PushStyleVar(ed::StyleVar_FlowDuration, 1.0f);
        for (auto& pin : GetPins())
        {
            if (!pin->m_Link)
                continue;
            ed::Flow(pin->m_ID, pin->GetType() == PinType::Flow ? ed::FlowDirection::Forward : ed::FlowDirection::Backward);
        }
        ed::PopStyleVar(2);
    }
    else
    {
        m_Context.ShowFlow();
    }
}

Node* BP::CurrentNode()
{
    return m_Context.CurrentNode();
}

const Node* BP::CurrentNode() const
{
    return m_Context.CurrentNode();
}

Node* BP::NextNode()
{
    return m_Context.NextNode();
}

const Node* BP::NextNode() const
{
    return m_Context.NextNode();
}

FlowPin BP::CurrentFlowPin() const
{
    if (!m_Context.m_Monitor)
        return {};
    return m_Context.CurrentFlowPin();
}

StepResult BP::LastStepResult() const
{
    return m_Context.LastStepResult();
}

uint32_t BP::StepCount() const
{
    return m_Context.StepCount();
}

Node * BP::CreateDummyNode(const imgui_json::value& value, BP& blueprint)
{
    DummyNode * dummy = (BluePrint::DummyNode *)m_NodeRegistry->Create("DummyNode", blueprint);
    imgui_json::GetTo<imgui_json::number>(value, "id", dummy->m_ID);
    imgui_json::GetTo<imgui_json::string>(value, "name", dummy->m_name);
    imgui_json::GetTo<imgui_json::string>(value, "type_name", dummy->m_type_name);
    string v;
    imgui_json::GetTo<imgui_json::string>(value, "type", v);
    NodeTypeFromString(v, dummy->m_type);
    imgui_json::GetTo<imgui_json::string>(value, "style", v);
    NodeStyleFromString(v, dummy->m_style);
    imgui_json::GetTo<imgui_json::string>(value, "catalog", dummy->m_catalog);
    // Create Dummy pins
    dummy->InsertInputPins(value);
    dummy->InsertOutputPins(value);
    return (Node *)dummy;
}

int BP::Load(const imgui_json::value& value)
{
    if (!value.is_object())
        return BP_ERR_NODE_LOAD;

    Clear();

    const imgui_json::array* nodeArray = nullptr;
    if (!imgui_json::GetPtrTo(value, "nodes", nodeArray)) // required
        return BP_ERR_NODE_LOAD;

    //IDGenerator generator;
    for (auto& nodeValue : *nodeArray)
    {
        int ret = 0;
        ID_TYPE typeId;
        if (!imgui_json::GetTo<imgui_json::number>(nodeValue, "type_id", typeId)) // required
            return BP_ERR_NODE_LOAD;

        auto node = m_NodeRegistry->Create(typeId, *this);
        if (!node)
        {
            // Create a Dummy node to replace real node
            node = CreateDummyNode(nodeValue, *this);
            node->Load(nodeValue);
        }
        else if ((ret = node->Load(nodeValue)) != BP_ERR_NONE)
        {
            // Create a Dummy node to replace real node
            node = CreateDummyNode(nodeValue, *this);
            node->Load(nodeValue);
        }

        m_Nodes.emplace_back(node);
    }

    const imgui_json::object* stateObject = nullptr;
    if (!imgui_json::GetPtrTo(value, "state", stateObject)) // required
        return BP_ERR_NODE_LOAD;

    uint32_t generatorState = 0;
    if (!imgui_json::GetTo<imgui_json::number>(*stateObject, "generator_state", generatorState)) // required
        return BP_ERR_NODE_LOAD;

    m_Generator.SetState(generatorState);
    m_IsOpen = true;
    return BP_ERR_NONE;
}

int BP::Import(const imgui_json::value& value, ImVec2 pos)
{
    if (!value.is_object())
        return BP_ERR_GROUP_LOAD;

    if (!value.contains("group") || !value.contains("status") || !value.contains("nodes"))
        return BP_ERR_GROUP_LOAD;

    ID_TYPE typeId;
    auto& groupValue = value["group"];
    if (!imgui_json::GetTo<imgui_json::number>(groupValue, "type_id", typeId)) // required
        return BP_ERR_GROUP_LOAD;

    GroupNode *group_node = (GroupNode *)m_NodeRegistry->Create(typeId, *this);
    if (!group_node)
        return BP_ERR_GROUP_LOAD;

    group_node->LoadGroup(value, pos);
    m_Nodes.emplace_back(group_node);

    return BP_ERR_NONE;
}

void BP::Save(imgui_json::value& value) const
{
    auto& nodesValue = value["nodes"]; // required
    nodesValue = imgui_json::array();
    for (auto& node : m_Nodes)
    {
        imgui_json::value nodeValue;

        nodeValue["type_id"] = imgui_json::number(node->GetTypeInfo().m_ID); // required
        nodeValue["type_name"] = node->GetTypeInfo().m_Name; // optional, to make data readable for humans

        node->Save(nodeValue);

        nodesValue.push_back(nodeValue);
    }

    auto& stateValue = value["state"]; // required
    stateValue["generator_state"] = imgui_json::number(m_Generator.State()); // required
}

int BP::Load(std::string path)
{
    auto value = imgui_json::value::load(path);
    if (!value.second)
        return -1;

    return Load(value.first);
}

bool BP::Save(std::string path) const
{
    imgui_json::value value;
    Save(value);
    return value.save(path, 4);
}

ID_TYPE BP::MakeNodeID(Node* node)
{
    (void)node;
    return m_Generator.GenerateID();
}

ID_TYPE BP::MakePinID(Pin* pin)
{
    if (pin) m_Pins.push_back(pin);

    return m_Generator.GenerateID();
}

Pin * BP::GetPinFromID(ID_TYPE pinid)
{
    if (m_Pins.empty())
        return nullptr;

    auto it = std::find_if(m_Pins.begin(), m_Pins.end(), [pinid](const Pin * const pin)
    {
        return pin->m_ID == pinid;
    });
    if (it != m_Pins.end())
    {
        return *it;
    }
    return nullptr;
}

const Pin * BP::GetPinFromID(ID_TYPE pinid) const
{
    if (m_Pins.empty())
        return nullptr;

    auto it = std::find_if(m_Pins.begin(), m_Pins.end(), [pinid](const Pin * const pin)
    {
        return pin->m_ID == pinid;
    });
    if (it != m_Pins.end())
    {
        return *it;
    }
    return nullptr;
}

bool BP::HasPinAnyLink(const Pin& pin) const
{
    if (pin.IsLinked())
        return true;

    if (pin.m_LinkFrom.size() > 0)
        return true;

    return false;
}

vector<Pin*> BP::FindPinsLinkedTo(const Pin& pin) const
{
    vector<Pin*> result;
    for (auto& p : m_Pins)
    {
        auto linkedPin = p->GetLink(this);
        if(std::find(m_Pins.begin(), m_Pins.end(), linkedPin) != m_Pins.end())
        {
            if (linkedPin && linkedPin->m_ID == pin.m_ID)
                result.push_back(p);
        }
    }
    return result;
}

void BP::ResetState()
{
    m_Context.ResetState();

    for (auto node : m_Nodes)
        node->Reset(m_Context);
}
# pragma endregion

# pragma region Action
Action::Action(std::string name, std::string icon, OnTriggeredEvent::Delegate delegate)
    : m_Name(name), m_Icon(icon)
{
    if (delegate)
        OnTriggered += std::move(delegate);
} 

void Action::SetName(std::string name)
{
    if (m_Name == name)
        return;

    m_Name = name;

    OnChange(this);
}

const std::string& Action::GetName() const
{
    return m_Name;
}

void Action::SetIcon(std::string icon)
{
    if (m_Icon == icon)
        return;

    m_Icon = icon;

    OnChange(this);
}

const std::string& Action::GetIcon() const
{
    return m_Icon;
}

void Action::SetEnabled(bool set)
{
    if (m_IsEnabled == set)
        return;

    m_IsEnabled = set;

    OnChange(this);
}

bool Action::IsEnabled() const
{
    return m_IsEnabled;
}

void Action::Execute(void * handle)
{
    //LOGV("Action: %s", m_Name.c_str());
    m_UsrData = handle;
    OnTriggered();
}
# pragma endregion

} // namespace BluePrint
