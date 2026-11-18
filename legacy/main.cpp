#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <algorithm>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cctype>

#include <windows.h>

using namespace std;

const string FILE_USERS = "users.txt";
const string FILE_SCHEDULE = "schedule.txt";
const int N = 7;

const string WEEK[8] = { // 星期名称，下标 1~7
    "", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"
};


/*===================================
保安基本信息
===================================*/
struct Guard {
    string name, id, phone, location;
};

Guard guard_info[N] = {
    { "赵", "BA01", "13811111111", "文苑路九号" },
    { "钱", "BA02", "13811111112", "文苑路九号" },
    { "孙", "BA03", "13811111113", "文苑路九号" },
    { "李", "BA04", "13811111114", "文苑路九号" },
    { "周", "BA05", "13811111115", "文苑路九号" },
    { "吴", "BA06", "13811111116", "文苑路九号" },
    { "陈", "BA07", "13811111117", "文苑路九号" },
};

string currentUser = "" ; // 当前登录用户名
string regName, regPwd; // 输入的账号密码
string loginName; // 登陆成功用户名

bool legalAccount(string name, string password) {
    if ( name.empty() || password.empty() ) {
        return false;
    }
    //用户名和密码中不能包含空格、制表符、冒号
    return name.find_first_of(" \t:") == string::npos
    && password.find_first_of(" \t:") == string::npos;
}

// 去除输入字符串首位空格
string trim(const string &s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == string::npos) {
        return "";
    } 
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

string readLine(const string &prompt) {
    cout << prompt << endl;
    string s;
    if (!getline(cin,s)) {
        cout << "\n[输入结束，程序退出]\n";
        exit(0);
    }
    return s;
}

// 读入合法数字
int readInt(const string prompt, int lo, int hi) {
    while (true) {
        string s = trim(readLine(prompt));
        if (s == "0") return 0;
        bool ok = !s.empty();
        for(size_t i=0; ok && i<s.size(); i++) {
            if (!isdigit(s[i])) { // 检查输入是否为纯数字
                ok = false;
            }
        }
        if (ok) {
            int v = atoi(s.c_str());
            if (v >= lo && v <= hi) { // 检验读入数字是否在范围内
                return v;
            }
        }
        cout << "  [输入无效] 请输入 " << lo << " ~ " << hi << " 的数字（0=取消）\n";
    }
}

// 获取当前时间
string nowTime() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             lt -> tm_year + 1900, lt -> tm_mon + 1, lt -> tm_mday,
             lt -> tm_hour, lt -> tm_min, lt -> tm_sec);
    return string(buf);
}

// 获取当前周数
int nowYearWeek() {
    time_t t = time(nullptr);
    struct tm *lt = localtime(&t);

    // 以周一为 1、周日为 7
    int isoWday = (lt->tm_wday == 0) ? 7 : lt->tm_wday;

    struct tm thu = *lt;
    thu.tm_mday += 4 - isoWday;
    thu.tm_hour = 12;
    mktime(&thu);

    if (thu.tm_year > lt->tm_year) {
        return 1;
    }

    if (thu.tm_year < lt->tm_year) {
        int prevYear = 1900 + lt->tm_year - 1;

        struct tm dec31 = {0};
        dec31.tm_year = prevYear - 1900;
        dec31.tm_mon  = 11;
        dec31.tm_mday = 31;
        dec31.tm_hour = 12;
        mktime(&dec31);

        int wd = dec31.tm_wday;  // 0=周日
        bool leap = (prevYear % 4 == 0 && (prevYear % 100 != 0 || prevYear % 400 == 0));
        return (wd == 4 || (wd == 3 && leap)) ? 53 : 52;
    }
    return thu.tm_yday / 7 + 1;
}

// 计算 UTF-8 字符串显示宽度（中文占 2 列）
static int displayWidth(const string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        int len = (c < 0x80) ? 1 : ((c >> 5) == 0x06 ? 2 : ((c >> 4) == 0x0E ? 3 : 4));
        unsigned int cp = c;
        for (int k = 1; k < len && i + k < s.size(); ++k) {
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        bool wide = (cp >= 0x1100 && (cp <= 0x115F || (cp >= 0x2E80 && cp <= 0xA4CF) ||
                     (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
                     (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60)));
        w += wide ? 2 : 1;
        i += len;
    }
    return w;
}

static string padRight(const string& s, int width){
    int cur = displayWidth(s);
    return (cur >= width) ? s : s + string(width - cur, ' ');
}

// 注册用户
bool registerUser(){
    regName = trim(readLine("  请输入账号："));
    regPwd  = trim(readLine("  请输入密码："));

    // 检查账号密码是否合法
    if (!legalAccount(regName, regPwd)) {
        return false;
    }

    ifstream fin(FILE_USERS);
    string name, pwd;
    while (fin >> name >> pwd) {
        if (name == regName) {
            return false; // 账号名字已被使用
        }
    }
    return true;
}

// 添加新用户进文件
bool saveUser() {
    ofstream fout(FILE_USERS, ios::app);
    if (!fout){
        return false;
    }
    fout << regName << ' ' << regPwd << "\n"; // 写入文件
    return true;
}

// 用户登录
bool loginUser() {
    string name = trim(readLine("  请输入账号："));
    string pwd  = trim(readLine("  请输入密码："));

    ifstream fin(FILE_USERS);
    string n, p;
    while (fin >> n >> p) {
        if (n == name && p == pwd) { // 账密匹配
            loginName = name;
            return true;
        }
    }
    return false;
}

// 登录菜单
void login() {
    while (currentUser.empty()) {
        cout << "\n============== 用户系统 ==============\n";
        cout << "  1. 注册\n";
        cout << "  2. 登录\n";
        cout << "  0. 退出程序\n";
        int option = readInt("请选择：", 0, 2); // 读入用户选择

        if (option == 0) {
            cout<< "  已退出。\n";
            exit(0);
        }

        if (option == 1) {
            if (registerUser()) {
                // 注册成功
                if (saveUser()) {
                    // 保存成功
                    cout << "  注册成功，请登录。\n";
                }
                else{
                    cout << "  保存失败：无法写入 " << FILE_USERS << "\n";
                }
            }
            else {
                cout << "  注册失败：账号或密码为空（或含空格、冒号等特殊字符），或账号已存在。\n";
            }
        }

        if (option == 2) {
            if (loginUser()) {
                currentUser = loginName;
                cout << "  登录成功！欢迎 " << currentUser << "。\n";
            }
            else {
                cout << "  登录失败：账号或密码错误。\n";
            }
        }
    }
}

struct Record {
    int id = 0; // 记录编号
    int week = 0; // 周次
    string createTime; // 创建时间
    string createUser; // 创建人
    string modifyTime; // 最后修改时间
    string modifyUser; // 最后修改人
    string remark; // 备注
    array<int,N> rest{}; // N名保安各自的休息日
};

// 获取当前应该存入的ID
int nextId(const vector<Record> &recs) {
    int res = 0; // 读取当前最大id
    for(size_t i = 0; i < recs.size(); i++) {
        res = max(res, recs[i].id);
    }
    return res + 1; // 能用的id为当前最大id+1
}

// 从分隔符中获取有效信息
vector<string> split(const string &s, char sep) {
    vector<string> out; // 存被分隔符隔开的每一部分
    string cur; // 存每一个分隔符之间的内容
    for(size_t i = 0; i < s.size(); i++) {
        if (s[i] == sep) {
            out.push_back(cur);
            cur.clear();
        }
        else {
            cur.push_back(s[i]);
        }
    }
    out.push_back(cur);
    return out;
}

// 反解析备注
array<int,N> parseRest(const string &s) {
    array<int,N> a{};
    vector<string> parts = split(s, ' ');
    for (size_t i = 0; i < parts.size() && i < N; ++i) {
        size_t pos = parts[i].find(':');
        if (pos == string::npos) continue;
        string day = parts[i].substr(pos + 1);
        for (int w = 1; w <= N; ++w) {
            if (WEEK[w] == day) {
                a[i] = w;
                break;
            }
        }
    }
    return a;
}

// 按周升序重排安排表
void sortAndRenumberRecords(vector<Record> &recs) {
    // 使用稳定的排序算法
    stable_sort(recs.begin(), recs.end(), [](const Record &a, const Record &b) {
        if (a.week != b.week) return a.week < b.week;
        return a.id < b.id;
    });
    for (size_t i = 0; i < recs.size(); ++i) {
        recs[i].id = (int)(i + 1);
    }
}

// 获取所有保存的排班表
vector<Record> loadRecords() {
    vector<Record> recs; // 缓存所有记录
    ifstream fin(FILE_SCHEDULE);
    if (!fin) return recs;
    string line;
    while (getline(fin,line)) {
        line = trim(line); // 读取每一条安排
        if (line.empty()) continue;
        vector<string> f = split(line, '|');
        
        if (f.size() < 8) continue; // 记录不符合标准格式

        // 整合本条安排
        Record r;
        r.id = atoi(trim(f[0]).c_str());
        r.createTime = trim(f[1]);
        r.createUser = trim(f[2]);
        r.modifyTime = trim(f[3]);
        r.modifyUser = trim(f[4]);
        r.remark = trim(f[5]);
        r.week = atoi(trim(f[6]).c_str());
        r.rest = parseRest(trim(f[7]));

        recs.push_back(r);
    }
    sortAndRenumberRecords(recs);
    return recs;
}

// 打印单个排班表
void printRota(const array<int, N> &rest) {
    for(int i = 0; i < N; i++) {
        cout << guard_info[i].name << ' '; 
    }
    cout << '\n';
    for(int i = 0; i < N; i++) {
        cout << WEEK[rest[i]] << ' ';
    }
    cout << '\n';
}

// 打印记录细节
void printRecordDetail(const Record &r) {
    cout << "\n编号：" << r.id 
         << "    周次：第 " << r.week << " 周"
         << "    生成时间：" << r.createTime
         << "    生成人：" << r.createUser << "\n";
    if (!r.modifyTime.empty()) {
        cout << "最后修改：" << r.modifyTime << "    修改人：" << r.modifyUser << "\n";
    } else {
        cout << "最后修改：（尚未修改）\n";
    }
    cout << "备注：" << (r.remark.empty() ? "（无）" : r.remark) << "\n";
    printRota(r.rest);
}

// 将备注转化为标准格式
string restToText(const array<int,N> &rest) {
    string s;
    for (int i = 0; i < N; i++) {
        if (i) s+=" ";
        s += guard_info[i].name;
        s += ":";
        s += WEEK[rest[i]];
    }
    return s;
}

// 将记录写入文件
bool saveRecords(const vector<Record> &recs) {
    ofstream fout(FILE_SCHEDULE, ios::trunc);
    if (!fout) {
        return false;
    }
    for (size_t i = 0; i< recs.size(); i++) {
        const Record &r = recs[i];
        fout << r.id << "|" << r.createTime << "|" << r.createUser << "|"
             << r.modifyTime << "|" << r.modifyUser << "|" << r.remark << "|"
             << r.week << "|" << restToText(r.rest) << "\n";
             // 休息明细写成可读文本
    }
    return true;
}

// 将排班表信息写入文件
void writeSchedule() {
    // 直接写入文件
    Record r; // 暂存记录
    cout << "\n  请依次输入 7 名保安本周的休息日（1=周一 ... N=周日，0=取消）：\n";
    for(int i = 0; i < N; i++) {
        while (true) {
            string p = "  " + guard_info[i].name + " 的休息日：";
            int d = readInt(p, 1, N); // 输入指定的休息日
            if (d == 0) {
                cout << "  已取消新增。\n";
                return ;
            }
            bool clash = false;
            for(int k = 0; k < i; k++) {
                if (r.rest[k] == d) {
                    clash = true;
                }
            }
            if (clash) {
                cout << "    [冲突] " << WEEK[d] << " 已被前面的人占用\n";
                continue;
            }
            r.rest[i] = d;
            break;
        }
    }

    // 补全记录信息
    vector<Record> recs = loadRecords();
    r.id = nextId(recs);

    r.week = readInt("  请输入排班周次（如 1 表示第一周，0=取消）：", 1, 999);
    if (r.week == 0) {
        cout << "  已取消新增。\n";
        return;
    }

    r.createTime = nowTime();
    r.createUser = currentUser;
    r.modifyTime = "";
    r.modifyUser = "";
    r.remark = readLine("  备注(可空)：");

    printRecordDetail(r);
    if (readInt("确认新增这条记录吗？1=确认 0=取消：", 0, 1) != 1) {
        cout << "  已取消。\n";
        return;
    }

    recs.push_back(r);
    if (!saveRecords(recs)) {
        cout << "  [错误] 写入排班表文件失败\n";
        return ;
    }
    
    cout << "  新增成功（编号 " << r.id << "，操作人 " << currentUser
        << "，时间 " << r.createTime << "）\n";
}

vector<array<int,N> > tmp_sols;
int tmp_mask[N]; // 存每次尝试匹配的状态信息
int tmp_cur[N]; // 存每个人在星期几休息
int tmp_order[N]; // 存dfs匹配顺序

// 第 dep 层给 order[dep] 号保安安定休息日，used存使用日期掩码
// used 中第 k 位为 1 表示第 k+1 天已经被使用
void dfs(int dep, int used) {
    if (dep == N) {
        array<int,N> a{};
        for (int i = 0; i < N; i++) {
            a[i] = tmp_cur[i];
        }

        // 合法方案存入解决方案中
        tmp_sols.push_back(a);

        return ;
    }
    int p = tmp_order[dep];
    for (int d = 1; d <= N; d++) {
        if (!(tmp_mask[p] & (1 << d - 1))) {
            continue; // 不接受这一天轮休
        }
        if (used & (1 << d - 1)) {
            continue; // 这一天已经有人轮休
        }
        tmp_cur[p] = d;
        dfs(dep + 1, used | (1 << d - 1)); // 修改掩码，进行下一层搜索
        tmp_cur[p] = 0; // 回溯
    }
}

vector<array<int,N> > solve(const int mask[]) {
    // 赋初值
    for (int i = 0; i < N; i++) {
        tmp_mask[i] = mask[i];
        tmp_cur[i] = 0;
        tmp_order[i] = i;
    }
    tmp_sols.clear();

    // 按约束个数冒泡排序
    for (int i = 0;i < N; i++) {
        for (int j = i + 1; j < N; j++) {
            if (__builtin_popcount(tmp_mask[j]) < __builtin_popcount(tmp_mask[i])) {
                swap(tmp_order[i],tmp_order[j]);
            }
        }
    }

    dfs(0,0);
    return tmp_sols;
}

int match[N]; // d 匹配星期 match[d]
bool vis[N]; // 星期 d 是否被当前增广路访问

// 二分图最大匹配
bool findMaxSol(int p,const int mask[]) {
    for (int d = 0; d < N; d++) {
        if(!(mask[p] & (1 << d)) || vis[d]) {
            continue;
        }
        // 寻找能匹配的人和星期
        vis[d] = true;
        if (match[d] == -1 || findMaxSol(match[d],mask)) {
            match[d] = p;
            return true;
        }
    }
    return false;
}

void diagnoseNoSolution(const int mask[]) {
    bool err = false; // 记录是否存在错误

    for (int i = 0; i < N; i++ ) {
        if (mask[i] == 0) {
            cout << "    · 保安「" << guard_info[i].name << "」一个休息日都没有选\n";
            err = true;
        }
    }
    
    // 利用二分图最大匹配求出最少的无人员值守的天数
    for (int d = 0; d < N; d++) {
        match[d] = -1;
        vis[d] = false;
    }
    for (int p = 0; p < N; p++) {
        // 初始化
        for (int d = 0; d < N; d++) {
            vis[d] = 0;
        }
        // 利用匈牙利算法找到最多满足的方案
        findMaxSol(p,mask);
    }

    cout << "  判断结果（哪些星期没有值班人员）：\n";
    int emptyDay = 0; // 记录有几天是无人休息的
    for (int d = 1; d <=N; d++) {
        if (match[d] != -1) {
            continue;
        }
        emptyDay ++;
        int cand = 0; // 每一天有多少人值班
        for (int i = 0; i < N; i++) {
            if (mask[i] & (1 << d - 1)) {
                ++cand;
            }
        }
        if (cand == 0) {
            cout << "    · " << WEEK[d] << "：没有任何保安选择该日休息，"
                << "该日没有值班人员\n";
        }
        else {
            cout << "    · " << WEEK[d] << "：愿意休息的 " << cand
                << " 名保安已被其他日期占用，该日没有值班人员\n";
        }
        err = true;
    }
    // 防止出现意外情况
    if(emptyDay == 0 && !err) {
        cout << "    · 无法确定具体日期，请检查输入\n";
    }
}

void generateSchedule() {
    while (true) {
        int mask[N] = {0}; // 记录状压后保安的休息天数
        bool cancelled = false; // 是否取消
        cout << "\n============== 录入排班 ==============";
        cout << "\n每人可以只选一天，也可以选几天（如 2,4），系统会为每人挑合适的一天；";
        cout << "\n输入 0 表示取消。\n";

        for (int i = 0; i < N && !cancelled; i++) {
            while (true) {
                string s = readLine(string("\n  ") + guard_info[i].name + " 的可选休息日(1~7，可多选)：");
                if (s == "0") {
                    cancelled = true;
                    break;
                }

                int m = 0; // 状压存0
                int cnt = 0; // 计有多少可能的休息日
                bool bad = false; // 跳出循环flag
                for (size_t k = 0; k < s.size(); k++) {
                    char ch = s[k];
                    if (ch == ' ' || ch == ',' || ch == '/' || ch == ';' || ch == '\t') {
                        continue;
                    }
                    if (ch >= '1' && ch <= 'N') {
                        m |= (1<< ch - '1');
                        cnt++;
                        continue;
                    }
                    bad = true;
                    break;
                }
                if (bad || cnt == 0) {
                    cout << "  [输入无效] 请输入 1~7 的数字，可多选\n";
                    continue;
                }
                mask[i] = m; // 存入数组
                break;
            }
        }
        if (cancelled) {
            cout << "\n  已取消录入。\n";
            return ;
        }

        // 根据约束查找答案
        vector<array<int,N> > sols = solve(mask);

        // 无解处理
        if (sols.empty()) {
            cout << "\n**********************************************************\n";
            cout << "  无法作出轮休排班，需要重新输入！\n";
            cout << "**********************************************************\n";
            diagnoseNoSolution(mask);
            cout << "\n  请重新输入 7 名保安的可选休息日。\n";
            continue;
        }

        cout << "\n共找到 " << sols.size() << " 种可能的轮休表：\n";
        for (size_t k = 0; k < sols.size(); ++k) {
            cout << "\n------------ 轮休表 " << (k + 1) << " ------------\n";
            printRota(sols[k]);
        }

        cout << "\n请输入要保存的方案编号(1~" << sols.size() << "，0=不保存)：";
        int pick = readInt("", 0, (int)sols.size());
        if (pick == 0) {
            cout << "  未保存。\n";
            return ;
        }

        vector<Record> recs = loadRecords();
        Record r;
        r.id = nextId(recs);

        r.week = readInt("请输入本次排班周次（如 1 表示第一周，0=取消）：", 1, 999);
        if (r.week == 0) {
            cout << "  已取消保存。\n";
            return;
        }

        r.createTime = nowTime();
        r.createUser = currentUser;
        r.modifyTime = "";
        r.modifyUser = "";
        r.remark = readLine("请输入本次排班备注(可空)：");
        r.rest = sols[pick - 1];

        recs.push_back(r);
        if(!saveRecords(recs)) {
            cout << "  [错误] 写入排班表文件失败\n";;
            return ;
        }

        cout << "  已保存为第 " << r.id << " 条排班表"
             << "（生成时间 " << r.createTime << "，生成人 " << currentUser << "）\n";
        return;
    }
}

//  添加排班表菜单
void addMenu() {
    cout << "\n============== 欢迎进入增加排班表菜单 " << currentUser << "==============\n";
    cout << "  1. 手动输入排班表\n";
    cout << "  2. 输入每人期望轮休日\n";
    cout << "  0. 回到主菜单\n";

    int option = readInt("请选择：", 0, 2);

    switch(option) {
        case 0: return ;
        case 1: writeSchedule(); break;
        case 2: generateSchedule(); break;
        default: break;
    }
}

void printTable(const vector<Record>& recs){
    if (recs.empty()) { cout << "  （暂无排班表记录）\n"; return; }
    cout << "\n" << padRight("编号", 6) << padRight("周次", 8)
         << padRight("生成时间", 22) << padRight("生成人", 12)
         << padRight("修改时间", 22) << padRight("修改人", 12)
         << "备注\n";
    cout << string(96, '-') << "\n";
    for (size_t i = 0; i < recs.size(); ++i) {
        const Record& r = recs[i];
        cout << padRight(to_string(r.id), 6)
             << padRight("第" + to_string(r.week) + "周", 8)
             << padRight(r.createTime, 22)
             << padRight(r.createUser, 12)
             << padRight(r.modifyTime.empty() ? "-" : r.modifyTime, 22)
             << padRight(r.modifyUser.empty() ? "-" : r.modifyUser, 12)
             << (r.remark.empty() ? "-" : r.remark) << "\n";
    }
    cout << "  共 " << recs.size() << " 条记录\n";
}

// 删除指定排班表
void deleteRecord() {
    vector<Record> recs = loadRecords();
    if (recs.empty()) {
        cout << "  （暂无排班表记录）\n";
        return;
    }
    printTable(recs);
    int id = readInt("请输入要删除的编号(0=取消)：", 0, 999999);
    if (id == 0) return;

    int idx = -1;
    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].id == id) {
            idx = (int)i;
        }
    }
    if (idx < 0) {
        cout << "  未找到编号 " << id << " 的记录。\n";
        return ;
    }

    printRecordDetail(recs[idx]);
    if (readInt("确认删除这条记录吗？1=删除 0=取消：", 0, 1) != 1) {
        cout << "  已取消。\n";
        return;
    }

    recs.erase(recs.begin() + idx);
    if (!saveRecords(recs)) {
        cout << "  [错误] 写入排班表文件失败\n";
        return ;
    }
    cout << "  已删除编号 " << id << "（操作人 " << currentUser
         << "，时间 " << nowTime() << "）\n";
}

// 删除所有排班表
void deleteAllRecord() {
    vector<Record> recs = loadRecords();
    if (recs.empty()) {
        cout << "  （暂无排班表记录）\n";
        return;
    }

    printTable(recs);
    cout << "  当前共有 " << recs.size()
         << " 条记录，此操作将全部删除且不可恢复！\n";

    // 第一次确认
    if (readInt("确认删除全部记录吗？1=删除 0=取消：", 0, 1) != 1) {
        cout << "  已取消。\n";
        return;
    }
    // 第二次确认（二次防护，避免误操作）
    if (readInt("请再次确认，1=继续删除 0=取消：", 0, 1) != 1) {
        cout << "  已取消。\n";
        return;
    }

    recs.clear();
    if (!saveRecords(recs)) {
        cout << "  [错误] 写入排班表文件失败\n";
        return;
    }

    cout << "  已删除全部排班表记录（操作人 " << currentUser
         << "，时间 " << nowTime() << "）\n";
}

void deleteMenu() {
    cout << "\n============== 欢迎进入删除排班表菜单 "
         << currentUser << "==============\n";
    cout << "  1. 删除指定编号排班表\n";
    cout << "  2. 删除全部排班表\n";
    cout << "  0. 回到主菜单\n";

    int option = readInt("请选择：", 0, 2);

    switch(option) {
        case 0: return ;
        case 1: deleteRecord(); break;
        case 2: deleteAllRecord(); break;
        default: break;
    }
}

void modifyMenu() {
    vector<Record> recs = loadRecords();
    if (recs.empty()) {
        cout << "  （暂无排班表记录）\n";
        return;
    }
    
    printTable(recs);
    int id = readInt("请输入要修改的编号(0=取消)：", 0, 999999);
    if (id == 0) {
        return ;
    }
    int idx = -1;
    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].id == id) {
            idx = (int)i;
        }
    }
    if (idx < 0) {
        cout << "  未找到编号 " << id << " 的记录。\n";
        return ;
    }

    printRecordDetail(recs[idx]);
    int pi = readInt("请选择要修改的保安序号(1~7，0=取消)：", 0, N);
    if (pi == 0) {
        return;
    }
    pi--;

    while (true) {
        int d = readInt(string("  请输入 ") + guard_info[pi].name + " 的新休息日(1~7，0=取消)：", 1, N);
        if (d == 0) {
            return;
        }
        // 通过交换，保证修改后安排表的合法性
        int other = -1;
        for (int k = 0; k < N; ++k) {
            if (k != pi && recs[idx].rest[k] == d) {
                other = k;
            }
        }
        if (other >= 0) {
            int old = recs[idx].rest[pi];
            recs[idx].rest[other] = old;
            recs[idx].rest[pi] = d;
            cout << "    " << WEEK[d] << " 原本由 " << guard_info[other].name
                 << " 休息，已与其交换为 " << WEEK[old] << "\n";
        }
        else {
            recs[idx].rest[pi] = d;
        }
        break;
    }

    recs[idx].modifyTime = nowTime();
    recs[idx].modifyUser = currentUser;
    string note = readLine("  修改说明(可空，将追加到备注)：");
    if (!note.empty()) {
        if (!recs[idx].remark.empty()) recs[idx].remark += "；";
        recs[idx].remark += note;
    }

    if (!saveRecords(recs)) {
        cout << "  [错误] 写入排班表文件失败\n";
        return ;
    }
    cout << "  修改成功（修改时间 " << recs[idx].modifyTime
         << "，修改人 " << currentUser << "）\n";
    printRecordDetail(recs[idx]);
}

void queryMenu() {
    vector<Record> recs = loadRecords();
    if (recs.empty()) {
        cout << "  （暂无排班表记录）\n";
        return ;
    }

    printTable(recs);
    int id = readInt("请输入要查询的编号(0=取消)：", 0, 999999);
    if (id == 0) {
        return;
    }
    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].id == id) {
            printRecordDetail(recs[i]);
            return ;
        }
    }
    cout << "  未找到编号 " << id << " 的记录。\n";
}

// 获取今天是星期几
int queryToday() {
    time_t t = time(nullptr);
    tm *lt = localtime(&t);
    int w = lt->tm_wday;   // 0=周日, 1=周一, ..., 6=周六
    return (w == 0) ? 7 : w;
}

// 查询当天值班保安
void queryTodayDuty() {
    vector<Record> recs = loadRecords();
    if (recs.empty()) {
        cout << "  （暂无排班表记录，无法查询）\n";
        return;
    }

    int today = queryToday();

    cout << "\n  当前系统时间：" << nowTime() << "\n";
    cout << "  今天是 " << WEEK[today]
         << "，本年度第 " << nowYearWeek() << " 周\n";

    // 选出要参考的排班周次
    int week;
    if (recs.size() == 1) {
        week = recs[0].week;
        cout << "  使用第 " << week << " 周排班表\n";
    } else {
        vector<int> weeks;
        for (size_t i = 0; i < recs.size(); ++i) {
            if (find(weeks.begin(), weeks.end(), recs[i].week) == weeks.end()) {
                weeks.push_back(recs[i].week);
            }
        }
        sort(weeks.begin(), weeks.end());

        cout << "  可用的排班周次：";
        for (size_t i = 0; i < weeks.size(); ++i) {
            cout << "第" << weeks[i] << "周 ";
        }
        cout << "\n";

        week = readInt("  请输入要参考的周次（0=取消）：", 0, 999);
        if (week == 0) return;
    }

    // 取该周次里 id 最大的（最新）一条记录
    int idx = -1;
    for (size_t i = 0; i < recs.size(); ++i) {
        if (recs[i].week == week) {
            if (idx < 0 || recs[i].id > recs[idx].id) {
                idx = (int)i;
            }
        }
    }
    if (idx < 0) {
        cout << "  未找到第 " << week << " 周的排班表。\n";
        return;
    }

    const Record &r = recs[idx];

    // 输出标题：明确是「第 N 周排班表」在「星期几」的值班情况
    cout << "\n============== 第 " << r.week << " 周排班表 · "
         << WEEK[today] << " 值班保安 ==============\n";
    cout << padRight("姓名", 10)
         << padRight("编号", 10)
         << padRight("电话", 16)
         << "住所\n";
    cout << string(60, '-') << "\n";

    int dutyCount = 0;
    for (int i = 0; i < N; ++i) {
        if (r.rest[i] != today) {
            cout << padRight(guard_info[i].name, 10)
                 << padRight(guard_info[i].id, 10)
                 << padRight(guard_info[i].phone, 16)
                 << guard_info[i].location << "\n";
            ++dutyCount;
        }
    }

    cout << "  共 " << dutyCount << " 名保安值班，"
         << (N - dutyCount) << " 名保安休息\n";
}

void printGuardInfo(const Guard& g) {
    cout << "\n============== 保安信息 ==============\n";
    cout << "姓名：" << g.name << "\n";
    cout << "编号：" << g.id << "\n";
    cout << "电话：" << g.phone << "\n";
    cout << "岗位：" << g.location << "\n";
}

void printGuardRecentWeeks(int gi) {
    vector<Record> recs = loadRecords();

    int cur = nowYearWeek();
    vector<int> missing;

    cout << "\n---------- " << guard_info[gi].name
         << "（" << guard_info[gi].id << "）最近四周值班情况 ----------\n";
    cout << "  系统当前时间：" << nowTime() << "\n";
    cout << "  系统本周：第 " << cur << " 周\n";

    for (int w = cur - 3; w <= cur; ++w) {
        if (w < 1) continue;          // 年初不足四周，跳过非法周次

        int idx = -1;
        for (size_t i = 0; i < recs.size(); ++i) {
            if (recs[i].week == w && (idx < 0 || recs[i].id > recs[idx].id)) {
                idx = (int)i;
            }
        }

        if (idx < 0) {
            missing.push_back(w);
            continue;
        }

        const Record &r = recs[idx];
        int d = r.rest[gi];

        cout << "\n  第 " << w << " 周：";
        if (d < 1 || d > N) {
            cout << "休息日未记录（数据异常，请检查排班表文件）\n";
            continue;
        }

        cout << "休息 " << WEEK[d] << "；工作 ";
        bool first = true;
        for (int t = 1; t <= N; ++t) {
            if (t == d) continue;
            if (!first) cout << "、";
            cout << WEEK[t];
            first = false;
        }
        cout << "（排班表编号 " << r.id << "）\n";
    }

    if (!missing.empty()) {
        cout << "\n";
        for (size_t i = 0; i < missing.size(); ++i) {
            cout << "  未找到第 " << missing[i] << " 周\n";
        }
    }
}

void queryGuardInfo(){
    string input = trim(readLine("  请输入保安姓名（0=取消）："));
    if (input.empty() || input == "0") {
        cout << "  已取消。\n";
        return;
    }

    // 遍历找人
    vector<int> hits;
    for (int i = 0; i < N; ++i) {
        if (guard_info[i].name == input) {
            hits.push_back(i);
        }
    }

    // 没有找到
    if (hits.empty()) {
        cout << "  未找到名为「" << input << "」的保安。\n";
        cout << "  现有保安姓名：";
        for (int i = 0; i < N; ++i) {
            cout << guard_info[i].name;
            if (i + 1 < N) cout << "、";
        }
        cout << "\n";
        return;
    }

    // 找到一个人
    if (hits.size() == 1) {
        printGuardInfo(guard_info[hits[0]]);
        printGuardRecentWeeks(hits[0]);
        return;
    }

    // 重名
    cout << "  找到 " << hits.size() << " 名名为「" << input << "」的保安：\n";
    for (size_t k = 0; k < hits.size(); ++k) {
        const Guard& g = guard_info[hits[k]];
        cout << "    " << (k + 1) << ". "
             << g.name
             << "（编号 " << g.id
             << "，电话 " << g.phone
             << "，岗位 " << g.location << "）\n";
    }

    int pick = readInt("  请选择要查看的序号（0=取消）：", 0, (int)hits.size());
    if (pick == 0) {
        cout << "  已取消。\n";
        return;
    }
    printGuardInfo(guard_info[hits[pick - 1]]);
    printGuardRecentWeeks(hits[pick - 1]);
}

void mainMenu() {
    while (true) {
        cout << "\n============== 欢迎进入排班系统主菜单 " << currentUser << "==============\n";
        cout << "  1. 增加排班表\n";
        cout << "  2. 删除排班表\n";
        cout << "  3. 修改排班表\n";
        cout << "  4. 查找排班表\n";
        cout << "  5. 查询值班保安信息\n";
        cout << "  6. 查询指定保安信息\n";
        cout << "  0. 退出程序\n";
        int option = readInt("请选择：", 0, 6); // 读入用户选择
        
        if (option == 0) {
            cout << "感谢使用本系统。\n";
            Sleep(1000);
            exit(0);
        }

        if (option == 1) addMenu();
        if (option == 2) deleteMenu();
        if (option == 3) modifyMenu();
        if (option == 4) queryMenu();
        if (option == 5) queryTodayDuty();
        if (option == 6) queryGuardInfo();
    }
}

int main() {

    SetConsoleOutputCP(CP_UTF8);  // 控制台输出用 UTF-8
    SetConsoleCP(CP_UTF8);        // 控制台输入用 UTF-8
    
    login(); // 登录

    mainMenu(); //主菜单

    return 0;
}