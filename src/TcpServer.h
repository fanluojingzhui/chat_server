#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <string>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <vector>
#include "ThreadPool.h"
#include "UserManager.h"
#include "ChatRoom.h"
#include "HttpParser.h"

struct DbConfig;

class TcpServer {
public:
    TcpServer(int port, size_t num_threads, const DbConfig& db);
    ~TcpServer();
    
    bool start();
    void run();
    void stop();

private:
    void handleClient(int client_fd);
    void handleHttpRequest(int client_fd, const std::string& data);
    void handleWebSocket(int client_fd, const std::string& data);
    void handleRegister(int client_fd, const HttpRequest& req);
    void handleLogin(int client_fd, const HttpRequest& req);
    void handleLogout(int client_fd, const HttpRequest& req);
    void broadcastMessage(const std::string& message, int excludeFd = -1);
    void broadcastSystemMessage(const std::string& message);
    void broadcastUserList();
    void cleanupClient(int client_fd);
    std::string readFile(const std::string& path);
    std::string getContentType(const std::string& path);

    int server_fd;
    int epfd;
    int port;
    std::atomic<bool> running;
    
    // fd → username 映射
    std::map<int, std::string> fd_username;
    std::mutex fd_mtx;
    
    // WebSocket 连接
    std::set<int> websocket_fds;
    std::mutex ws_mtx;
    
    std::unique_ptr<ThreadPool> thread_pool;
    UserManager userManager;
    ChatRoom chatRoom;
};

#endif