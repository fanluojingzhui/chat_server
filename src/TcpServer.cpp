#include "TcpServer.h"
#include "Config.h"
#include "HttpParser.h"
#include "HttpResponse.h"
#include "WebSocket.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <errno.h>

using json = nlohmann::json;

#define BUFFER_SIZE 4096
#define MAX_EVENTS 1024

TcpServer::TcpServer(int port, size_t num_threads, const DbConfig& db)
    : port(port), running(false), server_fd(-1), epfd(-1),
      thread_pool(std::make_unique<ThreadPool>(num_threads)),
      userManager(db.host, db.user, db.password, db.database, db.port) {
    userManager.init();
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "创建 socket 失败" << std::endl;
        return false;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "绑定失败" << std::endl;
        close(server_fd);
        return false;
    }
    
    if (listen(server_fd, 10) == -1) {
        std::cerr << "监听失败" << std::endl;
        close(server_fd);
        return false;
    }
    
    epfd = epoll_create1(0);
    if (epfd == -1) {
        std::cerr << "epoll_create1 失败" << std::endl;
        close(server_fd);
        return false;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        std::cerr << "epoll_ctl 失败" << std::endl;
        close(epfd);
        close(server_fd);
        return false;
    }
    
    running = true;
    std::cout << "HTTP服务器已启动，端口：" << port << std::endl;
    std::cout << "访问 http://localhost:" << port << std::endl;
    return true;
}

void TcpServer::run() {
    struct epoll_event events[MAX_EVENTS];
    
    while (running) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (!running) break;
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait 失败: " << strerror(errno) << std::endl;
            continue;
        }
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, 
                                       (struct sockaddr*)&client_addr, 
                                       &client_addr_len);
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    std::cerr << "accept 失败" << std::endl;
                    continue;
                }
                
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                
                // 设置发送超时
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                int client_port = ntohs(client_addr.sin_port);
                
                std::cout << "新客户端: " << client_ip << ":" << client_port << std::endl;
                
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                thread_pool->enqueue([this, fd]() {
                    handleClient(fd);
                });
            }
        }
    }
}

void TcpServer::cleanupClient(int client_fd) {
    std::string username = chatRoom.getUsername(client_fd);
    
    // ★ 先清理，再广播
    chatRoom.removeClient(client_fd);
    
    {
        std::lock_guard<std::mutex> lock(fd_mtx);
        fd_username.erase(client_fd);
    }
    {
        std::lock_guard<std::mutex> lock(ws_mtx);
        websocket_fds.erase(client_fd);
    }
    
    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    
    // ★ 最后广播
    if (!username.empty()) {
        broadcastSystemMessage(username + " 离开了聊天室");
        broadcastUserList();
    }
}

void TcpServer::handleClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    std::string request_data;
    
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            cleanupClient(client_fd);
            return;
        }
        
        if (n == 0) {
            cleanupClient(client_fd);
            return;
        }
        
        request_data.append(buffer, n);
    }
    
    if (request_data.empty()) return;
    
    if (WebSocket::isHandshake(request_data)) {
        // 从握手请求的 Cookie 中解析 sessionId，得到用户名并绑定到该 fd
        HttpRequest handshakeReq = HttpParser::parse(request_data);
        auto cookies = handshakeReq.parseCookies();
        std::string sessionId = cookies["sessionId"];
        std::string username = userManager.getUsernameBySession(sessionId);

        std::string key = WebSocket::getKey(request_data);
        std::string response = WebSocket::handshake(key);
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);

        {
            std::lock_guard<std::mutex> lock(ws_mtx);
            websocket_fds.insert(client_fd);
        }
        if (!username.empty()) {
            std::lock_guard<std::mutex> lock(fd_mtx);
            fd_username[client_fd] = username;
        }
        std::cout << "WebSocket 握手成功: fd=" << client_fd
                  << " 用户=" << username << std::endl;
        return;
    }

    // 必须先释放 ws_mtx 再调用 handleWebSocket，
    // 否则 handleWebSocket 内部的 cleanupClient / broadcastMessage 会再次加锁导致死锁
    bool isWebSocket = false;
    {
        std::lock_guard<std::mutex> lock(ws_mtx);
        isWebSocket = websocket_fds.count(client_fd) > 0;
    }
    if (isWebSocket) {
        handleWebSocket(client_fd, request_data);
        return;
    }
    
    handleHttpRequest(client_fd, request_data);
    
    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    {
        std::lock_guard<std::mutex> lock(fd_mtx);
        fd_username.erase(client_fd);
    }
}

void TcpServer::handleHttpRequest(int client_fd, const std::string& data) {
    HttpRequest req = HttpParser::parse(data);
    
    std::cout << "请求: " << req.method << " " << req.path << std::endl;
    
    if (req.method == "POST") {
        if (req.path == "/register") {
            handleRegister(client_fd, req);
        } else if (req.path == "/login") {
            handleLogin(client_fd, req);
        } else {
            std::string response = HttpResponse::build(404, "text/html", 
                "<h1>404 Not Found</h1>");
            send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        }
        return;
    }
    
    if (req.method == "GET") {
        if (req.path == "/logout") {
            handleLogout(client_fd, req);
            return;
        }
        
        if (req.path == "/api/current-user") {
            auto cookies = req.parseCookies();
            std::string sessionId = cookies["sessionId"];
            std::string username = userManager.getUsernameBySession(sessionId);

            json body;
            body["username"] = username;
            std::string response = HttpResponse::build(200, "application/json", body.dump());
            send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
            return;
        }
        
        if (req.path == "/") {
            req.path = "/index.html";
        }
        
        std::string filepath = "./www" + req.path;
        std::string content = readFile(filepath);
        
        std::string response;
        if (content.empty()) {
            response = HttpResponse::notFound();
            std::cout << "404: " << req.path << std::endl;
        } else {
            std::string contentType = getContentType(req.path);
            response = HttpResponse::build(200, contentType, content);
            std::cout << "200: " << req.path << " (" << content.size() << " bytes)" << std::endl;
        }
        
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        return;
    }
    
    std::string response = HttpResponse::build(405, "text/html", 
        "<h1>405 Method Not Allowed</h1>");
    send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
}

void TcpServer::handleWebSocket(int client_fd, const std::string& data) {
    WebSocket::Frame frame = WebSocket::parseFrame(data);
    
    if (frame.opcode == 0x8) {
        std::string closeFrame = WebSocket::buildCloseFrame();
        send(client_fd, closeFrame.data(), closeFrame.size(), MSG_NOSIGNAL);
        cleanupClient(client_fd);
        return;
    }
    
    if (frame.opcode == 0x9) {
        std::string pong = WebSocket::buildPongFrame(frame.payload);
        send(client_fd, pong.data(), pong.size(), MSG_NOSIGNAL);
        return;
    }
    
    if (frame.opcode == 0x1) {
        std::string message = frame.payload;

        json j;
        try {
            j = json::parse(message);
        } catch (const json::exception& e) {
            std::cerr << "非法 JSON: " << e.what() << std::endl;
            return;
        }

        std::string type = j.value("type", "");

        if (type == "join") {
            std::string username;
            {
                std::lock_guard<std::mutex> lock(fd_mtx);
                username = fd_username[client_fd];
            }

            if (username.empty()) return;

            if (!chatRoom.addClient(client_fd, username)) {
                json errJson;
                errJson["type"] = "error";
                errJson["message"] = "该账号已在线";
                std::string f = WebSocket::buildFrame(errJson.dump());
                send(client_fd, f.data(), f.size(), MSG_NOSIGNAL);
                return;
            }

            // ★ 广播系统消息
            broadcastSystemMessage(username + " 加入了聊天室");

            // ★ 广播用户列表
            broadcastUserList();

        } else if (type == "chat") {
            std::string content = j.value("message", "");
            if (content.empty()) return;

            std::string username = chatRoom.getUsername(client_fd);
            if (username.empty()) return;

            userManager.saveMessage(username, content);

            json chatJson;
            chatJson["type"] = "chat";
            chatJson["username"] = username;
            chatJson["message"] = content;
            broadcastMessage(chatJson.dump());
        }
    }
}

void TcpServer::handleRegister(int client_fd, const HttpRequest& req) {
    auto form = req.parseFormData();
    
    std::string username = form["username"];
    std::string password = form["password"];
    std::string email = form["email"];
    
    std::cout << "注册: " << username << " " << email << std::endl;
    
    if (username.empty() || password.empty() || email.empty()) {
        std::string response = HttpResponse::build(400, "text/html",
            "<h1>400 Bad Request</h1><p>参数不完整</p>");
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        return;
    }
    
    if (userManager.registerUser(username, password, email)) {
        std::string response = HttpResponse::redirect("/login.html");
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        std::cout << "注册成功: " << username << std::endl;
    } else {
        std::string response = HttpResponse::build(400, "text/html",
            "<h1>400 Bad Request</h1><p>用户名已存在</p>");
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        std::cout << "注册失败: " << username << " 已存在" << std::endl;
    }
}

void TcpServer::handleLogin(int client_fd, const HttpRequest& req) {
    auto form = req.parseFormData();
    
    std::string username = form["username"];
    std::string password = form["password"];
    
    std::cout << "登录: " << username << std::endl;
    
    if (userManager.login(username, password)) {
        std::string sessionId = userManager.createSession(username);
        
        {
            std::lock_guard<std::mutex> lock(fd_mtx);
            fd_username[client_fd] = username;
        }
        
        std::string cookie = "sessionId=" + sessionId + "; Path=/";
        
        std::string response = "HTTP/1.1 302 Found\r\n"
                               "Location: /chat.html\r\n"
                               "Set-Cookie: " + cookie + "\r\n"
                               "Content-Length: 0\r\n"
                               "Connection: close\r\n"
                               "\r\n";
        
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        std::cout << "登录成功: " << username << std::endl;
    } else {
        std::string response = HttpResponse::build(401, "text/html",
            "<h1>401 Unauthorized</h1><p>用户名或密码错误</p>");
        send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
        std::cout << "登录失败: " << username << std::endl;
    }
}

void TcpServer::handleLogout(int client_fd, const HttpRequest& req) {
    auto cookies = req.parseCookies();
    
    auto it = cookies.find("sessionId");
    if (it != cookies.end()) {
        userManager.deleteSession(it->second);
    }
    
    std::string response = "HTTP/1.1 302 Found\r\n"
                           "Location: /login.html\r\n"
                           "Set-Cookie: sessionId=; Path=/; Max-Age=0\r\n"
                           "Content-Length: 0\r\n"
                           "Connection: close\r\n"
                           "\r\n";
    
    send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
    std::cout << "退出登录" << std::endl;
}

void TcpServer::broadcastMessage(const std::string& message, int excludeFd) {
    std::string frame = WebSocket::buildFrame(message);
    
    // ★ 先复制 fd 列表，再发送
    std::vector<int> fds;
    {
        std::lock_guard<std::mutex> lock(ws_mtx);
        for (int fd : websocket_fds) {
            if (fd == excludeFd) continue;
            fds.push_back(fd);
        }
    }
    
    // ★ 不持锁发送
    for (int fd : fds) {
        send(fd, frame.data(), frame.size(), MSG_NOSIGNAL);
    }
}

void TcpServer::broadcastSystemMessage(const std::string& message) {
    json j;
    j["type"] = "system";
    j["message"] = message;
    broadcastMessage(j.dump());
}

void TcpServer::broadcastUserList() {
    json j;
    j["type"] = "users";
    j["users"] = chatRoom.getOnlineUsers();
    broadcastMessage(j.dump());
}

std::string TcpServer::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return content;
}

std::string TcpServer::getContentType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos) return "image/jpeg";
    if (path.find(".gif") != std::string::npos) return "image/gif";
    if (path.find(".ico") != std::string::npos) return "image/x-icon";
    return "text/plain";
}

void TcpServer::stop() {
    running = false;
    if (epfd != -1) {
        close(epfd);
        epfd = -1;
    }
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}