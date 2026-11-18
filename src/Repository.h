#pragma once

#include "Scheduling.h"

#include <filesystem>

namespace schedule {

enum class Registration { success, invalid, duplicate };

class Repository {
public:
    explicit Repository(std::filesystem::path directory);

    const Roster& Guards() const { return guards_; }
    Registration Register(const std::string& name, const std::string& password) const;
    bool Authenticate(const std::string& name, const std::string& password) const;
    std::vector<Record> LoadRecords() const;
    void SaveRecords(const std::vector<Record>& records) const;

private:
    std::filesystem::path users_;
    std::filesystem::path schedules_;
    const Roster guards_;
};

} // namespace schedule
