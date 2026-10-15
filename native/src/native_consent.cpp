// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_consent.h"
#ifdef _WIN32
#include <Windows.h>
#include <CommCtrl.h>
#include <ShObjIdl.h>
#include <string>
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Ole32.lib")
namespace {
std::wstring Utf16(const std::string& utf8) {
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, nullptr, 0);
    if (count <= 0) return {};
    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), -1, wide.data(), count);
    wide.pop_back();
    return wide;
}
}
namespace lagosystem::ui {
bool ConfirmDeviceAction(const std::string& title, const std::string& message) {
    const std::wstring heading = Utf16(title);
    const std::wstring detail = Utf16(message);
    if (heading.empty() || detail.empty()) return false;
    TASKDIALOGCONFIG dialog{};
    dialog.cbSize = sizeof(dialog);
    dialog.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
    dialog.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    dialog.pszWindowTitle = L"Local Device Bridge";
    dialog.pszMainInstruction = heading.c_str();
    dialog.pszContent = detail.c_str();
    dialog.pszMainIcon = TD_WARNING_ICON;
    dialog.nDefaultButton = IDNO;
    int choice = IDNO;
    if (TaskDialogIndirect(&dialog, &choice, nullptr, nullptr) != S_OK) return false;
    return choice == IDYES;
}

std::optional<std::filesystem::path> PickExportDirectory() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(init)) return std::nullopt;
    std::optional<std::filesystem::path> chosen;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)) &&
            SUCCEEDED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST))) {
            dialog->SetTitle(L"Select a folder for this XML/PDF export");
            if (SUCCEEDED(dialog->Show(nullptr))) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                        if (path != nullptr && *path != L'\0') chosen = std::filesystem::path(path);
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
        }
        dialog->Release();
    }
    CoUninitialize();
    return chosen;
}
}
#elif !defined(__APPLE__)
#ifdef LAGOSYSTEM_HAS_GTK3
#include <gtk/gtk.h>
namespace lagosystem::ui {
bool ConfirmDeviceAction(const std::string& title, const std::string& message) {
    if (!gtk_init_check(nullptr, nullptr)) return false;
    GtkWidget* dialog = gtk_message_dialog_new(nullptr, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
        GTK_BUTTONS_YES_NO, "%s", title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message.c_str());
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);
    const bool approved = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES;
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return approved;
}
std::optional<std::filesystem::path> PickExportDirectory() {
    if (!gtk_init_check(nullptr, nullptr)) return std::nullopt;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Choose a folder for this XML/PDF export", nullptr, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, nullptr);
    std::optional<std::filesystem::path> chosen;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (path != nullptr) {
            if (*path != '\0') chosen = std::filesystem::path(path);
            g_free(path);
        }
    }
    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();
    return chosen;
}
}
#else
namespace lagosystem::ui {
bool ConfirmDeviceAction(const std::string&, const std::string&) { return false; }
std::optional<std::filesystem::path> PickExportDirectory() { return std::nullopt; }
}
#endif
#endif
