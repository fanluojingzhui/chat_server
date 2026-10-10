#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// 数据库连接配置
struct DbConfig {
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    int port = 3306;
};

// 从 .env 文件读取数据库配置
// 返回 false 表示文件不存在，或 host/user/database 关键字段缺失
bool loadDbConfig(const std::string& path, DbConfig& out);

#endif
