#pragma once

#include "Ui.h"
#include "Repository.h"

#include <optional>

namespace ui {

struct RestChange {
    int guard = 0;
    int day = 0;
    schedule::SpecialReason reason = schedule::SpecialReason::none;
    std::string note;
};

std::optional<std::string> ShowLogin(const schedule::Repository& repository);
std::optional<schedule::Draft> NewRecord(CWnd& parent, const schedule::Roster& guards,
                                         const schedule::Rota* generated = nullptr);
std::optional<schedule::Rota> GenerateRota(CWnd& parent, const schedule::Roster& guards);
std::optional<RestChange> EditRestDay(CWnd& parent, const schedule::Roster& guards,
                                      const schedule::Record& record);
void ShowTodayDuty(CWnd& parent, const schedule::Roster& guards,
                   const std::vector<schedule::Record>& records);
void ShowGuardInfo(CWnd& parent, const schedule::Roster& guards,
                   const std::vector<schedule::Record>& records);

} // namespace ui
