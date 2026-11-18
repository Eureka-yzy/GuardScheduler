#include "Scheduling.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace schedule {

const std::array<std::string, N + 1> days{
    "", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"
};

namespace {

int CountBits(unsigned int mask) {
    int count = 0;
    while (mask != 0) {
        mask &= mask - 1;
        ++count;
    }
    return count;
}

void Enumerate(int depth, int used, const Masks& masks, const Rota& order,
               Rota& current, std::vector<Rota>& solutions) {
    if (depth == N) {
        solutions.push_back(current);
        return;
    }
    const int person = order[depth];
    for (int day = 1; day <= N; ++day) {
        const int bit = DayBit(day);
        if ((masks[person] & bit) && !(used & bit)) {
            current[person] = day;
            Enumerate(depth + 1, used | bit, masks, order, current, solutions);
            current[person] = 0;
        }
    }
}

bool Augment(int person, const Masks& masks, std::array<int, N>& match,
             std::array<bool, N>& visited) {
    for (int slot = 0; slot < N; ++slot) {
        if (!(masks[person] & DayBit(slot + 1)) || visited[slot]) {
            continue;
        }
        visited[slot] = true;
        if (match[slot] == -1 || Augment(match[slot], masks, match, visited)) {
            match[slot] = person;
            return true;
        }
    }
    return false;
}

std::tm LocalTime(std::time_t time) {
#ifdef _WIN32
    std::tm date{};
    if (localtime_s(&date, &time) == 0) {
        return date;
    }
#else
    if (const auto* date = std::localtime(&time)) {
        return *date;
    }
#endif
    throw std::runtime_error("无法读取本机日期和时间。");
}

int WeekOfYear(const std::tm& local, int weekday) {
    std::tm thursday = local;
    thursday.tm_mday += 4 - weekday;
    thursday.tm_hour = 12;
    thursday.tm_isdst = -1;
    std::mktime(&thursday);
    if (thursday.tm_year > local.tm_year) {
        return 1;
    }
    if (thursday.tm_year < local.tm_year) {
        const int previousYear = local.tm_year + 1899;
        std::tm december31{};
        december31.tm_year = previousYear - 1900;
        december31.tm_mon = 11;
        december31.tm_mday = 31;
        december31.tm_hour = 12;
        december31.tm_isdst = -1;
        std::mktime(&december31);
        const bool leap = previousYear % 4 == 0 &&
                          (previousYear % 100 != 0 || previousYear % 400 == 0);
        return (december31.tm_wday == 4 || (december31.tm_wday == 3 && leap)) ? 53 : 52;
    }
    return thursday.tm_yday / N + 1;
}

} // namespace

std::string Trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

const char* ReasonName(SpecialReason reason) {
    switch (reason) {
    case SpecialReason::medical: return "看病";
    case SpecialReason::documents: return "办证";
    case SpecialReason::travel: return "旅游";
    default: return "无特殊情况";
    }
}

LocalDate LocalNow(std::time_t time) {
    const auto local = LocalTime(time);
    const int weekday = local.tm_wday == 0 ? N : local.tm_wday;
    char timestamp[64]{};
    std::snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                  local.tm_hour, local.tm_min, local.tm_sec);
    return {timestamp, weekday, WeekOfYear(local, weekday)};
}

std::vector<Rota> Solve(const Masks& masks) {
    Rota order{0, 1, 2, 3, 4, 5, 6};
    // 与控制台版一样，按原掩码下标比较，只交换搜索顺序。
    // 使用 order 中的人去比较会改变方案编号顺序。
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            if (CountBits(masks[j]) < CountBits(masks[i])) {
                std::swap(order[i], order[j]);
            }
        }
    }
    Rota current{};
    std::vector<Rota> solutions;
    Enumerate(0, 0, masks, order, current, solutions);
    return solutions;
}

Diagnosis Diagnose(const Masks& masks) {
    Diagnosis result;
    std::array<int, N> match;
    match.fill(-1);
    for (int person = 0; person < N; ++person) {
        if (masks[person] == 0) {
            result.guardsWithoutChoices.push_back(person);
        }
        std::array<bool, N> visited{};
        Augment(person, masks, match, visited);
    }
    for (int day = 1; day <= N; ++day) {
        if (match[day - 1] == -1) {
            const auto candidates = std::count_if(masks.begin(), masks.end(), [day](int mask) {
                return (mask & DayBit(day)) != 0;
            });
            result.unmatchedDays.push_back({day, static_cast<int>(candidates)});
        }
    }
    return result;
}

bool IsValidRota(const Rota& rest) {
    int used = 0;
    for (const int day : rest) {
        if (day < 1 || day > N || (used & DayBit(day))) {
            return false;
        }
        used |= DayBit(day);
    }
    return true;
}

void SortAndRenumber(std::vector<Record>& records) {
    std::stable_sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
        return a.week == b.week ? a.id < b.id : a.week < b.week;
    });
    for (size_t i = 0; i < records.size(); ++i) {
        records[i].id = static_cast<int>(i + 1);
    }
}

int AddRecord(std::vector<Record>& records, const Draft& draft,
              const std::string& user, const std::string& timestamp) {
    if (draft.week < 1 || draft.week > 999 || !IsValidRota(draft.rest)) {
        throw std::runtime_error("周次必须为 1–999，且每人休息日必须各不相同。");
    }
    int highestId = 0;
    int newId = 1;
    for (const auto& record : records) {
        highestId = std::max(highestId, record.id);
        if (record.week <= draft.week) {
            ++newId;
        }
    }
    records.push_back({highestId + 1, draft.week, timestamp, user, {}, {},
                       Trim(draft.remark), draft.rest, draft.reasons});
    SortAndRenumber(records);
    return newId;
}

int ChangeRestDay(Rota& rest, int guard, int day) {
    int other = -1;
    for (int i = 0; i < N; ++i) {
        if (i != guard && rest[i] == day) {
            other = i;
        }
    }
    if (other >= 0) {
        rest[other] = rest[guard];
    }
    rest[guard] = day;
    return other;
}

void ModifyRecord(Record& record, int guard, int day, SpecialReason reason, const std::string& note,
                  const std::string& user, const std::string& timestamp) {
    ChangeRestDay(record.rest, guard, day);
    record.reasons[guard] = reason;
    record.modifyTime = timestamp;
    record.modifyUser = user;
    if (!note.empty()) {
        if (!record.remark.empty()) {
            record.remark += "；";
        }
        record.remark = Trim(record.remark + note);
    }
}

const Record* LatestForWeek(const std::vector<Record>& records, int week) {
    const Record* latest = nullptr;
    for (const auto& record : records) {
        if (record.week == week && (!latest || record.id > latest->id)) {
            latest = &record;
        }
    }
    return latest;
}

std::vector<int> AvailableWeeks(const std::vector<Record>& records) {
    std::vector<int> weeks;
    for (const auto& record : records) {
        weeks.push_back(record.week);
    }
    std::sort(weeks.begin(), weeks.end());
    weeks.erase(std::unique(weeks.begin(), weeks.end()), weeks.end());
    return weeks;
}

std::vector<int> OnDuty(const Rota& rest, int day) {
    std::vector<int> result;
    for (int guard = 0; guard < N; ++guard) {
        if (rest[guard] != day) {
            result.push_back(guard);
        }
    }
    return result;
}

} // namespace schedule
