# pragma once
# include <imgui.h>
# include <vector>
# include <string>
# include <time.h>
# include <stdint.h>

namespace imgui_logger {

using std::vector;
using std::string;

enum class LogLevel: int32_t
{
    Verbose,
    Info,
    Warning,
    Error,
};

enum LogColor
{
    LogColor_HighlightBorder = 0,
    LogColor_HighlightFill,
    LogColor_PinBorder,
    LogColor_PinFill,
    LogColor_LogTimeColor,
    LogColor_LogSymbolColor,
    LogColor_LogStringColor,
    LogColor_LogTagColor,
    LogColor_LogKeywordColor,
    LogColor_LogTextColor,
    LogColor_LogOutlineColor,
    LogColor_LogNumberColor,
    LogColor_LogVerboseColor,
    LogColor_LogWarningColor,
    LogColor_LogErrorColor,
    LogColor_LogInfoColor,
    LogColor_LogAssertColor,
    LogColor_Count,
};

# define LOGV(...)      ::imgui_logger::OverlayLogger::GetCurrent()->Log(::imgui_logger::LogLevel::Verbose,  __VA_ARGS__)
# define LOGI(...)      ::imgui_logger::OverlayLogger::GetCurrent()->Log(::imgui_logger::LogLevel::Info,     __VA_ARGS__)
# define LOGW(...)      ::imgui_logger::OverlayLogger::GetCurrent()->Log(::imgui_logger::LogLevel::Warning,  __VA_ARGS__)
# define LOGE(...)      ::imgui_logger::OverlayLogger::GetCurrent()->Log(::imgui_logger::LogLevel::Error,    __VA_ARGS__)


struct IMGUI_API OverlayLogger
{
    OverlayLogger()
    {
        m_Colors[LogColor_HighlightBorder]             = ImColor(  5, 130, 255, 128);
        m_Colors[LogColor_HighlightFill]               = ImColor(  5, 130, 255,  64);
        m_Colors[LogColor_PinBorder]                   = ImColor(255, 176,  50,   0);
        m_Colors[LogColor_PinFill]                     = ImColor(  0,  75, 150, 128);

        m_Colors[LogColor_LogTimeColor]                = ImColor(150, 209,   0, 255);
        m_Colors[LogColor_LogSymbolColor]              = ImColor(192, 192, 192, 255);
        m_Colors[LogColor_LogStringColor]              = ImColor(255, 174, 133, 255);
        m_Colors[LogColor_LogTagColor]                 = ImColor(255, 214, 143, 255);
        m_Colors[LogColor_LogKeywordColor]             = ImColor(255, 255, 255, 255);
        m_Colors[LogColor_LogTextColor]                = ImColor(192, 192, 192, 255);
        m_Colors[LogColor_LogOutlineColor]             = ImColor(  0,   0,   0, 255);
        m_Colors[LogColor_LogNumberColor]              = ImColor(255, 255, 128, 255);
        m_Colors[LogColor_LogVerboseColor]             = ImColor(128, 255, 128, 255);
        m_Colors[LogColor_LogWarningColor]             = ImColor(255, 255, 192, 255);
        m_Colors[LogColor_LogErrorColor]               = ImColor(255, 152, 152, 255);
        m_Colors[LogColor_LogInfoColor]                = ImColor(138, 197, 255, 255);
        m_Colors[LogColor_LogAssertColor]              = ImColor(255,  61,  68, 255);
    }

    static void SetCurrent(OverlayLogger* instance);
    static OverlayLogger* GetCurrent();

    void Log(LogLevel level, const char* format, ...) IM_FMTARGS(3);

    void Update(float dt);
    void Draw(const ImVec2& a, const ImVec2& b);

    void AddKeyword(string keyword);
    void RemoveKeyword(string keyword);
    void SetLogColor(LogColor index, ImColor col);

private:
    struct Range
    {
        int     m_Start = 0;
        int     m_Size  = 0;
        ImColor m_Color;
    };

    struct Entry
    {
        LogLevel        m_Level     = LogLevel::Verbose;
        time_t          m_Timestamp = 0;
        string          m_Text;
        float           m_Timer     = 0.0f;
        bool            m_IsPinned  = false;
        vector<Range>   m_ColorRanges;
    };

    ImColor GetLevelColor(LogLevel level) const;

    void TintVertices(ImDrawList* drawList, int firstVertexIndex, ImColor color, float alpha, int rangeStart, int rangeSize);

    vector<Range> ParseMessage(LogLevel level, string message) const;

    float           m_OutlineSize                 = 0.5f;
    float           m_Padding                     = 10.0f;
    float           m_MessagePresentationDuration = 5.0f;
    float           m_MessageFadeOutDuration      = 0.5f;
    float           m_MessageLifeDuration         = m_MessagePresentationDuration + m_MessageFadeOutDuration;
    bool            m_HoldTimer                   = false;
    int32_t         m_EntrySize                   = 1024;
    vector<Entry>   m_Entries;
    vector<string>  m_Keywords;
    ImColor         m_Colors[LogColor_Count];

    static OverlayLogger* s_Instance;
};

} // namespace imgui_logger