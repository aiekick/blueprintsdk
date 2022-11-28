#pragma once
#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <imgui_helper.h>
#include <ImGuiFileDialog.h>
#include <dir_iterate.h>
#include <imgui_node_editor.h>
#include <BluePrint.h>
#include <Node.h>
#include <Pin.h>
#include <Utils.h>
#include <Debug.h>
#include <Document.h>
#include <inttypes.h>

#if IMGUI_ICONS
#include <icons.h>
#define ICON_OPEN_BLUEPRINT     u8"\uf07c"
#define ICON_IMPORT_GROUP       u8"\ue661"
#define ICON_NEW_BLUEPRINT      u8"\ue89c"
#define ICON_SAVE_BLUEPRINT     u8"\uf0c7"
#define ICON_SAVEAS_BLUEPRINT   u8"\ue992"
#define ICON_NEW_NODE           u8"\ue9fe"
#define ICON_UNLINK             u8"\ue646"
#define ICON_SHOW_FLOW          u8"\uf3b2"
#define ICON_BP_ZOOM_IN         u8"\uf1b2"
#define ICON_BP_ZOOM_OUT        u8"\uf1b3"
#define ICON_NODE_SETTING       u8"\ue8b8"
#define ICON_NODE_DEBUG         u8"\uf67b"
#define ICON_NODE_DLL           u8"\uf0c1"
#define ICON_RUN_MAIN           u8"\uf01d"
#define ICON_STEP_NEXT          u8"\uf050"
#define ICON_BREAKPOINT         u8"\uf28b"
#define ICON_BLUEPRINT_STYLE    u8"\uf576"
#define ICON_NODE_COPY          u8"\ue02e"
#define ICON_NODE_SEARCH        u8"\uf002"
#define ICON_METERS             u8"\ue41c"
#define ICON_NODE_DELETE        ICON_MD_DELETE
#define ICON_NODE_OPEN          ICON_MD_FOLDER_OPEN
#define ICON_NODE_FILE          u8"\ue173"
#define ICON_NODE_CLEAR         ICON_MD_CLEAR_ALL
#define ICON_NODE_NEXT          u8"\uf0da"
#define ICON_NODE_ENABLE        u8"\ue8f4"
#define ICON_NODE_DISABLE       u8"\ue8f5"
#define ICON_RESET              u8"\ue042"
#define ICON_THUMBNAIL          u8"\ue8d9"
#else
#define ICON_OPEN_BLUEPRINT     "Open"
#define ICON_NEW_BLUEPRINT      "New"
#define ICON_SAVE_BLUEPRINT     "Save"
#define ICON_SAVEAS_BLUEPRINT   "SaveAs"
#define ICON_NEW_NODE           "+"
#define ICON_UNLINK             "UnLink"
#define ICON_SHOW_FLOW          "Show"
#define ICON_BP_ZOOM_IN         "Zoom In"
#define ICON_BP_ZOOM_OUT        "Zoom Out"
#define ICON_NODE_COPY          "C"
#define ICON_NODE_SETTING       "S"
#define ICON_NODE_DELETE        "D"
#define ICON_BREAKPOINT         "B"
#define ICON_BLUEPRINT_STYLE    "T"
#define ICON_NODE_DEBUG         "Start"
#define ICON_NODE_DLL           "L"
#define ICON_RUN_MAIN           "Run"
#define ICON_STEP_NEXT          "Next"
#define ICON_METERS             "M"
#define ICON_NODE_OPEN          "*"
#define ICON_NODE_FILE          ">"
#define ICON_NODE_CLEAR         "x"
#define ICON_NODE_SEARCH        "f"
#define ICON_NODE_NEXT          ">"
#define ICON_RESET              "R"
#define ICON_THUMBNAIL          "N"
#endif

namespace ed = ax::NodeEditor;

enum BluePrintStyleColor
{
    BluePrintStyleColor_TitleBg = 0,
    BluePrintStyleColor_TitleBgDummy,
    BluePrintStyleColor_GroupBg,
    BluePrintStyleColor_Border,
    BluePrintStyleColor_DummyBorder,
    BluePrintStyleColor_ToolButton,
    BluePrintStyleColor_ToolButtonActive,
    BluePrintStyleColor_ToolButtonHovered,
    BluePrintStyleColor_DebugCurrentNode,
    BluePrintStyleColor_DebugNextNode,
    BluePrintStyleColor_DebugBreakPointNode,
    BluePrintStyleColor_PinVoid,
    BluePrintStyleColor_PinAny,
    BluePrintStyleColor_PinVector,
    BluePrintStyleColor_PinMat,
    BluePrintStyleColor_PinFlow,
    BluePrintStyleColor_PinBool,
    BluePrintStyleColor_PinInt32,
    BluePrintStyleColor_PinInt64,
    BluePrintStyleColor_PinFloat,
    BluePrintStyleColor_PinDouble,
    BluePrintStyleColor_PinString,
    BluePrintStyleColor_PinPoint,
    BluePrintStyleColor_PinCustom,
    BluePrintStyleColor_Count,
};
namespace BluePrint
{
enum class BluePrintStyle:int32_t 
{
    BP_Style_BluePrint = 0,
    BP_Style_Light,
    BP_Style_Mono,
    BP_Style_Custom,
};

BluePrintStyle BPStyleFromName(string name);
string BPStyleToString(BluePrintStyle style);

struct IMGUI_API BluePrintUI;
# pragma region Popup
struct ContextMenu
{
    void Open(void);
    void Show(BluePrintUI& UI);
};

struct NodeContextMenu
{
    void Open(Node* node = nullptr);
    void Show(BluePrintUI& UI);
};

struct PinContextMenu
{
    void Open(Pin* pin = nullptr);
    void Show(BluePrintUI& UI);
};

struct LinkContextMenu
{
    void Open(Pin* pin = nullptr);
    void Show(BluePrintUI& UI);
};

struct NodeDeleteDialog
{
    void Open(Node* node = nullptr);
    void Show(BluePrintUI& UI);
};

enum BluePrintFlag : int32_t
{
    BluePrintFlag_None = 0,
    BluePrintFlag_Filter = 1,
    BluePrintFlag_Fusion = 1 << 1,
    BluePrintFlag_System = 1 << 2,
    // layout
    BluePrintFlag_Vertical = 1 << 8,
    BluePrintFlag_All = 1 << 31,
};

struct NodeCreateDialog
{
    void Open(Pin* fromPin = nullptr, uint32_t flag = BluePrintFlag::BluePrintFlag_All);
    void Show(BluePrintUI& UI);

          Node* GetCreatedNode()       { return m_CreatedNode; }
    const Node* GetCreatedNode() const { return m_CreatedNode; }

    span<      Pin*>       GetCreatedLinks()       { return m_CreatedLinks; }
    span<const Pin* const> GetCreatedLinks() const { return make_span(const_cast<const Pin* const*>(m_CreatedLinks.data()), m_CreatedLinks.size()); }

private:
    vector<Pin*> CreateLinkToFirstMatchingPin(Node& node, Pin& fromPin);

    Node*           m_CreatedNode = nullptr;
    vector<Pin*>    m_CreatedLinks;
};

struct NodeSettingDialog
{
    void Open(Node* node = nullptr);
    void Show(BluePrintUI& UI);
};
# pragma endregion

enum BluePrintCallBack:int 
{
    BP_CB_Unknown = -1,
    BP_CB_Link,
    BP_CB_Unlink,
    BP_CB_NODE_DELETED,
    BP_CB_PARAM_CHANGED,
    BP_CB_SETTING_CHANGED,
    BP_CB_Custom,
};

enum BluePrintCallBackReturn:int 
{
    BP_CBR_Unknown = -1,
    BP_CBR_Nothing,
    BP_CBR_AutoLink,
    BP_CBR_Custom,
};

typedef int (*BluePrintCallback)(int type, std::string name, void* handle);
typedef struct BluePrintCallbackFunctions
{
    BluePrintCallback   BluePrintOnInitialized  {nullptr};
    BluePrintCallback   BluePrintOnLoaded       {nullptr};
    BluePrintCallback   BluePrintOnSave         {nullptr};
    BluePrintCallback   BluePrintOnChanged      {nullptr};
    BluePrintCallback   BluePrintOnStart        {nullptr};
    BluePrintCallback   BluePrintOnPause        {nullptr};
    BluePrintCallback   BluePrintOnStop         {nullptr};
    BluePrintCallback   BluePrintOnQuit         {nullptr};
    BluePrintCallback   BluePrintOnReset        {nullptr};

} BluePrintCallbackFunctions;

struct BluePrintUI
{
    BluePrintUI();
    void Initialize(const char * bp_file = nullptr, const char * plugin_path = nullptr);
    void Finalize();
    bool Frame(bool child_window = false, bool show_node = true, bool bp_enabled = true, uint32_t flag = BluePrintFlag::BluePrintFlag_All);
    void SetStyle(enum BluePrintStyle style = BluePrintStyle::BP_Style_BluePrint);
    void SetCallbacks(BluePrintCallbackFunctions callbacks, void * handle);
    
    ed::Config                      m_Config;
    ed::EditorContext*              m_Editor {nullptr};
    unique_ptr<Document>            m_Document {nullptr};
    ImGuiFileDialog                 m_FileDialog;
    std::string                     m_BookMarkPath;
    std::vector<ClipNode>           m_ClipBoard;
    bool                            m_isNewNodePopuped {false};
    bool                            m_isChildWindow {false};
    bool                            m_isShowInfoTooltips {false};
    bool                            m_isShowThumbnails {false};
    float                           m_ThumbnailScale {0.25f};
    int                             m_ThumbnailShowCount {0};
    Pin*                            m_newNodeLinkPin {nullptr};
    ImVec4                          m_StyleColors[BluePrintStyleColor_Count];
    ImVec2                          m_PopupMousePos {};
    enum BluePrintStyle             m_Style {BluePrintStyle::BP_Style_BluePrint};
private:
    DebugOverlay*                   m_DebugOverlay {nullptr};

private:
    ContextMenu         m_ContextMenu;
    NodeContextMenu     m_NodeContextMenu;
    PinContextMenu      m_PinContextMenu;
    LinkContextMenu     m_LinkContextMenu;
    NodeDeleteDialog    m_NodeDeleteDialog;
    NodeCreateDialog    m_NodeCreateDialog;
    NodeSettingDialog   m_NodeSettingDialog;

public:
    void    UpdateActions();
    void    CleanStateStorage();
    Node*   ShowNewNodeMenu(ImVec2 popupPosition = {}, std::string catalog_filter = "");
    void    ShowStyleEditor(bool* show = nullptr);
    void    ShowToolbar(bool* show = nullptr);
    void    ShowShortToolbar(bool vertical = true, bool* show = nullptr);
    void    Thumbnails(float view_expand = 1.0f, ImVec2 size = ImVec2(0, 0), ImVec2 pos = ImVec2(-1, -1));
    bool    Blueprint_IsValid();

    bool File_IsOpen();
    bool File_IsModified();
    void File_MarkModified();
    bool File_Open(std::string path, string* error = nullptr);
    bool File_Open();
    bool File_Import(std::string path, ImVec2 pos, string* error = nullptr);
    bool File_Import();
    bool File_Export(Node * group_node);
    bool File_New();
    bool File_New_Filter(imgui_json::value& bp, std::string name, std::string sfilter);
    bool File_New_Fusion(imgui_json::value& bp, std::string name, std::string sfilter);
    bool File_SaveAsEx(std::string path);
    bool File_SaveAs();
    bool File_Save();
    bool File_Close();
    bool File_Exit();

    bool Edit_Undo();
    bool Edit_Redo();
    bool Edit_Cut();
    bool Edit_Copy();
    bool Edit_Paste();
    bool Edit_Duplicate();
    bool Edit_Delete();
    bool Edit_Unlink();
    bool Edit_Setting();
    bool Edit_Insert(ID_TYPE id);

    bool View_ShowFlow();
    bool View_ShowMeters();
    bool View_ZoomToContent();
    bool View_ZoomToSelection();
    bool View_NavigateBackward();
    bool View_NavigateForward();

    bool Blueprint_Run();
    bool Blueprint_Stop();
    bool Blueprint_Pause();
    bool Blueprint_Next();
    bool Blueprint_Current();
    bool Blueprint_BreakPoint();

    Node* FindEntryPointNode();
    Node* FindExitPointNode();

    bool Blueprint_SetFilter(const std::string name, const PinValue& value);
    bool Blueprint_RunFilter(ImGui::ImMat& input, ImGui::ImMat& output);
    bool Blueprint_SetFusion(const std::string name, const PinValue& value);
    bool Blueprint_RunFusion(ImGui::ImMat& input_first, ImGui::ImMat& input_second, ImGui::ImMat& output, int64_t current, int64_t duration);

    Action m_File_Open       = { "Open...",         ICON_OPEN_BLUEPRINT,   [this] { File_Open();        } };
    Action m_File_Import     = { "Import...",       ICON_IMPORT_GROUP,     [this] { File_Import();      } };
    Action m_File_New        = { "New",             ICON_NEW_BLUEPRINT,    [this] { File_New();         } };
    Action m_File_SaveAs     = { "Save As...",      ICON_SAVEAS_BLUEPRINT, [this] { File_SaveAs();      } };
    Action m_File_Save       = { "Save",            ICON_SAVE_BLUEPRINT,   [this] { File_Save();        } };
    Action m_File_Close      = { "Close",           ICON_FAD_CLOSE,        [this] { File_Close();       } };
    Action m_File_Exit       = { "Exit",            ICON_MD_EXIT_TO_APP,   [this] { File_Exit();        } };

    Action m_Edit_Undo       = { "Undo",            ICON_MD_UNDO,          [this] { Edit_Undo();      } };
    Action m_Edit_Redo       = { "Redo",            ICON_MD_REDO,          [this] { Edit_Redo();      } };
    Action m_Edit_Cut        = { "Cut",             ICON_MD_CONTENT_CUT,   [this] { Edit_Cut();       } };
    Action m_Edit_Copy       = { "Copy",            ICON_MD_CONTENT_COPY,  [this] { Edit_Copy();      } };
    Action m_Edit_Paste      = { "Paste",           ICON_MD_CONTENT_PASTE, [this] { Edit_Paste();     } };
    Action m_Edit_Duplicate  = { "Duplicate",       ICON_NODE_COPY,        [this] { Edit_Duplicate(); } };
    Action m_Edit_Delete     = { "Delete",          ICON_MD_DELETE,        [this] { Edit_Delete();    } };
    Action m_Edit_Unlink     = { "Unlink",          ICON_UNLINK,           [this] { Edit_Unlink();    } };
    Action m_Edit_Setting    = { "Setting",         ICON_MD_SETTINGS,      [this] { Edit_Setting();   } };

    Action m_View_ShowFlow          = { "Show Flow",         ICON_SHOW_FLOW,             [this] { View_ShowFlow();           } };
    Action m_View_ShowMeters        = { "Show Meters",       ICON_METERS,                [this] { View_ShowMeters();         } };
    Action m_View_ZoomToContent     = { "Zoom To Content",   ICON_BP_ZOOM_OUT,           [this] { View_ZoomToContent();      } };
    Action m_View_ZoomToSelection   = { "Zoom To Selection", ICON_BP_ZOOM_IN,            [this] { View_ZoomToSelection();    } };
    Action m_View_NavigateBackward  = { "Navigate Backward", ICON_MD_ARROW_BACK,         [this] { View_NavigateBackward();   } };
    Action m_View_NavigateForward   = { "Navigate Forward",  ICON_MD_ARROW_FORWARD,      [this] { View_NavigateForward();    } };

    Action m_Blueprint_Run          = { "Run",          ICON_FA_PLAY,           [this] { Blueprint_Run();           } };
    Action m_Blueprint_Stop         = { "Stop",         ICON_FA_STOP,           [this] { Blueprint_Stop();          } };
    Action m_Blueprint_Pause        = { "Pause",        ICON_FA_PAUSE,          [this] { Blueprint_Pause();         } };
    Action m_Blueprint_Next         = { "Next",         ICON_STEP_NEXT,         [this] { Blueprint_Next();          } };
    Action m_Blueprint_Current      = { "Restep",       ICON_MD_SYNC,           [this] { Blueprint_Current();       } };
    Action m_Blueprint_BreakPoint   = { "BreakPoint",   ICON_BREAKPOINT,        [this] { Blueprint_BreakPoint();    } };

private:
    void                InstallDocumentCallbacks();
    bool                CheckNodeStyle(const Node* node, NodeStyle style);
    float               DrawNodeToolBar(Node *node, Node **need_clone_node);
    void                DrawNodes();
    void                DrawInfoTooltip();
    void                ShowDialogs();
    void                FileDialogs();
    void                HandleCreateAction(uint32_t flag = BluePrintFlag::BluePrintFlag_All);
    void                HandleDestroyAction();
    void                HandleContextMenuAction(uint32_t flag = BluePrintFlag::BluePrintFlag_All);
    void                InitFileDialog(const char * bookmark_path = nullptr);

private:
    void                CreateNewDocument();
    void                CreateNewFilterDocument();
    void                CreateNewFusionDocument();
    void                CommitLinksToEditor();
    bool                ReadyToQuit {false};

public:
    BluePrintCallbackFunctions  m_CallBacks;
    void *                      m_UserHandle {nullptr};
};
} // namespace BluePrint
