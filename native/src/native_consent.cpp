// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_consent.h"
#ifdef _WIN32
#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#pragma comment(lib, "Comctl32.lib")
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
}
#else
namespace lagosystem::ui {
bool ConfirmDeviceAction(const std::string&, const std::string&) { return false; }
}
#endif
#endif
