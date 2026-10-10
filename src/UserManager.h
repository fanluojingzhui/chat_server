#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>
#include <map>
#include <mutex>
#include <memory>
#include "Database.h"
#include "Crypto.h"

class UserManager {
public:
    UserManager(const std::string& host,
                const std::string& user,
                const std::string& password,
                const std::string& database,
                int port = 3306);
    ~UserManager();
    
    bool init();
    
    bool registerUser(const std::string& username,
                      const std::string& password,
                      const std::string& email);
    
    bool login(const std::string& username,
               const std::string& password);
    
    bool userExists(const std::string& username);
    
    std::string createSession(const std::string& username);
    std::string getUsernameBySession(const std::string& sessionId);
    void deleteSession(const std::string& sessionId);
    
    bool saveMessage(const std::string& username,
                     const std::string& content);
    std::vector<Message> getMessages(int limit = 50);

private:
    std::unique_ptr<Database> db;
    std::map<std::string, std::string> sessions;
    std::mutex sessionMtx;
    
    std::string generateSessionId();
};

#endif