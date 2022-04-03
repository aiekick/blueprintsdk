#pragma once
#include <stddef.h> // size_t
#include <stdint.h> // intX_t, uintX_t
#include <limits.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <map>
#include <memory>
#include <imgui_json.h>
#include <variant.hpp> // variant
# define span_FEATURE_MAKE_SPAN 1
#include <span.hpp>
#include <imgui.h>
#include <imgui_helper.h>
#undef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#define BP_ERR_NONE          0
#define BP_ERR_GENERAL      -1
#define BP_ERR_NODE_LOAD    -2
#define BP_ERR_PIN_NUMPER   -3
#define BP_ERR_INPIN_LOAD   -4
#define BP_ERR_OUTPIN_LOAD  -5
#define BP_ERR_PIN_LINK     -6
#define BP_ERR_DOC_LOAD     -7
#define BP_ERR_GROUP_LOAD   -8

typedef uint32_t ID_TYPE;
typedef uint32_t VERSION_TYPE;

using nonstd::get;
using nonstd::monostate;
using nonstd::span;
using nonstd::make_span;
using std::string;
using std::vector;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

#include "Pin.h"

#define OFFSET_32 0x811c9dc5
#define OFFSET_64 0xcbf29ce484222325

#define PRIME_32 0x01000193
#define PRIME_64 0x100000001b3

namespace BluePrint
{
# pragma region fnv1a
inline uint64_t fnv1a_hash_64(string str) 
{
    uint64_t prime = PRIME_64;
    uint64_t hash = OFFSET_64;
    for (int i = 0; i < str.length(); i++) 
    {
        hash = hash ^ str[i];
        hash = hash * prime;
    }
    // return correct digits, based on size
    return hash;
}

inline uint32_t fnv1a_hash_32(string str) 
{
    uint32_t prime = PRIME_32;
    uint32_t hash = OFFSET_32;
    for (int i = 0; i < str.length(); i++) 
    {
        hash = hash ^ str[i];
        hash = hash * prime;
    }
    // return correct digits, based on size
    return hash;
}
# pragma endregion

template <typename T>
inline bool GetPtrTo(const imgui_json::value& value, std::string key, const T*& result)
{
    if (!value.contains(key))
        return false;
    auto& valueObject = value[key];
    auto valuePtr = valueObject.get_ptr<T>();
    if (valuePtr == nullptr)
        return false;
    result = valuePtr;
    return true;
};

template <typename T, typename V>
inline bool GetTo(const imgui_json::value& value, std::string key, V& result)
{
    const T* valuePtr = nullptr;
    if (!GetPtrTo(value, key, valuePtr))
        return false;
    result = static_cast<V>(*valuePtr);
    return true;
};

struct NodeRegistry;
struct Node;
struct Context;
enum class StepResult
{
    Success,
    Done,
    Error
};

# pragma region IDGenerator
struct IDGenerator
{
    ID_TYPE GenerateID();

    void SetState(ID_TYPE state);
    ID_TYPE State() const;

private:
    ID_TYPE m_State = ImGui::get_current_time_usec();
};
# pragma endregion


# pragma region Context
struct ContextMonitor
{
    virtual ~ContextMonitor() {};
    virtual void OnStart(Context& context) {}
    virtual void OnError(Context& context) {}
    virtual void OnDone(Context& context) {}
    virtual void OnPause(Context& context) {}
    virtual void OnResume(Context& context) {}
    virtual void OnStepNext(Context& context) {}
    virtual void OnStepCurrent(Context& context) {}

    virtual void OnPreStep(Context& context) {}
    virtual void OnPostStep(Context& context) {}
};

struct IMGUI_API Context
{
    void SetContextMonitor(ContextMonitor* monitor);
            ContextMonitor* GetContextMonitor();
    const   ContextMonitor* GetContextMonitor() const;

    void ResetState();

    StepResult Start(FlowPin& entryPoint);
    StepResult Step(Context * context = nullptr, bool restep = false);
    StepResult Restep(Context * context = nullptr);
    
    StepResult Run(FlowPin& entryPoint);        // non-thread run, blocking mode
    StepResult Execute(FlowPin& entryPoint);
    StepResult Pause();
    StepResult ThreadStep();
    StepResult ThreadRestep();
    StepResult Stop();

    Node* CurrentNode();
    const Node* CurrentNode() const;

    Node* NextNode();
    const Node* NextNode() const;

    FlowPin CurrentFlowPin() const;

    StepResult LastStepResult() const;

    uint32_t StepCount() const;

    void PushReturnPoint(FlowPin& entryPoint);

    template <typename T>
    auto GetPinValue(Pin& pin, bool threading = false) const;

    void SetPinValue(const Pin& pin, PinValue value);
    PinValue GetPinValue(const Pin& pin, bool threading = false) const;

    StepResult SetStepResult(StepResult result);

    void ShowFlow();

    ContextMonitor*             m_Monitor  {nullptr};
    bool                        m_Executing {false};
    bool                        m_Paused {false};
    bool                        m_StepToNext {false};
    bool                        m_StepCurrent {false};
    bool                        m_ThreadRunning {false};    // sub-thread is running
    bool                        m_pause_event   {false};


    std::vector<FlowPin>            m_Callstack;
    Node*                           m_CurrentNode {nullptr};
    Node*                           m_PrevNode {nullptr};
    FlowPin                         m_CurrentFlowPin = {};
    FlowPin                         m_PrevFlowPin = {};
    StepResult                      m_LastResult {StepResult::Done};
    uint32_t                        m_StepCount {0};
    std::map<uint32_t, PinValue>    m_Values;
    std::thread*                    m_thread {nullptr};
};

template <typename T>
inline auto Context::GetPinValue(Pin& pin, bool threading) const
{
    return GetPinValue(pin, threading).As<T>();
}

# pragma endregion

# pragma region Action
enum class EventHandle : uint64_t { Invalid };
template <typename... Args>
struct Event
{
    using Delegate = std::function<void(Args...)>;

    EventHandle Add(Delegate delegate)
    {
        auto eventHandle = static_cast<EventHandle>(++m_LastHandleId);
        m_Delegates[eventHandle] = std::move(delegate);
        return eventHandle;
    }

    bool Remove(EventHandle eventHandle)
    {
        return m_Delegates.erase(eventHandle) > 0;
    }

    void Clear()
    {
        m_Delegates.clear();
    }

    template <typename... CallArgs>
    void Invoke(CallArgs&&... args)
    {
        vector<Delegate> delegates;
        delegates.reserve(m_Delegates.size());
        for (auto& entry : m_Delegates)
            delegates.push_back(entry.second);

        for (auto& delegate : delegates)
            delegate(args...);
    }

    EventHandle operator += (Delegate delegate)       { return Add(std::move(delegate)); }
    bool        operator -= (EventHandle eventHandle) { return Remove(eventHandle);      }
    template <typename... CallArgs>
    void        operator () (CallArgs&&... args)      { Invoke(std::forward<CallArgs>(args)...); }

private:
    using EventHandleType = std::underlying_type_t<EventHandle>;

    std::map<EventHandle, Delegate> m_Delegates;
    EventHandleType            m_LastHandleId = 0;
};

struct IMGUI_API Action
{
    using OnChangeEvent     = Event<Action*>;
    using OnTriggeredEvent  = Event<>;

    Action() = default;
    Action(std::string name, std::string icon = "", OnTriggeredEvent::Delegate delegate = {});

    void SetName(std::string name);
    const string& GetName() const;

    void SetIcon(std::string icon);
    const string& GetIcon() const;

    void SetEnabled(bool set);
    bool IsEnabled() const;

    void Execute(void * handle = nullptr);

    OnChangeEvent       OnChange;
    OnTriggeredEvent    OnTriggered;

private:
    string  m_Name;
    string  m_Icon;
    bool    m_IsEnabled = true;
public:
    void*   m_UsrData = nullptr;
};
# pragma endregion

# pragma region BP
struct IMGUI_API BP
{
    BP(shared_ptr<NodeRegistry> nodeRegistry = nullptr, shared_ptr<PinExRegistry> pinexRegistry = nullptr);
    BP(const BP& other);
    BP(BP&& other);
    ~BP();

    BP& operator=(const BP& other);
    BP& operator=(BP&& other);

    template <typename T>
    T* CreateNode()
    {
        if (auto node = CreateNode(T::GetStaticTypeInfo().m_ID))
            return static_cast<T*>(node);
        else
            return nullptr;
    }

    Node* CreateNode(ID_TYPE nodeTypeId);
    Node* CreateNode(std::string nodeTypeName);
    void DeleteNode(Node* node);
    Node* CloneNode(Node* node);
    void InsertNode(Node* node);

    void ForgetPin(Pin* pin);

    void Clear();

    span<      Node*>       GetNodes();
    span<const Node* const> GetNodes() const;

    span<      Pin*>       GetPins();
    span<const Pin* const> GetPins() const;

            Node* FindNode(ID_TYPE nodeId);
    const   Node* FindNode(ID_TYPE nodeId) const;

            Pin* FindPin(ID_TYPE pinId);
    const   Pin* FindPin(ID_TYPE pinId) const;

    shared_ptr<NodeRegistry> GetNodeRegistry() const;
    shared_ptr<PinExRegistry> GetPinExRegistry() const;

    const Context& GetContext() const;

    void    SetContextMonitor(ContextMonitor* monitor);
            ContextMonitor* GetContextMonitor();
    const   ContextMonitor* GetContextMonitor() const;

    StepResult Run(Node& entryPointNode);
    StepResult Execute(Node& entryPointNode);
    StepResult Stop();
    StepResult Pause();
    StepResult Next();
    StepResult Current();
    bool IsOpened() { return m_IsOpen; }
    void SetOpen(bool opened) { m_IsOpen = opened; }
    bool IsExecuting();
    bool IsPaused();
    void ShowFlow();
    bool GetStyleLight();
    void SetStyleLight(bool light = true);

    Node* CurrentNode();
    const Node* CurrentNode() const;

    Node* NextNode();
    const Node* NextNode() const;

    FlowPin CurrentFlowPin() const;

    StepResult LastStepResult() const;

    uint32_t StepCount() const;

    int Load(const imgui_json::value& value);
    int Import(const imgui_json::value& value, ImVec2 pos);
    void Save(imgui_json::value& value) const;

    int Load(std::string path);
    bool Save(std::string path) const;

    ID_TYPE MakeNodeID(Node* node);
    ID_TYPE MakePinID(Pin* pin);

    bool HasPinAnyLink(const Pin& pin) const;

    Pin * GetPinFromID(ID_TYPE pinid);
    const Pin * GetPinFromID(ID_TYPE pinid) const;

    std::vector<Pin*> FindPinsLinkedTo(const Pin& pin) const;

    void OnContextRunDone();
    void OnContextPause();
    void OnContextResume();
    void OnContextPreStep();
    void OnContextPostStep();
    void OnContextStepNext();
    void OnContextStepCurrent();

private:
    void ResetState();
    Node * CreateDummyNode(const imgui_json::value& value, BP& blueprint);

    shared_ptr<NodeRegistry>        m_NodeRegistry;
    shared_ptr<PinExRegistry>       m_PinExRegistry;
    IDGenerator                     m_Generator;
    std::vector<Node*>              m_Nodes;
    std::vector<Pin*>               m_Pins;
    Context                         m_Context;
    bool                            m_StyleLight {false};
    bool                            m_IsOpen {false};
};
# pragma endregion

} // namespace BluePrint

# define VERSION_MAJOR(v)   ((v & 0xFF000000) >> 24)
# define VERSION_MINOR(v)   ((v & 0x00FF0000) >> 16)
# define VERSION_PATCH(v)   ((v&0x0000FF00)>>8)
# define VERSION_BUILT(v)   (v&0x000000FF)
# define MAJOR  1
# define MINOR  7
# define PATCH  7
# define BUILT  0

// Please Set CMakeList.txt IMGUI_BP_SDK_VERSION_STRING also
# define VERSION_BLUEPRINT  ((MAJOR << 24) | (MINOR << 16) | (PATCH << 8) | BUILT)

# define BP_NODE(type, node_version, node_type, node_style, node_catalog) \
    static ::BluePrint::NodeTypeInfo GetStaticTypeInfo() \
    { \
        return \
        { \
            fnv1a_hash_32(#type + string("*") + node_catalog), \
            #type, \
            #type, \
            node_version, \
            node_type, \
            node_style, \
            node_catalog, \
            [](::BluePrint::BP& blueprint) -> ::BluePrint::Node* { return new type(blueprint); } \
        }; \
    } \
    \
    ::BluePrint::NodeTypeInfo GetTypeInfo() const override \
    { \
        return GetStaticTypeInfo(); \
    }

# define BP_NODE_WITH_NAME(type, name, node_version, node_type, node_style, node_catalog) \
    static ::BluePrint::NodeTypeInfo GetStaticTypeInfo() \
    { \
        return \
        { \
            fnv1a_hash_32(#type + string("*") + node_catalog), \
            #type, \
            name, \
            node_version, \
            node_type, \
            node_style, \
            node_catalog, \
            [](::BluePrint::BP& blueprint) -> ::BluePrint::Node* { return new type(blueprint); } \
        }; \
    } \
    \
    ::BluePrint::NodeTypeInfo GetTypeInfo() const override \
    { \
        return GetStaticTypeInfo(); \
    }

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

# define BP_NODE_DYNAMIC(type, node_version, node_type, node_style, node_catalog) \
    extern "C" EXPORT int32_t version() { \
        return VERSION_BLUEPRINT; \
    } \
    \
    extern "C" EXPORT BluePrint::NodeTypeInfo* create(BluePrint::BP* blueprint) { \
        return new BluePrint::NodeTypeInfo\
        ( \
            BluePrint::fnv1a_hash_32(#type + string("*") + node_catalog), \
            #type, \
            #type, \
            node_version, \
            node_type, \
            node_style, \
            node_catalog, \
            [](::BluePrint::BP& blueprint) -> ::BluePrint::Node* { return new BluePrint::type(blueprint); } \
        ); \
    } \
    \
    extern "C" EXPORT void destroy(BluePrint::NodeTypeInfo* pObj) { \
        delete pObj; \
    }
    
