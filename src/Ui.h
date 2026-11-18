#pragma once

#include <afxwin.h>
#include <afxcmn.h>

#include "Scheduling.h"

#include <charconv>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

namespace ui {

inline CString Wide(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int size = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), size, nullptr, 0);
    if (length == 0) {
        return {};
    }
    CString result;
    wchar_t* buffer = result.GetBuffer(length);
    MultiByteToWideChar(CP_UTF8, 0, text.data(), size, buffer, length);
    result.ReleaseBuffer(length);
    return result;
}

inline std::string Text(CWnd& window, int id) {
    CString value;
    window.GetDlgItemText(id, value);
    if (value.IsEmpty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                          value.GetLength(), nullptr, 0, nullptr, nullptr);
    if (length == 0) {
        throw std::runtime_error("输入中含有无法转换的字符。");
    }
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, value.GetLength(),
                        result.data(), length, nullptr, nullptr);
    return result;
}

inline int PositiveNumber(CWnd& window, int id, int maximum) {
    const auto text = schedule::Trim(Text(window, id));
    int value = 0;
    const auto end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end && value >= 1 && value <= maximum
        ? value : 0;
}

inline void Error(CWnd& window, LPCTSTR text) {
    window.MessageBox(text, L"保安排班系统", MB_OK | MB_ICONWARNING);
}

template <typename Action>
void Run(CWnd& window, Action action) {
    try {
        action();
    } catch (const std::exception& error) {
        Error(window, Wide(error.what()));
    }
}

inline void ComboAdd(CWnd& window, int id, LPCTSTR text) {
    window.SendDlgItemMessage(id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
}

inline void ComboSelect(CWnd& window, int id, int index) {
    window.SendDlgItemMessage(id, CB_SETCURSEL, static_cast<WPARAM>(index));
}

inline int ComboIndex(CWnd& window, int id) {
    return static_cast<int>(window.SendDlgItemMessage(id, CB_GETCURSEL));
}

inline void AddDays(CWnd& window, int id) {
    for (int day = 1; day <= schedule::N; ++day) {
        ComboAdd(window, id, Wide(schedule::days[day]));
    }
}

inline std::string DayName(int day) {
    return day >= 1 && day <= schedule::N ? schedule::days[day] : "未记录";
}

inline std::string WeekName(int week) { return "第 " + std::to_string(week) + " 周"; }

inline std::string GuardLabel(const schedule::Guard& guard) {
    return guard.name + "（" + guard.id + "）";
}

inline std::string RosterName(const schedule::Roster& guards, int index) {
    const auto& guard = guards[index];
    for (const auto& other : guards) {
        if (other.id != guard.id && other.name == guard.name) {
            return GuardLabel(guard);
        }
    }
    return guard.name;
}

inline std::string RestText(const schedule::Rota& rest, const schedule::Roster& guards) {
    std::string text;
    for (int i = 0; i < schedule::N; ++i) {
        if (i != 0) {
            text += "　";
        }
        text += RosterName(guards, i) + " · " + DayName(rest[i]);
    }
    return text;
}

inline std::string ReasonsText(const schedule::Reasons& reasons, const schedule::Roster& guards,
                               bool includeNames) {
    std::string text;
    for (const auto reason : schedule::reasonPriority) {
        std::string names;
        for (int guard = 0; guard < schedule::N; ++guard) {
            if (reasons[guard] == reason) {
                if (!names.empty()) {
                    names += "、";
                }
                names += RosterName(guards, guard);
            }
        }
        if (names.empty()) {
            continue;
        }
        if (!text.empty()) {
            text += includeNames ? "　" : " > ";
        }
        text += schedule::ReasonName(reason);
        if (includeNames) {
            text += " · " + names;
        }
    }
    return text;
}

inline std::string ReasonSuffix(schedule::SpecialReason reason) {
    return reason == schedule::SpecialReason::none
        ? std::string{} : std::string("（") + schedule::ReasonName(reason) + "）";
}

struct Column {
    LPCTSTR title;
    int percentage;
};

inline void SetupList(CListCtrl& list, std::initializer_list<Column> columns) {
    list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    CRect rect;
    list.GetClientRect(&rect);
    const int width = rect.Width() - GetSystemMetrics(SM_CXVSCROLL);
    int index = 0;
    for (const auto& column : columns) {
        list.InsertColumn(index++, column.title, LVCFMT_LEFT, width * column.percentage / 100);
    }
}

inline int AddRow(CListCtrl& list, std::initializer_list<CString> values) {
    const int row = list.InsertItem(list.GetItemCount(), *values.begin());
    int column = 0;
    for (const auto& value : values) {
        if (column != 0) {
            list.SetItemText(row, column, value);
        }
        ++column;
    }
    return row;
}

inline int SelectedRow(CListCtrl& list) { return list.GetNextItem(-1, LVNI_SELECTED); }

inline void SelectRow(CListCtrl& list, int row) {
    list.SetItemState(row, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
    list.EnsureVisible(row, FALSE);
}

inline bool SelectionChanged(NMHDR* notification) {
    const auto& change = *reinterpret_cast<NMLISTVIEW*>(notification);
    return (change.uChanged & LVIF_STATE) &&
           ((change.uOldState ^ change.uNewState) & LVIS_SELECTED);
}

inline int AddGuard(CListCtrl& list, const schedule::Guard& guard) {
    return AddRow(list, {Wide(guard.name), Wide(guard.id), Wide(guard.phone), Wide(guard.location)});
}

inline void SetupGuardList(CListCtrl& list) {
    SetupList(list, {{L"姓名", 16}, {L"编号", 20}, {L"电话", 28}, {L"岗位 / 住所", 36}});
}

} // namespace ui
