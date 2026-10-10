#include "ChatRoom.h"
#include <iostream>

bool ChatRoom::addClient(int fd, const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // 检查用户是否已在线
    for (auto& [f, client] : clients) {
        if (client.username == username && client.isWebSocket) {
            return false;  // 已在线
        }
    }
    
    clients[fd] = {fd, username, true};
    std::cout << "用户加入聊天室: " << username << " (fd=" << fd << ")" << std::endl;
    return true;
}

void ChatRoom::removeClient(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto it = clients.find(fd);
    if (it != clients.end()) {
        std::cout << "用户离开聊天室: " << it->second.username << std::endl;
        clients.erase(it);
    }
}

std::vector<std::string> ChatRoom::getOnlineUsers() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::string> users;
    for (auto& [fd, client] : clients) {
        users.push_back(client.username);
    }
    return users;
}

bool ChatRoom::isOnline(const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto& [fd, client] : clients) {
        if (client.username == username) return true;
    }
    return false;
}

ChatClient* ChatRoom::getClient(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(fd);
    if (it == clients.end()) return nullptr;
    return &(it->second);
}

std::string ChatRoom::getUsername(int fd) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(fd);
    if (it == clients.end()) return "";
    return it->second.username;
}