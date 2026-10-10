#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <mutex>
#include <mysql/mysql.h>

struct User {
    int id;
    std::string username;
    std::string password;
    std::string salt;
    std::string email;
};

struct Message {
    long long id;
    std::string username;
    std::string content;
    std::string room;
    std::string createdAt;
};

class Database {
public:
    Database(const std::string& host,
             const std::string& user,
             const std::string& password,
             const std::string& database,
             int port = 3306);
    ~Database();
    
    bool init();
    
    bool createUser(const std::string& username,
                    const std::string& password,
                    const std::string& salt,
                    const std::string& email);
    bool getUser(const std::string& username, User& user);
    bool userExists(const std::string& username);
    
    bool saveMessage(const std::string& username,
                     const std::string& content,
                     const std::string& room = "public");
    std::vector<Message> getMessages(const std::string& room = "public",
                                      int limit = 50);

private:
    MYSQL* conn;
    std::string host, user, password, database;
    int port;
    std::mutex mtx;
    
    bool connect();
    void disconnect();
    std::string escape(const std::string& str);
};

#endif