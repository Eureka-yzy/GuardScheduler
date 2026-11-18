#pragma once

#include <array>
#include <ctime>
#include <string>
#include <vector>

namespace schedule {

constexpr int N = 7;
using Rota = std::array<int, N>;
using Masks = std::array<int, N>;
constexpr int DayBit(int day) { return 1 << (day - 1); }

enum class SpecialReason { none = 0, medical = 1, documents = 2, travel = 3 };
using Reasons = std::array<SpecialReason, N>;
constexpr std::array<SpecialReason, 3> reasonPriority{
    SpecialReason::medical, SpecialReason::documents, SpecialReason::travel
};

struct Guard {
    std::string name;
    std::string id;
    std::string phone;
    std::string location;
};

using Roster = std::array<Guard, N>;
extern const std::array<std::string, N + 1> days;

struct Record {
    int id = 0;
    int week = 0;
    std::string createTime;
    std::string createUser;
    std::string modifyTime;
    std::string modifyUser;
    std::string remark;
    Rota rest{};
    Reasons reasons{};
};

struct Draft {
    int week = 0;
    std::string remark;
    Rota rest{};
    Reasons reasons{};
};

struct LocalDate {
    std::string timestamp;
    int weekday = 0;
    int week = 0;
};

struct UnmatchedDay {
    int day;
    int candidates;
};

struct Diagnosis {
    std::vector<int> guardsWithoutChoices;
    std::vector<UnmatchedDay> unmatchedDays;
};

std::string Trim(const std::string& value);
const char* ReasonName(SpecialReason reason);
LocalDate LocalNow(std::time_t time = std::time(nullptr));
std::vector<Rota> Solve(const Masks& masks);
Diagnosis Diagnose(const Masks& masks);
bool IsValidRota(const Rota& rest);
void SortAndRenumber(std::vector<Record>& records);
int AddRecord(std::vector<Record>& records, const Draft& draft,
              const std::string& user, const std::string& timestamp);
// Returns the displaced guard's index, or -1 if there was no swap.
int ChangeRestDay(Rota& rest, int guard, int day);
void ModifyRecord(Record& record, int guard, int day, SpecialReason reason, const std::string& note,
                  const std::string& user, const std::string& timestamp);
const Record* LatestForWeek(const std::vector<Record>& records, int week);
std::vector<int> AvailableWeeks(const std::vector<Record>& records);
std::vector<int> OnDuty(const Rota& rest, int day);

} // namespace schedule
