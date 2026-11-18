#include "Repository.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace schedule {
namespace {

const std::string escapedFormat = "mfc-escaped-v1";
const std::string reasonsFormat = "mfc-reasons-v1=";

std::runtime_error FileError(const char* action, const std::filesystem::path& path) {
    return std::runtime_error(std::string(action) + path.filename().u8string() +
                              "。请检查文件及目录的读写权限。");
}

std::string ReadText(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || (exists && !std::filesystem::is_regular_file(path, error))) {
        throw FileError("无法读取 ", path);
    }
    if (!exists) {
        return {};
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw FileError("无法打开 ", path);
    }
    std::string contents;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer))) {
        contents.append(buffer, sizeof(buffer));
    }
    if (!input.eof() || input.bad()) {
        throw FileError("无法读取 ", path);
    }
    contents.append(buffer, static_cast<size_t>(input.gcount()));
    return contents;
}

std::string WithoutBom(const std::string& text) {
    return text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? text.substr(3) : text;
}

void WriteText(const std::filesystem::path& path, const std::string& contents) {
    auto temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw FileError("无法写入 ", path);
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    bool replaced = false;
    if (output) {
#ifdef _WIN32
        replaced = MoveFileExW(temporary.c_str(), path.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        replaced = !error;
#endif
    }
    if (!replaced) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw FileError("无法保存 ", path);
    }
}

bool Legal(const std::string& value) {
    return !value.empty() && value.find_first_of(" \t:\r\n") == std::string::npos;
}

std::vector<std::string> Split(const std::string& text, char separator) {
    std::vector<std::string> result;
    size_t start = 0;
    for (size_t end = text.find(separator); end != std::string::npos;
         end = text.find(separator, start)) {
        result.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    result.push_back(text.substr(start));
    return result;
}

Roster ReadRoster(const std::filesystem::path& path) {
    const auto contents = WithoutBom(ReadText(path));
    if (contents.empty()) {
        throw std::runtime_error("请在程序所在目录放置 guard_info.txt，并填写七名保安资料。");
    }
    std::istringstream input(contents);
    Roster guards{};
    size_t total = 0;
    int lineNumber = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++lineNumber;
        line = Trim(line);
        if (line.empty()) {
            continue;
        }
        auto fields = Split(line, '|');
        for (auto& field : fields) {
            field = Trim(field);
        }
        if (fields.size() != 4 || std::any_of(fields.begin(), fields.end(), [](const std::string& field) {
                return field.empty();
            })) {
            throw std::runtime_error("guard_info.txt 第 " + std::to_string(lineNumber) +
                                     " 行格式有误。每行应填写姓名、编号、电话和岗位，并用 | 分隔。");
        }
        if (total == guards.size()) {
            throw std::runtime_error("guard_info.txt 必须包含七名保安资料。");
        }
        guards[total++] = {fields[0], fields[1], fields[2], fields[3]};
    }
    if (total != guards.size()) {
        throw std::runtime_error("guard_info.txt 必须包含七名保安资料。");
    }

    // 原排班文件按 BA01 到 BA07 保存轮休和原因。排序后改变名单行顺序不影响对应关系。
    std::sort(guards.begin(), guards.end(), [](const Guard& a, const Guard& b) {
        return a.id < b.id;
    });
    for (size_t i = 1; i < guards.size(); ++i) {
        if (guards[i - 1].id == guards[i].id) {
            throw std::runtime_error("guard_info.txt 中的保安编号重复：" + guards[i].id);
        }
    }
    return guards;
}

int ReadNumber(const std::string& text) {
    errno = 0;
    const long number = std::strtol(text.c_str(), nullptr, 10);
    if (errno == ERANGE || number < (std::numeric_limits<int>::min)() ||
        number > (std::numeric_limits<int>::max)()) {
        return 0;
    }
    return static_cast<int>(number);
}

Rota ParseRota(const std::string& text) {
    Rota rest{};
    const auto parts = Split(text, ' ');
    for (size_t i = 0; i < parts.size() && i < rest.size(); ++i) {
        const auto colon = parts[i].find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const auto day = parts[i].substr(colon + 1);
        for (int weekday = 1; weekday <= N; ++weekday) {
            if (days[weekday] == day) {
                rest[i] = weekday;
                break;
            }
        }
    }
    return rest;
}

std::string RotaText(const Rota& rest, const Roster& guards) {
    std::string text;
    for (int i = 0; i < N; ++i) {
        if (i != 0) {
            text += ' ';
        }
        const auto& name = guards[i].name;
        // 旧格式通过空格和冒号读取。姓名含分隔符时用序号作为该位置的标签。
        text += (name.find_first_of(" :") == std::string::npos ? name : std::to_string(i + 1)) + ":";
        if (rest[i] >= 1 && rest[i] <= N) {
            text += days[rest[i]];
        }
    }
    return text;
}

Reasons ParseReasons(const std::vector<std::string>& fields) {
    Reasons reasons{};
    for (size_t i = 8; i < fields.size(); ++i) {
        const auto& field = fields[i];
        if (field.size() != reasonsFormat.size() + reasons.size() ||
            field.compare(0, reasonsFormat.size(), reasonsFormat) != 0) {
            continue;
        }
        for (size_t guard = 0; guard < reasons.size(); ++guard) {
            const char code = field[reasonsFormat.size() + guard];
            if (code >= '1' && code <= '3') {
                reasons[guard] = static_cast<SpecialReason>(code - '0');
            }
        }
        break;
    }
    return reasons;
}

bool NeedsEscaping(const std::string& text) {
    return text.find_first_of("|\r\n") != std::string::npos;
}

// 普通记录保留旧版格式。仅对含分隔符的记录追加编码标记。
std::string Encode(const std::string& text) {
    std::string result;
    for (char ch : text) {
        switch (ch) {
        case '%': result += "%25"; break;
        case '|': result += "%7C"; break;
        case '\r': result += "%0D"; break;
        case '\n': result += "%0A"; break;
        default: result += ch; break;
        }
    }
    return result;
}

std::string Decode(const std::string& text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && text.size() - i >= 3) {
            const auto pair = text.substr(i + 1, 2);
            char decoded = '\0';
            if (pair == "25") decoded = '%';
            if (pair == "7C") decoded = '|';
            if (pair == "0D") decoded = '\r';
            if (pair == "0A") decoded = '\n';
            if (decoded != '\0') {
                result += decoded;
                i += 2;
                continue;
            }
        }
        result += text[i];
    }
    return result;
}

} // namespace

Repository::Repository(std::filesystem::path directory)
    : users_(directory / "users.txt"), schedules_(directory / "schedule.txt"),
      guards_(ReadRoster(directory / "guard_info.txt")) {}

Registration Repository::Register(const std::string& name, const std::string& password) const {
    const auto cleanName = Trim(name);
    const auto cleanPassword = Trim(password);
    if (!Legal(cleanName) || !Legal(cleanPassword)) {
        return Registration::invalid;
    }
    auto contents = ReadText(users_);
    std::istringstream input(WithoutBom(contents));
    std::string existingName;
    std::string existingPassword;
    while (input >> existingName) {
        if (!(input >> existingPassword)) {
            throw std::runtime_error("users.txt 中有缺少密码的账号行。请检查原文件。");
        }
        if (existingName == cleanName) {
            return Registration::duplicate;
        }
    }
    if (!contents.empty() && contents.back() != '\n') {
        contents += "\r\n";
    }
    contents += cleanName + " " + cleanPassword + "\r\n";
    WriteText(users_, contents);
    return Registration::success;
}

bool Repository::Authenticate(const std::string& name, const std::string& password) const {
    const auto cleanName = Trim(name);
    const auto cleanPassword = Trim(password);
    std::istringstream input(WithoutBom(ReadText(users_)));
    std::string existingName;
    std::string existingPassword;
    while (input >> existingName >> existingPassword) {
        if (existingName == cleanName && existingPassword == cleanPassword) {
            return true;
        }
    }
    return false;
}

std::vector<Record> Repository::LoadRecords() const {
    std::istringstream input(WithoutBom(ReadText(schedules_)));
    std::vector<Record> records;
    std::string line;
    while (std::getline(input, line)) {
        const auto fields = Split(Trim(line), '|');
        if (fields.size() < 8) {
            continue;
        }
        const bool escaped = fields.size() >= 9 && fields[8] == escapedFormat;
        const auto text = [&](size_t index) {
            const auto value = Trim(fields[index]);
            return escaped ? Decode(value) : value;
        };
        records.push_back({ReadNumber(fields[0]), ReadNumber(fields[6]), text(1),
                           text(2), text(3), text(4), text(5), ParseRota(Trim(fields[7])),
                           ParseReasons(fields)});
    }
    SortAndRenumber(records);
    return records;
}

void Repository::SaveRecords(const std::vector<Record>& records) const {
    std::ostringstream output;
    for (const auto& record : records) {
        const bool escaped = NeedsEscaping(record.createTime) || NeedsEscaping(record.createUser) ||
                             NeedsEscaping(record.modifyTime) || NeedsEscaping(record.modifyUser) ||
                             NeedsEscaping(record.remark);
        const auto text = [escaped](const std::string& value) {
            return escaped ? Encode(value) : value;
        };
        output << record.id << '|' << text(record.createTime) << '|' << text(record.createUser) << '|'
               << text(record.modifyTime) << '|' << text(record.modifyUser) << '|'
               << text(record.remark) << '|' << record.week << '|' << RotaText(record.rest, guards_);
        if (escaped) {
            output << '|' << escapedFormat;
        }
        if (std::any_of(record.reasons.begin(), record.reasons.end(), [](SpecialReason reason) {
                return reason != SpecialReason::none;
            })) {
            output << '|' << reasonsFormat;
            for (const auto reason : record.reasons) {
                output << static_cast<int>(reason);
            }
        }
        output << "\r\n";
    }
    WriteText(schedules_, output.str());
}

} // namespace schedule
