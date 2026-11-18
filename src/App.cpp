#include "Dialogs.h"
#include "resource.h"

#include <limits>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using namespace schedule;
using namespace ui;

std::filesystem::path ProgramDirectory() {
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW(nullptr, path, _countof(path));
    if (length == 0 || length >= _countof(path)) {
        throw std::runtime_error("无法确定程序目录。");
    }
    return std::filesystem::path(std::wstring(path, length)).parent_path();
}

class DirectoryLock {
public:
    explicit DirectoryLock(const std::filesystem::path& directory) {
        const auto path = directory / "GuardScheduler.lock";
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                              nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_SHARING_VIOLATION) {
                throw std::runtime_error("此目录中的排班系统已在运行。");
            }
            throw std::runtime_error("无法锁定数据目录。请将程序放到具有读写权限的目录。");
        }
    }

    ~DirectoryLock() { CloseHandle(handle_); }
    DirectoryLock(const DirectoryLock&) = delete;
    DirectoryLock& operator=(const DirectoryLock&) = delete;

private:
    HANDLE handle_;
};

std::string DetailText(const Record& record, const Roster& guards) {
    std::string text = "编号 " + std::to_string(record.id) + "  ·  " + WeekName(record.week) +
                       "    生成 " + record.createTime + "  " + record.createUser;
    text += "\r\n最后修改 ";
    text += record.modifyTime.empty() ? "尚未修改" : record.modifyTime + "  " + record.modifyUser;
    text += "\r\n轮休 " + RestText(record.rest, guards);
    const auto reasons = ReasonsText(record.reasons, guards, true);
    text += "\r\n特殊情况 " + (reasons.empty() ? std::string("无") : reasons);
    text += "\r\n备注 " + (record.remark.empty() ? std::string("无") : record.remark);
    return text;
}

class MainDialog : public CDialog {
public:
    MainDialog(const Repository& repository, std::string user)
        : CDialog(IDD_MAIN), repository_(repository), user_(std::move(user)),
          guards_(repository.Guards()), records_(repository.LoadRecords()) {}

protected:
    void DoDataExchange(CDataExchange* exchange) override {
        CDialog::DoDataExchange(exchange);
        DDX_Control(exchange, IDC_RECORDS, list_);
    }

    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetDlgItemText(IDC_USER_LABEL, Wide("当前账号  " + user_));
        SetupList(list_, {{L"编号", 6}, {L"周次", 8}, {L"生成时间", 17}, {L"生成人", 9},
                          {L"修改时间", 17}, {L"修改人", 9}, {L"特殊情况", 18}, {L"备注", 16}});
        Refresh();
        return TRUE;
    }

    void OnOK() override {
        Run(*this, [&] {
            const int id = PositiveNumber(*this, IDC_RECORD_ID, (std::numeric_limits<int>::max)());
            if (id == 0) {
                Error(*this, L"请输入要查找的记录编号。");
                GetDlgItem(IDC_RECORD_ID)->SetFocus();
                return;
            }
            if (static_cast<size_t>(id) > records_.size()) {
                Error(*this, L"未找到该编号的排班记录。");
                GetDlgItem(IDC_RECORD_ID)->SetFocus();
                return;
            }
            SelectRow(list_, id - 1);
            list_.SetFocus();
        });
    }

    afx_msg void OnManual() {
        Run(*this, [&] {
            if (const auto draft = NewRecord(*this, guards_)) {
                SaveNew(*draft);
            }
        });
    }

    afx_msg void OnGenerate() {
        Run(*this, [&] {
            if (const auto rota = GenerateRota(*this, guards_)) {
                if (const auto draft = NewRecord(*this, guards_, &*rota)) {
                    SaveNew(*draft);
                }
            }
        });
    }

    afx_msg void OnModify() {
        Run(*this, [&] {
            const int row = SelectedRow(list_);
            if (row < 0) {
                return;
            }
            const auto change = EditRestDay(*this, guards_, records_[row]);
            if (!change) {
                return;
            }
            auto updated = records_;
            ModifyRecord(updated[row], change->guard, change->day, change->reason, change->note,
                         user_, LocalNow().timestamp);
            Commit(std::move(updated), row);
        });
    }

    afx_msg void OnDelete() {
        Run(*this, [&] {
            const int row = SelectedRow(list_);
            if (row < 0) {
                return;
            }
            const auto& record = records_[row];
            const auto prompt = "确认删除编号 " + std::to_string(record.id) + " 的排班？\r\n\r\n" +
                                WeekName(record.week) + "\r\n" + RestText(record.rest, guards_);
            if (!ConfirmDelete(prompt)) {
                return;
            }
            auto updated = records_;
            updated.erase(updated.begin() + row);
            SortAndRenumber(updated);
            const int nextRow = row < static_cast<int>(updated.size()) ? row : row - 1;
            Commit(std::move(updated), nextRow);
        });
    }

    afx_msg void OnDeleteAll() {
        Run(*this, [&] {
            if (records_.empty()) {
                return;
            }
            if (ConfirmDelete("将永久删除当前的 " + std::to_string(records_.size()) +
                              " 条排班记录。继续吗？") &&
                ConfirmDelete("再次确认删除全部排班记录？")) {
                Commit({}, -1);
            }
        });
    }

    afx_msg void OnToday() { Run(*this, [&] { ShowTodayDuty(*this, guards_, records_); }); }
    afx_msg void OnGuards() { Run(*this, [&] { ShowGuardInfo(*this, guards_, records_); }); }

    afx_msg void OnSelected(NMHDR* notification, LRESULT* result) {
        *result = 0;
        if (!refreshing_ && SelectionChanged(notification)) {
            RefreshSelection();
        }
    }

    DECLARE_MESSAGE_MAP()

private:
    bool ConfirmDelete(const std::string& prompt) {
        return MessageBox(Wide(prompt), L"删除排班", MB_YESNO | MB_ICONWARNING |
                          MB_DEFBUTTON2) == IDYES;
    }

    void SaveNew(const Draft& draft) {
        auto updated = records_;
        const int id = AddRecord(updated, draft, user_, LocalNow().timestamp);
        Commit(std::move(updated), id - 1);
    }

    void Commit(std::vector<Record> updated, int row) {
        repository_.SaveRecords(updated);
        records_ = std::move(updated);
        Refresh(row);
    }

    void Refresh(int row = -1) {
        refreshing_ = true;
        list_.SetRedraw(FALSE);
        list_.DeleteAllItems();
        for (const auto& record : records_) {
            const auto reasons = ReasonsText(record.reasons, guards_, false);
            AddRow(list_, {Wide(std::to_string(record.id)), Wide(WeekName(record.week)),
                           Wide(record.createTime), Wide(record.createUser),
                           Wide(record.modifyTime.empty() ? "—" : record.modifyTime),
                           Wide(record.modifyUser.empty() ? "—" : record.modifyUser),
                           Wide(reasons.empty() ? "—" : reasons),
                           Wide(record.remark.empty() ? "—" : record.remark)});
        }
        list_.SetRedraw(TRUE);
        if (row >= 0 && row < list_.GetItemCount()) {
            SelectRow(list_, row);
        }
        list_.Invalidate();
        refreshing_ = false;
        SetDlgItemText(IDC_RECORD_COUNT, Wide("共 " + std::to_string(records_.size()) + " 条记录"));
        GetDlgItem(IDC_DELETE_ALL)->EnableWindow(!records_.empty());
        GetDlgItem(IDOK)->EnableWindow(!records_.empty());
        GetDlgItem(IDC_RECORD_ID)->EnableWindow(!records_.empty());
        RefreshSelection();
    }

    void RefreshSelection() {
        const int row = SelectedRow(list_);
        const bool selected = row >= 0 && static_cast<size_t>(row) < records_.size();
        GetDlgItem(IDC_MODIFY)->EnableWindow(selected);
        GetDlgItem(IDC_DELETE)->EnableWindow(selected);
        if (selected) {
            SetDlgItemText(IDC_RECORD_DETAIL, Wide(DetailText(records_[row], guards_)));
        } else {
            SetDlgItemText(IDC_RECORD_DETAIL, records_.empty()
                ? L"暂无排班记录。" : L"选择一条记录，或按编号查找。");
        }
    }

    const Repository& repository_;
    const std::string user_;
    const Roster& guards_;
    std::vector<Record> records_;
    CListCtrl list_;
    bool refreshing_ = false;
};

BEGIN_MESSAGE_MAP(MainDialog, CDialog)
    ON_BN_CLICKED(IDC_MANUAL, &MainDialog::OnManual)
    ON_BN_CLICKED(IDC_GENERATE, &MainDialog::OnGenerate)
    ON_BN_CLICKED(IDC_MODIFY, &MainDialog::OnModify)
    ON_BN_CLICKED(IDC_DELETE, &MainDialog::OnDelete)
    ON_BN_CLICKED(IDC_DELETE_ALL, &MainDialog::OnDeleteAll)
    ON_BN_CLICKED(IDC_TODAY, &MainDialog::OnToday)
    ON_BN_CLICKED(IDC_GUARDS, &MainDialog::OnGuards)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_RECORDS, &MainDialog::OnSelected)
END_MESSAGE_MAP()

class GuardSchedulerApp : public CWinApp {
public:
    BOOL InitInstance() override {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
        if (!InitCommonControlsEx(&controls) || !CWinApp::InitInstance()) {
            return FALSE;
        }
        try {
            const auto directory = ProgramDirectory();
            const DirectoryLock lock(directory);
            const Repository repository(directory);
            if (const auto user = ShowLogin(repository)) {
                MainDialog dialog(repository, *user);
                m_pMainWnd = &dialog;
                dialog.DoModal();
                m_pMainWnd = nullptr;
            }
        } catch (const std::exception& error) {
            m_pMainWnd = nullptr;
            AfxMessageBox(Wide(error.what()), MB_OK | MB_ICONERROR);
        }
        return FALSE;
    }
};

GuardSchedulerApp app;

} // namespace
