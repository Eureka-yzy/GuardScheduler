#include "Dialogs.h"
#include "resource.h"

namespace ui {
namespace {

using namespace schedule;

void AddReasonOptions(CWnd& window, int id) {
    ComboAdd(window, id, Wide(ReasonName(SpecialReason::none)));
    for (const auto reason : reasonPriority) {
        ComboAdd(window, id, Wide(ReasonName(reason)));
    }
}

void SelectReason(CWnd& window, int id, SpecialReason reason) {
    for (size_t i = 0; i < reasonPriority.size(); ++i) {
        if (reasonPriority[i] == reason) {
            ComboSelect(window, id, static_cast<int>(i + 1));
            return;
        }
    }
    ComboSelect(window, id, 0);
}

SpecialReason ChosenReason(CWnd& window, int id) {
    const int index = ComboIndex(window, id);
    return index > 0 && static_cast<size_t>(index) <= reasonPriority.size()
        ? reasonPriority[index - 1] : SpecialReason::none;
}

class LoginDialog : public CDialog {
public:
    explicit LoginDialog(const Repository& repository)
        : CDialog(IDD_LOGIN), repository_(repository) {}

    std::string user;

protected:
    void OnOK() override {
        Run(*this, [&] {
            const auto name = Trim(Text(*this, IDC_USERNAME));
            if (!repository_.Authenticate(name, Text(*this, IDC_PASSWORD))) {
                Error(*this, L"账号或密码错误。");
                GetDlgItem(IDC_PASSWORD)->SetFocus();
                return;
            }
            user = name;
            EndDialog(IDOK);
        });
    }

    afx_msg void OnRegister() {
        Run(*this, [&] {
            const auto name = Text(*this, IDC_USERNAME);
            const auto password = Text(*this, IDC_PASSWORD);
            switch (repository_.Register(name, password)) {
            case Registration::invalid:
                Error(*this, L"账号和密码不能为空，也不能包含空格、制表符或冒号。");
                break;
            case Registration::duplicate:
                Error(*this, L"该账号已存在。");
                break;
            case Registration::success:
                MessageBox(L"注册成功，请再次输入密码登录。", L"注册", MB_OK | MB_ICONINFORMATION);
                SetDlgItemText(IDC_PASSWORD, L"");
                GetDlgItem(IDC_PASSWORD)->SetFocus();
                break;
            }
        });
    }

    DECLARE_MESSAGE_MAP()

private:
    const Repository& repository_;
};

BEGIN_MESSAGE_MAP(LoginDialog, CDialog)
    ON_BN_CLICKED(IDC_REGISTER, &LoginDialog::OnRegister)
END_MESSAGE_MAP()

class NewRecordDialog : public CDialog {
public:
    NewRecordDialog(CWnd& parent, const Roster& guards, const Rota* generated)
        : CDialog(IDD_NEW_RECORD, &parent), guards_(guards), manual_(generated == nullptr) {
        if (generated) {
            draft.rest = *generated;
        }
    }

    Draft draft;

protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        if (!manual_) {
            SetWindowText(L"自动排班 · 保存方案");
        }
        for (int guard = 0; guard < N; ++guard) {
            SetDlgItemText(IDC_NEW_GUARD_1 + guard, Wide(GuardLabel(guards_[guard])));
            const int id = IDC_REST_1 + guard;
            AddDays(*this, id);
            AddReasonOptions(*this, IDC_REASON_1 + guard);
            SelectReason(*this, IDC_REASON_1 + guard, SpecialReason::none);
            if (!manual_) {
                ComboSelect(*this, id, draft.rest[guard] - 1);
                GetDlgItem(id)->EnableWindow(FALSE);
            }
        }
        return TRUE;
    }

    void OnOK() override {
        Run(*this, [&] {
            draft.week = PositiveNumber(*this, IDC_WEEK, 999);
            if (draft.week == 0) {
                Error(*this, L"请输入 1 到 999 之间的排班周次。");
                GetDlgItem(IDC_WEEK)->SetFocus();
                return;
            }
            int used = 0;
            for (int guard = 0; guard < N; ++guard) {
                const int control = IDC_REST_1 + guard;
                const int day = ComboIndex(*this, control) + 1;
                if (day == 0 || (used & DayBit(day))) {
                    Error(*this, Wide(day == 0 ? "请选择保安 " + RosterName(guards_, guard) + " 的休息日。"
                                               : days[day] + " 已有保安休息，请重新选择。"));
                    GetDlgItem(control)->SetFocus();
                    return;
                }
                used |= DayBit(day);
                draft.rest[guard] = day;
                draft.reasons[guard] = ChosenReason(*this, IDC_REASON_1 + guard);
            }
            draft.remark = Text(*this, IDC_REMARK);
            if (manual_) {
                auto preview = WeekName(draft.week) + "的轮休排班\r\n\r\n" + RestText(draft.rest, guards_);
                const auto reasons = ReasonsText(draft.reasons, guards_, true);
                if (!reasons.empty()) {
                    preview += "\r\n特殊情况 " + reasons;
                }
                preview += "\r\n\r\n确认新增这条记录吗？";
                if (MessageBox(Wide(preview), L"确认排班", MB_YESNO | MB_ICONQUESTION |
                               MB_DEFBUTTON2) != IDYES) {
                    return;
                }
            }
            EndDialog(IDOK);
        });
    }

private:
    const Roster& guards_;
    const bool manual_;
};

class PreferencesDialog : public CDialog {
public:
    PreferencesDialog(CWnd& parent, const Roster& guards)
        : CDialog(IDD_PREFERENCES, &parent), guards_(guards) {}

    std::vector<Rota> solutions;

protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        for (int guard = 0; guard < N; ++guard) {
            SetDlgItemText(IDC_PREF_GUARD_1 + guard, Wide(GuardLabel(guards_[guard])));
        }
        return TRUE;
    }

    void OnOK() override {
        Run(*this, [&] {
            Masks masks{};
            for (int guard = 0; guard < N; ++guard) {
                for (int day = 1; day <= N; ++day) {
                    if (IsDlgButtonChecked(IDC_PREF_11 + guard * N + day - 1) == BST_CHECKED) {
                        masks[guard] |= DayBit(day);
                    }
                }
            }
            solutions = Solve(masks);
            if (!solutions.empty()) {
                EndDialog(IDOK);
                return;
            }
            const auto diagnostic = Diagnose(masks);
            std::string message = "当前偏好无法让所有人各休一天，并让七个休息日互不相同。\r\n";
            if (!diagnostic.guardsWithoutChoices.empty()) {
                message += "\r\n未选择休息日的保安";
                for (const int guard : diagnostic.guardsWithoutChoices) {
                    message += "　" + RosterName(guards_, guard);
                }
                message += "\r\n";
            }
            if (!diagnostic.unmatchedDays.empty()) {
                message += "\r\n以下日期未能分配休息人员\r\n";
                for (const auto& item : diagnostic.unmatchedDays) {
                    message += "· " + days[item.day] + "　";
                    message += item.candidates == 0
                        ? "没有人选择该日\r\n"
                        : std::to_string(item.candidates) + " 名候选保安已分配至其他日期\r\n";
                }
            }
            message += "\r\n请调整勾选后重新生成。";
            Error(*this, Wide(message));
        });
    }

private:
    const Roster& guards_;
};

class SolutionsDialog : public CDialog {
public:
    SolutionsDialog(CWnd& parent, const Roster& guards, const std::vector<Rota>& solutions)
        : CDialog(IDD_SOLUTIONS, &parent), guards_(guards), solutions_(solutions) {}

    int selected = -1;

protected:
    void DoDataExchange(CDataExchange* exchange) override {
        CDialog::DoDataExchange(exchange);
        DDX_Control(exchange, IDC_SOLUTIONS, list_);
    }

    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetupList(list_, {{L"编号", 9}, {Wide(RosterName(guards_, 0)), 13}, {Wide(RosterName(guards_, 1)), 13},
                          {Wide(RosterName(guards_, 2)), 13}, {Wide(RosterName(guards_, 3)), 13},
                          {Wide(RosterName(guards_, 4)), 13}, {Wide(RosterName(guards_, 5)), 13},
                          {Wide(RosterName(guards_, 6)), 13}});
        SetDlgItemText(IDC_SOLUTION_COUNT, Wide("共 " + std::to_string(solutions_.size()) +
                                               " 个可行方案 · 各列为该保安的休息日"));
        list_.SetRedraw(FALSE);
        for (size_t i = 0; i < solutions_.size(); ++i) {
            const auto& a = solutions_[i];
            AddRow(list_, {Wide(std::to_string(i + 1)), Wide(days[a[0]]), Wide(days[a[1]]),
                           Wide(days[a[2]]), Wide(days[a[3]]), Wide(days[a[4]]),
                           Wide(days[a[5]]), Wide(days[a[6]])});
        }
        list_.SetRedraw(TRUE);
        return TRUE;
    }

    void OnOK() override {
        selected = SelectedRow(list_);
        if (selected < 0) {
            Error(*this, L"请在列表中选择一个方案。");
            return;
        }
        EndDialog(IDOK);
    }

    afx_msg void OnLocate() {
        Run(*this, [&] {
            const int number = PositiveNumber(*this, IDC_SOLUTION_ID,
                                              static_cast<int>(solutions_.size()));
            if (number == 0) {
                Error(*this, L"请输入列表中存在的方案编号。");
                return;
            }
            SelectRow(list_, number - 1);
            list_.SetFocus();
        });
    }

    DECLARE_MESSAGE_MAP()

private:
    const Roster& guards_;
    const std::vector<Rota>& solutions_;
    CListCtrl list_;
};

BEGIN_MESSAGE_MAP(SolutionsDialog, CDialog)
    ON_BN_CLICKED(IDC_LOCATE_SOLUTION, &SolutionsDialog::OnLocate)
END_MESSAGE_MAP()

class ModifyDialog : public CDialog {
public:
    ModifyDialog(CWnd& parent, const Roster& guards, const Record& record)
        : CDialog(IDD_MODIFY, &parent), guards_(guards), record_(record) {}

    RestChange change;

protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetDlgItemText(IDC_EDIT_CONTEXT, Wide("记录 " + std::to_string(record_.id) + " · " +
                                             WeekName(record_.week)));
        for (const auto& guard : guards_) {
            ComboAdd(*this, IDC_EDIT_GUARD, Wide(GuardLabel(guard)));
        }
        AddDays(*this, IDC_EDIT_DAY);
        AddReasonOptions(*this, IDC_EDIT_REASON);
        ComboSelect(*this, IDC_EDIT_GUARD, 0);
        OnGuardChanged();
        return TRUE;
    }

    void OnOK() override {
        Run(*this, [&] {
            change.guard = ComboIndex(*this, IDC_EDIT_GUARD);
            change.day = ComboIndex(*this, IDC_EDIT_DAY) + 1;
            if (change.guard < 0 || change.day == 0) {
                Error(*this, L"请选择保安及其新休息日。");
                return;
            }
            change.reason = ChosenReason(*this, IDC_EDIT_REASON);
            change.note = Text(*this, IDC_REMARK);
            EndDialog(IDOK);
        });
    }

    afx_msg void OnGuardChanged() {
        const int guard = ComboIndex(*this, IDC_EDIT_GUARD);
        if (guard >= 0) {
            ComboSelect(*this, IDC_EDIT_DAY, record_.rest[guard] - 1);
            SelectReason(*this, IDC_EDIT_REASON, record_.reasons[guard]);
        }
        OnDayChanged();
    }

    afx_msg void OnDayChanged() {
        const int guard = ComboIndex(*this, IDC_EDIT_GUARD);
        if (guard < 0) {
            return;
        }
        const int day = ComboIndex(*this, IDC_EDIT_DAY) + 1;
        const int oldDay = record_.rest[guard];
        std::string text = "当前休息日　" + DayName(oldDay);
        if (day >= 1) {
            auto preview = record_.rest;
            const int other = ChangeRestDay(preview, guard, day);
            if (other >= 0) {
                text += "\r\n交换后　" + RosterName(guards_, guard) + " · " + DayName(day) + "　　" +
                        RosterName(guards_, other) + " · " + DayName(oldDay);
            } else if (day != oldDay) {
                text += "\r\n调整后　" + DayName(day);
            }
        }
        SetDlgItemText(IDC_SWAP_PREVIEW, Wide(text));
    }

    DECLARE_MESSAGE_MAP()

private:
    const Roster& guards_;
    const Record& record_;
};

BEGIN_MESSAGE_MAP(ModifyDialog, CDialog)
    ON_CBN_SELCHANGE(IDC_EDIT_GUARD, &ModifyDialog::OnGuardChanged)
    ON_CBN_SELCHANGE(IDC_EDIT_DAY, &ModifyDialog::OnDayChanged)
END_MESSAGE_MAP()

class DutyDialog : public CDialog {
public:
    DutyDialog(CWnd& parent, const Roster& guards, const std::vector<Record>& records)
        : CDialog(IDD_TODAY, &parent), guards_(guards), records_(records), date_(LocalNow()),
          weeks_(AvailableWeeks(records)) {}

protected:
    void DoDataExchange(CDataExchange* exchange) override {
        CDialog::DoDataExchange(exchange);
        DDX_Control(exchange, IDC_DUTY_LIST, list_);
    }

    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetupGuardList(list_);
        SetDlgItemText(IDC_DUTY_DATE, Wide(date_.timestamp + "  ·  " + DayName(date_.weekday) +
                                          "  ·  系统本周 " + WeekName(date_.week)));
        for (const int week : weeks_) {
            ComboAdd(*this, IDC_DUTY_WEEK, Wide(WeekName(week)));
        }
        if (records_.size() == 1) {
            ComboSelect(*this, IDC_DUTY_WEEK, 0);
        }
        OnWeekChanged();
        return TRUE;
    }

    afx_msg void OnWeekChanged() {
        list_.DeleteAllItems();
        const int index = ComboIndex(*this, IDC_DUTY_WEEK);
        if (index < 0) {
            SetDlgItemText(IDC_TODAY_REST, L"");
            SetDlgItemText(IDC_DUTY_SUMMARY, weeks_.empty() ? L"暂无排班记录。" : L"请选择参考周次。");
            return;
        }
        const auto* record = LatestForWeek(records_, weeks_[index]);
        const auto duty = OnDuty(record->rest, date_.weekday);
        for (const int guard : duty) {
            AddGuard(list_, guards_[guard]);
        }
        std::string resting;
        for (int guard = 0; guard < N; ++guard) {
            if (record->rest[guard] == date_.weekday) {
                if (!resting.empty()) {
                    resting += "、";
                }
                resting += RosterName(guards_, guard) + ReasonSuffix(record->reasons[guard]);
            }
        }
        SetDlgItemText(IDC_TODAY_REST, Wide(resting.empty() ? "休息人员  无" : "休息人员  " + resting));
        SetDlgItemText(IDC_DUTY_SUMMARY, Wide("使用记录 " + std::to_string(record->id) + "  ·  " +
                                             std::to_string(duty.size()) + " 人值班，" +
                                             std::to_string(N - duty.size()) + " 人休息"));
    }

    DECLARE_MESSAGE_MAP()

private:
    const Roster& guards_;
    const std::vector<Record>& records_;
    const LocalDate date_;
    const std::vector<int> weeks_;
    CListCtrl list_;
};

BEGIN_MESSAGE_MAP(DutyDialog, CDialog)
    ON_CBN_SELCHANGE(IDC_DUTY_WEEK, &DutyDialog::OnWeekChanged)
END_MESSAGE_MAP()

class GuardDialog : public CDialog {
public:
    GuardDialog(CWnd& parent, const Roster& guards, const std::vector<Record>& records)
        : CDialog(IDD_GUARDS, &parent), guards_(guards), records_(records), date_(LocalNow()) {}

protected:
    void DoDataExchange(CDataExchange* exchange) override {
        CDialog::DoDataExchange(exchange);
        DDX_Control(exchange, IDC_GUARD_LIST, list_);
    }

    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetupGuardList(list_);
        Filter("");
        return TRUE;
    }

    void OnOK() override {
        Run(*this, [&] { Filter(Trim(Text(*this, IDC_GUARD_NAME))); });
    }

    afx_msg void OnSelected(NMHDR* notification, LRESULT* result) {
        *result = 0;
        if (SelectionChanged(notification)) {
            ShowRecentWeeks();
        }
    }

    DECLARE_MESSAGE_MAP()

private:
    void Filter(const std::string& name) {
        list_.DeleteAllItems();
        for (int guard = 0; guard < N; ++guard) {
            if (name.empty() || guards_[guard].name == name) {
                const int row = AddGuard(list_, guards_[guard]);
                if (row >= 0) {
                    list_.SetItemData(row, guard);
                }
            }
        }
        if (list_.GetItemCount() == 1) {
            SelectRow(list_, 0);
        } else if (list_.GetItemCount() == 0) {
            SetDlgItemText(IDC_GUARD_DETAIL, L"没有找到同名保安。请核对姓名或清空后重新查找。");
        } else {
            ShowRecentWeeks();
        }
    }

    void ShowRecentWeeks() {
        const int row = SelectedRow(list_);
        if (row < 0) {
            SetDlgItemText(IDC_GUARD_DETAIL, L"选择一名保安查看最近四周的值班安排。");
            return;
        }
        const auto guard = list_.GetItemData(row);
        std::string detail = GuardLabel(guards_[guard]) + "  ·  " + date_.timestamp +
                             "  ·  系统本周 " + WeekName(date_.week) + "\r\n";
        for (int week = date_.week - 3; week <= date_.week; ++week) {
            if (week < 1) {
                continue;
            }
            detail += "\r\n" + WeekName(week) + "　";
            const auto* record = LatestForWeek(records_, week);
            if (!record) {
                detail += "未找到排班记录";
                continue;
            }
            const int day = record->rest[guard];
            if (day < 1 || day > N) {
                detail += "休息日未记录，请检查排班表文件";
                continue;
            }
            detail += "休息 " + DayName(day) + ReasonSuffix(record->reasons[guard]) + "   值班 ";
            std::string separator;
            for (int workingDay = 1; workingDay <= N; ++workingDay) {
                if (workingDay != day) {
                    detail += separator + days[workingDay];
                    separator = "、";
                }
            }
            detail += "（记录 " + std::to_string(record->id) + "）";
        }
        SetDlgItemText(IDC_GUARD_DETAIL, Wide(detail));
    }

    const Roster& guards_;
    const std::vector<Record>& records_;
    const LocalDate date_;
    CListCtrl list_;
};

BEGIN_MESSAGE_MAP(GuardDialog, CDialog)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_GUARD_LIST, &GuardDialog::OnSelected)
END_MESSAGE_MAP()

} // namespace

std::optional<std::string> ShowLogin(const Repository& repository) {
    LoginDialog dialog(repository);
    if (dialog.DoModal() == IDOK) {
        return dialog.user;
    }
    return std::nullopt;
}

std::optional<schedule::Draft> NewRecord(CWnd& parent, const Roster& guards, const Rota* generated) {
    NewRecordDialog dialog(parent, guards, generated);
    if (dialog.DoModal() == IDOK) {
        return dialog.draft;
    }
    return std::nullopt;
}

std::optional<schedule::Rota> GenerateRota(CWnd& parent, const Roster& guards) {
    PreferencesDialog preferences(parent, guards);
    if (preferences.DoModal() != IDOK) {
        return std::nullopt;
    }
    SolutionsDialog choices(parent, guards, preferences.solutions);
    if (choices.DoModal() == IDOK) {
        return preferences.solutions[choices.selected];
    }
    return std::nullopt;
}

std::optional<RestChange> EditRestDay(CWnd& parent, const Roster& guards, const Record& record) {
    ModifyDialog dialog(parent, guards, record);
    if (dialog.DoModal() == IDOK) {
        return dialog.change;
    }
    return std::nullopt;
}

void ShowTodayDuty(CWnd& parent, const Roster& guards, const std::vector<Record>& records) {
    DutyDialog(parent, guards, records).DoModal();
}

void ShowGuardInfo(CWnd& parent, const Roster& guards, const std::vector<Record>& records) {
    GuardDialog(parent, guards, records).DoModal();
}

} // namespace ui
