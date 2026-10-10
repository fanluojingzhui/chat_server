#include "Database.h"
#include <iostream>
#include <cstring>
#include <algorithm>

Database::Database(const std::string& host,
                   const std::string& user,
                   const std::string& password,
                   const std::string& database,
                   int port)
    : conn(nullptr), host(host), user(user), 
      password(password), database(database), port(port) {}

Database::~Database() {
    disconnect();
}

bool Database::connect() {
    conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "mysql_init 失败" << std::endl;
        return false;
    }
    
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);
    
    if (!mysql_real_connect(conn, host.c_str(), user.c_str(),
                            password.c_str(), database.c_str(),
                            port, nullptr, 0)) {
        std::cerr << "MySQL 连接失败: " << mysql_error(conn) << std::endl;
        return false;
    }
    
    std::cout << "MySQL 连接成功: " << database << std::endl;
    return true;
}

void Database::disconnect() {
    if (conn) {
        mysql_close(conn);
        conn = nullptr;
    }
}

bool Database::init() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (!connect()) {
        return false;
    }
    
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INT PRIMARY KEY AUTO_INCREMENT,
            username VARCHAR(50) UNIQUE NOT NULL,
            password VARCHAR(128) NOT NULL,
            salt VARCHAR(64) NOT NULL,
            email VARCHAR(100) NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_username (username)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    )";
    
    if (mysql_query(conn, sql) != 0) {
        std::cerr << "建表失败: " << mysql_error(conn) << std::endl;
        return false;
    }
    
    const char* sql2 = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id BIGINT PRIMARY KEY AUTO_INCREMENT,
            username VARCHAR(50) NOT NULL,
            content TEXT NOT NULL,
            room VARCHAR(50) DEFAULT 'public',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            INDEX idx_room_time (room, created_at DESC),
            INDEX idx_username (username)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    )";
    
    if (mysql_query(conn, sql2) != 0) {
        std::cerr << "建表失败: " << mysql_error(conn) << std::endl;
        return false;
    }
    
    std::cout << "数据库初始化成功" << std::endl;
    return true;
}

std::string Database::escape(const std::string& str) {
    if (!conn) return str;
    
    char* escaped = new char[str.size() * 2 + 1];
    mysql_real_escape_string(conn, escaped, str.c_str(), str.size());
    std::string result(escaped);
    delete[] escaped;
    return result;
}

bool Database::createUser(const std::string& username,
                          const std::string& password,
                          const std::string& salt,
                          const std::string& email) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string sql = "INSERT INTO users (username, password, salt, email) VALUES ('" +
                      escape(username) + "', '" +
                      escape(password) + "', '" +
                      escape(salt) + "', '" +
                      escape(email) + "')";
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        std::cerr << "创建用户失败: " << mysql_error(conn) << std::endl;
        return false;
    }
    
    return true;
}

bool Database::getUser(const std::string& username, User& user) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string sql = "SELECT id, username, password, salt, email "
                      "FROM users WHERE username = '" + escape(username) + "'";
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return false;
    
    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return false;
    }
    
    user.id = std::stoi(row[0]);
    user.username = row[1] ? row[1] : "";
    user.password = row[2] ? row[2] : "";
    user.salt = row[3] ? row[3] : "";
    user.email = row[4] ? row[4] : "";
    
    mysql_free_result(result);
    return true;
}

bool Database::userExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string sql = "SELECT 1 FROM users WHERE username = '" + 
                      escape(username) + "' LIMIT 1";
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return false;
    
    bool exists = mysql_num_rows(result) > 0;
    mysql_free_result(result);
    
    return exists;
}

bool Database::saveMessage(const std::string& username,
                           const std::string& content,
                           const std::string& room) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::string sql = "INSERT INTO messages (username, content, room) VALUES ('" +
                      escape(username) + "', '" +
                      escape(content) + "', '" +
                      escape(room) + "')";
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        std::cerr << "保存消息失败: " << mysql_error(conn) << std::endl;
        return false;
    }
    
    return true;
}

std::vector<Message> Database::getMessages(const std::string& room, int limit) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<Message> messages;
    
    std::string sql = "SELECT id, username, content, room, created_at "
                      "FROM messages WHERE room = '" + escape(room) + "' "
                      "ORDER BY created_at DESC LIMIT " + std::to_string(limit);
    
    if (mysql_query(conn, sql.c_str()) != 0) {
        return messages;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return messages;
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        Message msg;
        msg.id = std::stoll(row[0]);
        msg.username = row[1] ? row[1] : "";
        msg.content = row[2] ? row[2] : "";
        msg.room = row[3] ? row[3] : "";
        msg.createdAt = row[4] ? row[4] : "";
        messages.push_back(msg);
    }
    
    mysql_free_result(result);
    
    std::reverse(messages.begin(), messages.end());
    
    return messages;
}