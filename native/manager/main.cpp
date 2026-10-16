// SPDX-License-Identifier: GPL-3.0-or-later
// Native, per-user policy editor. NEVER accessible via Native Messaging IPC.
#include "../src/policy_store.h"
#include "../src/export_storage.h"
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/textdlg.h>
#include <wx/listctrl.h>
#include <wx/stattext.h>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <exception>
#include <vector>

namespace policy = lagosystem::policy;
namespace {
constexpr const char* capabilities[] = {
    "devices.list", "devices.paired", "devices.pair",
    "devices.unpair", "devices.probe", "devices.testPrint",
    "exports.save"
};
std::string Convert(const wxString& value) {
    return value.ToStdString(wxConvUTF8);
}
wxString Display(const std::string& value) {
    return wxString::FromUTF8(value);
}
bool Confirm(wxWindow* parent, const wxString& text) {
    return wxMessageBox(text + "\n\nGranting access can expose local resources to scripts "
        "running on that website, including injected scripts.",
        "Confirm local permission change", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, parent) == wxYES;
}
class ManagerFrame final : public wxFrame {
public:
    ManagerFrame() : wxFrame(nullptr, wxID_ANY, "Lagosystem Bridge Manager",
                             wxDefaultPosition, wxSize(1040, 750)) {
        wxPanel* panel = new wxPanel(this);
        wxBoxSizer* root = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* top = new wxBoxSizer(wxHORIZONTAL);
        sites_ = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(290, 400));
        top->Add(sites_, 0, wxEXPAND | wxALL, 8);

        wxBoxSizer* details = new wxBoxSizer(wxVERTICAL);
        title_ = new wxStaticText(panel, wxID_ANY, "Choose a website");
        details->Add(title_, 0, wxEXPAND | wxALL, 8);
        wxBoxSizer* siteActions = new wxBoxSizer(wxHORIZONTAL);
        AddButton(panel, siteActions, "Add website", [this] { AddSite(); });
        AddButton(panel, siteActions, "Rename", [this] { Rename(); });
        AddButton(panel, siteActions, "Change URL", [this] { ChangeUrl(); });
        AddButton(panel, siteActions, "Enable / Disable", [this] { Toggle(); });
        AddButton(panel, siteActions, "Remove", [this] { Remove(); });
        details->Add(siteActions, 0, wxEXPAND);

        details->Add(new wxStaticText(panel, wxID_ANY,
             "Per-website capabilities (all denied until explicitly granted):"),
             0, wxTOP | wxLEFT, 8);
        grants_ = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(570, 140));
        details->Add(grants_, 1, wxEXPAND | wxALL, 8);
        wxBoxSizer* capActions = new wxBoxSizer(wxHORIZONTAL);
        AddButton(panel, capActions, "Grant resource", [this] { Grant(); });
        AddButton(panel, capActions, "Revoke selected", [this] { Revoke(); });
        details->Add(capActions);

        details->Add(new wxStaticText(panel, wxID_ANY,
            "Export-only folders (write a NEW XML/PDF; no browse, read, overwrite or delete):"),
            0, wxTOP | wxLEFT, 8);
        folders_ = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(570, 125));
        details->Add(folders_, 1, wxEXPAND | wxALL, 8);
        wxBoxSizer* folderActions = new wxBoxSizer(wxHORIZONTAL);
        AddButton(panel, folderActions, "Allow folder", [this] { GrantFolder(); });
        AddButton(panel, folderActions, "Revoke folder", [this] { RevokeFolder(); });
        details->Add(folderActions);
        top->Add(details, 1, wxEXPAND | wxALL, 8);
        root->Add(top, 1, wxEXPAND);

        root->Add(new wxStaticText(panel, wxID_ANY,
            "Recent audit events — grants are never changed by a browser message"),
            0, wxLEFT | wxTOP, 12);
        audit_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 140),
                               wxLC_REPORT | wxLC_SINGLE_SEL);
        audit_->InsertColumn(0, "When", wxLIST_FORMAT_LEFT, 170);
        audit_->InsertColumn(1, "Website", wxLIST_FORMAT_LEFT, 240);
        audit_->InsertColumn(2, "Action", wxLIST_FORMAT_LEFT, 200);
        audit_->InsertColumn(3, "Resource", wxLIST_FORMAT_LEFT, 240);
        audit_->InsertColumn(4, "Result", wxLIST_FORMAT_LEFT, 85);
        root->Add(audit_, 0, wxEXPAND | wxALL, 10);
        AddButton(panel, root, "Refresh", [this] { RefreshAll(); });
        panel->SetSizer(root);

        sites_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { RefreshDetails(); });
        RefreshAll();
        Centre();
    }
private:
    wxListBox* sites_{};
    wxListBox* grants_{};
    wxListBox* folders_{};
    wxListCtrl* audit_{};
    wxStaticText* title_{};
    std::vector<policy::Site> cachedSites_;
    std::vector<policy::Grant> cachedGrants_;
    std::vector<policy::FolderGrant> cachedFolders_;

    template<typename Callable>
    void AddButton(wxWindow* parent, wxSizer* sizer, const wxString& text, Callable&& handler) {
        wxButton* button = new wxButton(parent, wxID_ANY, text);
        sizer->Add(button, 0, wxALL, 3);
        button->Bind(wxEVT_BUTTON, [this, handler](wxCommandEvent&) {
            try { handler(); }
            catch (const std::exception& ex) {
                wxMessageBox(Display(ex.what()), "Operation denied",
                             wxOK | wxICON_ERROR, this);
            }
        });
    }
    const policy::Site* Current() const {
        const int index = sites_->GetSelection();
        return index >= 0 && static_cast<std::size_t>(index) < cachedSites_.size()
            ? &cachedSites_[index] : nullptr;
    }
    void RefreshAll(const std::string& select = {}) {
        const std::string previous = select.empty() && Current() ? Current()->origin : select;
        cachedSites_ = policy::ListSites();
        sites_->Clear();
        int selected = wxNOT_FOUND;
        for (std::size_t index = 0; index < cachedSites_.size(); ++index) {
            const auto& item = cachedSites_[index];
            sites_->Append(Display((item.enabled ? "[ON] " : "[OFF] ") + item.name));
            if (item.origin == previous) selected = static_cast<int>(index);
        }
        if (selected != wxNOT_FOUND) sites_->SetSelection(selected);
        RefreshDetails();
        audit_->DeleteAllItems();
        for (const policy::AuditEvent& entry : policy::RecentAudit(100)) {
            const long index = audit_->InsertItem(audit_->GetItemCount(), Display(entry.date));
            audit_->SetItem(index, 1, Display(entry.origin));
            audit_->SetItem(index, 2, Display(entry.action));
            audit_->SetItem(index, 3, Display(entry.resource));
            audit_->SetItem(index, 4, entry.allowed ? "Allowed" : "Denied");
        }
    }
    void RefreshDetails() {
        cachedGrants_.clear();
        cachedFolders_.clear();
        grants_->Clear();
        folders_->Clear();
        const policy::Site* site = Current();
        if (!site) { title_->SetLabel("Choose a website to edit its permissions"); return; }
        title_->SetLabel(Display(site->name + " — " + site->url +
            (site->enabled ? " (active)" : " (disabled)")));
        cachedGrants_ = policy::ListGrants(site->origin);
        for (const auto& grant : cachedGrants_)
            grants_->Append(Display(grant.action + " | " + grant.resource));
        cachedFolders_ = policy::ListFolders(site->origin);
        for (const auto& folder : cachedFolders_)
            folders_->Append(Display(std::to_string(folder.id) + " | " + folder.directory.string()));
    }
    void AddSite() {
        wxTextEntryDialog name(this, "Friendly name:", "Add website");
        if (name.ShowModal() != wxID_OK) return;
        wxTextEntryDialog address(this, "HTTPS website URL (localhost HTTP permitted):", "Website URL");
        if (address.ShowModal() != wxID_OK) return;
        policy::AddSite(Convert(name.GetValue()), Convert(address.GetValue()), false);
        const auto origin = policy::NormalizeOrigin(Convert(address.GetValue()));
        RefreshAll(origin.value_or(""));
        wxMessageBox("The new website is DISABLED until enabled here, with no permissions.",
                     "Site added", wxOK | wxICON_INFORMATION, this);
    }
    void Rename() {
        const auto* site = Current();
        if (!site) return;
        const std::string key = site->origin;
        wxTextEntryDialog dialog(this, "Friendly name (does not change origin):",
                                 "Rename website", Display(site->name));
        if (dialog.ShowModal() == wxID_OK) {
            policy::RenameSite(key, Convert(dialog.GetValue()));
            RefreshAll(key);
        }
    }
    void ChangeUrl() {
        const auto* site = Current();
        if (!site) return;
        const std::string old = site->origin;
        wxTextEntryDialog dialog(this, "New URL. Changing origin REVOKES ALL grants:",
                                 "Change website", Display(site->url));
        if (dialog.ShowModal() != wxID_OK) return;
        const std::string url = Convert(dialog.GetValue());
        const auto next = policy::NormalizeOrigin(url);
        if (!next) throw std::invalid_argument("Invalid website URL");
        if (*next != old && !Confirm(this, "Change security origin? Existing device and folder grants will be deleted."))
            return;
        policy::ChangeUrl(old, url);
        RefreshAll(*next);
    }
    void Toggle() {
        const auto* site = Current();
        if (!site) return;
        const std::string origin = site->origin;
        const bool enable = !site->enabled;
        if (enable && !Confirm(this, "Enable this website to request the separately granted operations?"))
            return;
        policy::EnableSite(origin, enable);
        RefreshAll(origin);
    }
    void Remove() {
        const auto* site = Current();
        if (!site || !Confirm(this, "Permanently remove this website and all associated permissions?"))
            return;
        policy::DeleteSite(site->origin);
        RefreshAll();
    }
    void Grant() {
        const auto* site = Current();
        if (!site) return;
        const std::string origin = site->origin;
        wxArrayString choices;
        for (const char* action : capabilities) choices.Add(Display(action));
        wxSingleChoiceDialog choice(this, "Select the exact capability:", "Grant", choices);
        if (choice.ShowModal() != wxID_OK) return;
        const std::string action = Convert(choice.GetStringSelection());
        const bool serial = action.rfind("devices.", 0) == 0 &&
                             action != "devices.list" && action != "devices.paired";
        wxString resource = "*";
        if (serial) {
            wxTextEntryDialog device(this, "Device ID (COM3, /dev/ttyUSB0, /dev/cu.*):",
                                     "Allowed serial port");
            if (device.ShowModal() != wxID_OK) return;
            resource = device.GetValue();
        } else if (action == "exports.save") {
            wxArrayString exportMethods;
            exportMethods.Add("default");
            exportMethods.Add("picker");
            wxSingleChoiceDialog mode(this, "default = user exports directory; picker = user chooses every time",
                                      "Choose export method", exportMethods);
            if (mode.ShowModal() != wxID_OK) return;
            resource = mode.GetStringSelection();
        }
        if (!Confirm(this, "Grant " + Display(action) + " for " + resource +
            " to " + Display(origin) + "?")) return;
        policy::GrantAction(origin, action, Convert(resource));
        policy::Audit(origin, "manager.grant", Convert(resource), true);
        RefreshAll(origin);
    }
    void Revoke() {
        const auto* site = Current();
        const int selection = grants_->GetSelection();
        if (!site || selection < 0) return;
        const auto grant = cachedGrants_.at(static_cast<std::size_t>(selection));
        const std::string origin = site->origin;
        policy::RevokeAction(origin, grant.action, grant.resource);
        policy::Audit(origin, "manager.revoke", grant.resource, true);
        RefreshAll(origin);
    }
    void GrantFolder() {
        const auto* site = Current();
        if (!site) return;
        const std::string origin = site->origin;
        wxDirDialog dialog(this, "Select an EXPORT-ONLY folder (no file reads)", "",
                           wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) return;
        #ifdef _WIN32
        const std::filesystem::path selected = dialog.GetPath().ToStdWstring();
#else
        const std::filesystem::path selected = Convert(dialog.GetPath());
#endif
        if (!lagosystem::storage::IsExportDirectoryAllowed(selected))
            throw std::invalid_argument("This location is sensitive, protected or not writable.");
        if (!Confirm(this, "Allow new PDF/XML exports for " + Display(origin) +
            " in " + dialog.GetPath() + "?")) return;
        policy::AddFolder(origin, selected);
        policy::Audit(origin, "manager.folder.add", "[local folder]", true);
        RefreshAll(origin);
    }
    void RevokeFolder() {
        const auto* site = Current();
        const int selection = folders_->GetSelection();
        if (!site || selection < 0) return;
        const std::string origin = site->origin;
        policy::RemoveFolder(origin, cachedFolders_.at(static_cast<std::size_t>(selection)).id);
        policy::Audit(origin, "manager.folder.remove", "[local folder]", true);
        RefreshAll(origin);
    }
};
class Application final : public wxApp {
public:
    bool OnInit() override {
        try {
            auto* frame = new ManagerFrame();
            frame->Show();
            return true;
        } catch (const std::exception& ex) {
            wxMessageBox(Display(ex.what()), "Lagosystem Bridge Manager",
                         wxOK | wxICON_ERROR);
            return false;
        }
    }
};
}
wxIMPLEMENT_APP(Application);
