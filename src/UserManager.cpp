#include "UserManager.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

UserManager::UserManager(const std::string& host,
                         const std::string& user,
                         const std::string& password,
                         const std::string& database,
                         int port) {
    db = std::make_unique<Database>(host, user, password, database, port);
}

UserManager::~UserManager() {}

bool UserManager::init() {
    return db->init();
}

bool UserManager::registerUser(const std::string& username,
                               const std::string& password,
                               const std::string& email) {
    if (db->userExists(username)) {
        return false;
    }
    
    std::string salt = Crypto::generateSalt();
    std::string hashedPassword = Crypto::hashPassword(password, salt);
    
    return db->createUser(username, hashedPassword, salt, email);
}

bool UserManager::login(const std::string& username,
                        const std::string& password) {
    User user;
    if (!db->getUser(username, user)) {
        return false;
    }
    
    return Crypto::verifyPassword(password, user.salt, user.password);
}

bool UserManager::userExists(const std::string& username) {
    return db->userExists(username);
}

std::string UserManager::createSession(const std::string& username) {
    std::lock_guard<std::mutex> lock(sessionMtx);
    
    std::string sessionId = generateSessionId();
    sessions[sessionId] = username;
    return sessionId;
}

std::string UserManager::getUsernameBySession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionMtx);
    
    auto it = sessions.find(sessionId);
    if (it == sessions.end()) {
        return "";
    }
    return it->second;
}

void UserManager::deleteSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionMtx);
    sessions.erase(sessionId);
}

bool UserManager::saveMessage(const std::string& username,
                              const std::string& content) {
    return db->saveMessage(username, content);
}

std::vector<Message> UserManager::getMessages(int limit) {
    return db->getMessages("public", limit);
}

std::string UserManager::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}