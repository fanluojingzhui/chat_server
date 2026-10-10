#include "Config.h"
#include <fstream>
#include <map>

namespace {
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
}

bool loadDbConfig(const std::string& path, DbConfig& out) {
    std::ifstream file(path);
    if (!file) return false;

    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;   // 跳过空行和注释
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }

    out.host = kv["DB_HOST"];
    out.user = kv["DB_USER"];
    out.password = kv["DB_PASSWORD"];
    out.database = kv["DB_NAME"];

    std::string port = kv["DB_PORT"];
    if (!port.empty()) out.port = std::stoi(port);

    return !out.host.empty() && !out.user.empty() && !out.database.empty();
}
