#include <application.h>
#include <UI.h>

#define ENABLE_MULTI_VIEWPORT   1

static std::string ini_file = "test_blueprint.ini";
static std::string bluepoint_file = "test_bp.json";

void Application_GetWindowProperties(ApplicationWindowProperty& property)
{
    property.name = "BlueprintSDK Test";
    property.docking = false;
    property.resizable = false;
    property.full_size = true;
    property.auto_merge = false;
}

void Application_SetupContext(ImGuiContext* ctx)
{
    if (!ctx)
        return;
#ifdef USE_BOOKMARK
    ImGuiSettingsHandler bookmark_ini_handler;
    bookmark_ini_handler.TypeName = "BookMark";
    bookmark_ini_handler.TypeHash = ImHashStr("BookMark");
    bookmark_ini_handler.ReadOpenFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) -> void*
    {
        return ImGuiFileDialog::Instance();
    };
    bookmark_ini_handler.ReadLineFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) -> void
    {
        IGFD::FileDialog * dialog = (IGFD::FileDialog *)entry;
        dialog->DeserializeBookmarks(line);
    };
    bookmark_ini_handler.WriteAllFn = [](ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
    {
        ImGuiContext& g = *ctx;
        out_buf->reserve(out_buf->size() + g.SettingsWindows.size() * 6); // ballpark reserve
        auto bookmark = ImGuiFileDialog::Instance()->SerializeBookmarks();
        out_buf->appendf("[%s][##%s]\n", handler->TypeName, handler->TypeName);
        out_buf->appendf("%s\n", bookmark.c_str());
        out_buf->append("\n");
    };
    ctx->SettingsHandlers.push_back(bookmark_ini_handler);
#endif
}

void Application_Initialize(void** handle)
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ini_file.c_str();
    BluePrint::BluePrintUI * UI = new BluePrint::BluePrintUI();
    UI->Initialize(bluepoint_file.c_str(), nullptr);
    *handle = UI;
#ifdef USE_THUMBNAILS
    ImGuiFileDialog::Instance()->SetCreateThumbnailCallback([](IGFD_Thumbnail_Info *vThumbnail_Info) -> void
    {
        if (vThumbnail_Info && 
            vThumbnail_Info->isReadyToUpload && 
            vThumbnail_Info->textureFileDatas)
        {
            auto texture = ImGui::ImCreateTexture(vThumbnail_Info->textureFileDatas, vThumbnail_Info->textureWidth, vThumbnail_Info->textureHeight);
            vThumbnail_Info->textureID = (void*)texture;
            delete[] vThumbnail_Info->textureFileDatas;
            vThumbnail_Info->textureFileDatas = nullptr;

            vThumbnail_Info->isReadyToUpload = false;
            vThumbnail_Info->isReadyToDisplay = true;
        }
    });
    ImGuiFileDialog::Instance()->SetDestroyThumbnailCallback([](IGFD_Thumbnail_Info* vThumbnail_Info)
    {
        if (vThumbnail_Info && vThumbnail_Info->textureID)
        {
            ImTextureID texID = (ImTextureID)vThumbnail_Info->textureID;
            ImGui::ImDestroyTexture(texID);
        }
    });
#endif
}

void Application_Finalize(void** handle)
{
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)*handle;
    if (!UI)
        return;
    UI->Finalize();
    delete UI;
}

void Application_DropFromSystem(std::vector<std::string>& drops)
{

}

bool Application_Frame(void * handle, bool app_will_quit)
{
    BluePrint::BluePrintUI * UI = (BluePrint::BluePrintUI *)handle;
    if (!UI)
        return true;
#ifdef USE_THUMBNAILS
	ImGuiFileDialog::Instance()->ManageGPUThumbnails();
#endif
    return UI->Frame() || app_will_quit;
}