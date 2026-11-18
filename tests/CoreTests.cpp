#include "../src/Repository.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {

using namespace schedule;
const Rota ordered{1, 2, 3, 4, 5, 6, 7};
const Rota reversed{7, 6, 5, 4, 3, 2, 1};

void Check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void TestGenerator() {
    const Masks allDays{127, 127, 127, 127, 127, 127, 127};
    const auto all = Solve(allDays);
    Check(all.size() == 5040 && all.front() == ordered && all.back() == reversed,
          "all 5040 schemes in the expected order");
    const Masks fixed{1, 2, 4, 8, 16, 32, 64};
    Check(Solve(fixed) == std::vector<Rota>{ordered}, "exactly one scheme");
    Check(!IsValidRota({1, 1, 3, 4, 5, 6, 7}), "reject a duplicate day");
    Check(!IsValidRota({0, 2, 3, 4, 5, 6, 7}), "reject an unassigned day");

    // Compare the DFS solution set with direct permutation filtering.
    std::mt19937 generator(2026);
    for (int trial = 0; trial < 128; ++trial) {
        Masks masks{};
        for (auto& item : masks) {
            item = static_cast<int>(generator() & 127);
        }
        std::vector<Rota> expected;
        auto permutation = ordered;
        do {
            bool allowed = true;
            for (int i = 0; i < N; ++i) {
                allowed = allowed && (masks[i] & DayBit(permutation[i])) != 0;
            }
            if (allowed) {
                expected.push_back(permutation);
            }
        } while (std::next_permutation(permutation.begin(), permutation.end()));
        auto actual = Solve(masks);
        std::sort(actual.begin(), actual.end());
        Check(expected == actual, "compare DFS and direct permutation filtering");
    }

    const Masks missingSunday{1, 2, 4, 8, 16, 32, 32};
    const auto missing = Diagnose(missingSunday);
    Check(Solve(missingSunday).empty() && missing.unmatchedDays.size() == 1 &&
          missing.unmatchedDays[0].day == 7 && missing.unmatchedDays[0].candidates == 0,
          "unmatched Sunday maps to the seventh day");
    const auto conflict = Diagnose({3, 4, 8, 16, 32, 64, 0});
    Check(conflict.guardsWithoutChoices == std::vector<int>{6} &&
          conflict.unmatchedDays.size() == 1 && conflict.unmatchedDays[0].day == 2 &&
          conflict.unmatchedDays[0].candidates == 1, "unmatched day with a conflicting choice");
}

LocalDate AtNoon(int year, int month, int day) {
    std::tm date{};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    date.tm_hour = 12;
    date.tm_isdst = -1;
    return LocalNow(std::mktime(&date));
}

void TestDates() {
    Check(AtNoon(2020, 12, 31).week == 53, "last week of 2020");
    Check(AtNoon(2021, 1, 3).week == 53 && AtNoon(2021, 1, 3).weekday == 7,
          "early January belongs to ISO week 53");
    Check(AtNoon(2021, 1, 4).week == 1 && AtNoon(2021, 1, 4).weekday == 1,
          "Monday begins ISO week 1");
    Check(AtNoon(2018, 12, 31).week == 1, "late December belongs to ISO week 1");
    Check(AtNoon(2022, 1, 1).week == 52, "early January belongs to ISO week 52");
}

class ScratchDirectory {
public:
    ScratchDirectory()
        : path(std::filesystem::temp_directory_path() /
               ("guard-scheduler-tests-" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()))) {
        Check(std::filesystem::create_directory(path), "create test directory");
    }

    ~ScratchDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    const std::filesystem::path path;
};

void TestRepository() {
    const ScratchDirectory scratch;
    {
        std::ofstream roster(scratch.path / "guard_info.txt", std::ios::binary);
        roster << "\xEF\xBB\xBF"
               << "陈小七|BA07|13811111117|西门\r\n"
               << "赵 换班:一 | BA01 | 16600000001 | 正门 岗亭\r\n"
               << "孙|BA03|13811111113|北门\r\n"
               << "吴|BA06|13811111116|北门\r\n"
               << "李|BA04|13811111114|北门\r\n"
               << "钱|BA02|13811111112|北门\r\n"
               << "周|BA05|13811111115|北门\r\n";
    }
    const Repository repository(scratch.path);
    const auto& roster = repository.Guards();
    Check(roster[0].name == "赵 换班:一" && roster[0].id == "BA01" &&
          roster[0].phone == "16600000001" && roster[0].location == "正门 岗亭" &&
          roster[6].name == "陈小七" && roster[6].id == "BA07",
          "read UTF-8 roster, trim fields, and order the guards by ID");
    Check(repository.LoadRecords().empty(), "first run has no records");
    Check(repository.Register("bad:name", "pw") == Registration::invalid, "reject invalid name");
    {
        std::ofstream users(scratch.path / "users.txt", std::ios::binary);
        users << "旧用户 pw\r\n";
        std::ofstream schedules(scratch.path / "schedule.txt", std::ios::binary);
        schedules << "9|2026-01-01 12:00:00|旧用户|||原备注 %7C|10|"
                  << "赵:星期一 钱:星期二 孙:星期三 李:星期四 周:星期五 吴:星期六 陈:星期日\r\n";
    }
    Check(repository.Authenticate(" 旧用户 ", "pw"), "read old UTF-8 account");
    Check(!repository.Authenticate("旧用户", "wrong"), "reject wrong password");
    Check(repository.Register("旧用户", "pw") == Registration::duplicate, "reject duplicate");
    Check(repository.Register("新|用户", "新密码") == Registration::success, "register new account");
    Check(repository.Authenticate("新|用户", "新密码"), "authenticate new account");

    auto records = repository.LoadRecords();
    Check(records.size() == 1 && records[0].id == 1 && records[0].rest == ordered &&
          records[0].remark == "原备注 %7C" && records[0].reasons == Reasons{},
          "read old schedule, keep literal percent encoding, and default to no reasons");
    const std::string timestamp = "2026-02-01 12:00:00";
    const Reasons entered{SpecialReason::travel, SpecialReason::documents, SpecialReason::medical};
    Check(AddRecord(records, {10, "调班 | 交接 %25", reversed, entered}, "新|用户", timestamp) == 2,
          "newest record for the same week sorts last");
    Check(AddRecord(records, {3, "", ordered}, "旧用户", timestamp) == 1, "earlier week sorts first");
    Check(AvailableWeeks(records) == std::vector<int>({3, 10}), "list distinct weeks");

    Check(records[2].reasons == entered, "save reasons with the selected rota");
    ModifyRecord(records[2], 0, 6, SpecialReason::medical, "已确认", "旧用户", timestamp);
    repository.SaveRecords(records);
    records = repository.LoadRecords();
    Check(records[0].rest == ordered, "a name with separators does not affect rest data");
    const auto* latest = LatestForWeek(records, 10);
    Check(latest && latest->id == 3 && latest->rest[0] == 6 && latest->rest[1] == 7 &&
          IsValidRota(latest->rest) && latest->createUser == "新|用户" &&
          latest->modifyUser == "旧用户" && latest->modifyTime == timestamp &&
          latest->remark == "调班 | 交接 %25；已确认", "swap, audit, and escaped-text round trip");
    Check(latest->reasons == Reasons{SpecialReason::medical, SpecialReason::documents,
                                     SpecialReason::medical},
          "reasons survive storage and stay with their guards when rest days swap");
    Check(OnDuty(latest->rest, 7) == std::vector<int>({0, 2, 3, 4, 5, 6}), "today's duty");
    const auto beforeClear = records[2].rest;
    ModifyRecord(records[2], 0, 6, SpecialReason::none, "", "旧用户", timestamp);
    Check(records[2].rest == beforeClear && records[2].reasons[0] == SpecialReason::none &&
          records[2].reasons[1] == SpecialReason::documents,
          "clear one reason without changing rest days or other reasons");
    records.erase(records.begin() + 1);
    SortAndRenumber(records);
    repository.SaveRecords(records);
    Check(repository.LoadRecords().size() == 2 &&
          LatestForWeek(records, 10)->id == 2, "delete one and renumber");
    repository.SaveRecords({});
    Check(repository.LoadRecords().empty() && repository.Authenticate("旧用户", "pw"),
          "delete all schedules and keep accounts");
}

} // namespace

int main() {
    try {
        TestGenerator();
        TestDates();
        TestRepository();
        std::cout << "All core checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
