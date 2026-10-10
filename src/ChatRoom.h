#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include <string>
#include <map>
#include <vector>
#include <mutex>

struct ChatClient {
    int fd;
    std::string username;
    bool isWebSocket;
};

class ChatRoom {
public:
    // 添加客户端（返回 false 表示用户已在线）
    bool addClient(int fd, const std::string& username);
    
    // 移除客户端
    void removeClient(int fd);
    
    // 获取在线用户列表
    std::vector<std::string> getOnlineUsers();
    
    // 检查用户是否在线
    bool isOnline(const std::string& username);
    
    // 获取客户端信息
    ChatClient* getClient(int fd);
    
    // 获取 fd 对应的 username
    std::string getUsername(int fd);

private:
    std::map<int, ChatClient> clients;
    std::mutex mtx;
};

#endif