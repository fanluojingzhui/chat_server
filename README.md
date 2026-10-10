# C++ 聊天室服务器

一个基于 **epoll + 线程池 + MySQL + WebSocket** 的网页聊天室，使用现代 C++（C++17）编写。

- 前端：原生 HTML/CSS/JavaScript（`www/` 目录）
- 后端：C++，同时充当 HTTP 服务器与 WebSocket 服务器
- 数据：MySQL 持久化用户与历史消息
- 安全：密码加盐哈希（SHA-256），JSON 协议传输

## 功能

- 用户注册 / 登录 / 退出（基于 Cookie 的 session）
- 实时聊天消息广播
- 在线用户列表
- 进入 / 离开聊天室系统提示
- 消息持久化到数据库
- 同一账号同时仅允许一个在线

## 依赖

| 依赖 | 用途 | Ubuntu 安装 |
|---|---|---|
| CMake ≥ 3.10 | 构建 | `sudo apt install cmake` |
| g++（支持 C++17） | 编译 | `sudo apt install build-essential` |
| libmysqlclient-dev | MySQL 客户端库 | `sudo apt install libmysqlclient-dev` |
| libssl-dev | OpenSSL（SHA-256） | `sudo apt install libssl-dev` |

> `nlohmann/json` 已作为单头文件内置在 `third_party/nlohmann/json.hpp`，无需额外安装。

## 数据库准备

1. 确保 MySQL 已启动：

   ```bash
   sudo systemctl start mysql
   ```

2. 创建数据库和用户（名称、密码可自定义，但需与 `.env` 一致）：

   ```sql
   CREATE DATABASE IF NOT EXISTS chat_server DEFAULT CHARSET utf8mb4;
   CREATE USER IF NOT EXISTS 'chat'@'localhost' IDENTIFIED BY 'chat123456';
   GRANT ALL PRIVILEGES ON chat_server.* TO 'chat'@'localhost';
   FLUSH PRIVILEGES;
   ```

   数据表（`users`、`messages`）会在服务器首次启动时自动创建，无需手动建表。

## 配置

1. 复制配置模板并填写真实数据库连接信息：

   ```bash
   cp .env.example .env
   ```

2. 编辑 `.env`：

   ```ini
   DB_HOST=localhost
   DB_USER=chat
   DB_PASSWORD=chat123456
   DB_NAME=chat_server
   DB_PORT=3306
   ```

> `.env` 已被 `.gitignore` 忽略，**不会**提交到版本库；`.env.example` 是提交的模板。

## 编译

```bash
cmake -S . -B build
cmake --build build
```

编译产物为 `build/chat_server`。

## 运行

```bash
./build/chat_server
```

启动后访问：**http://localhost:8888**

> ⚠️ **必须在项目根目录运行**。服务器用相对路径读取 `./www` 静态文件和 `.env`，在其它目录启动会导致页面 404 或读不到配置。

浏览器中打开后会自动跳转到登录页：注册一个账号 → 登录 → 进入聊天室。

## 配置速查

| 项目 | 默认值 | 位置 |
|---|---|---|
| 监听端口 | 8888 | `src/main.cpp` |
| 线程池大小 | 8 | `src/main.cpp` |
| 数据库连接 | 见 `.env` | `src/Config.cpp` |

## 目录结构

```
chat_server/
├── src/                 # 服务器源码
│   ├── main.cpp         # 入口：加载配置并启动服务器
│   ├── TcpServer.*      # epoll 事件循环 + HTTP/WebSocket 分发 + 广播
│   ├── Config.*         # 从 .env 读取数据库配置
│   ├── UserManager.*    # 注册/登录/session 管理
│   ├── Database.*       # MySQL 持久化（用户、消息）
│   ├── ChatRoom.*       # 内存中的在线用户登记表
│   ├── Crypto.*         # 密码加盐哈希
│   ├── WebSocket.*      # WebSocket 握手与帧解析
│   ├── HttpParser.*     # HTTP 请求解析
│   ├── HttpResponse.*   # HTTP 响应构造
│   └── ThreadPool.*     # 线程池
├── www/                 # 前端静态页面
├── third_party/         # 第三方依赖（nlohmann/json 单头文件）
├── .env.example         # 配置模板（提交）
└── CMakeLists.txt
```

## 架构简述

```
浏览器 (HTML/JS)
   │  HTTP / WebSocket
   ▼
TcpServer (epoll 事件循环)
   ├── HTTP 请求 ──► 注册 / 登录 / 静态文件
   └── WebSocket ──► 聊天 ──► broadcastMessage 广播
                       │
                       ├── UserManager ──► Database (MySQL)
                       └── ChatRoom (在线用户)
```

- 主线程用 `epoll_wait` 监听事件，新连接和可读事件分发给**线程池**异步处理。
- 客户端连接采用**边缘触发（EPOLLET）**模式，读数据时循环读到 `EAGAIN`。
- 登录成功后下发 `sessionId` Cookie；WebSocket 握手时从 Cookie 解析出用户名绑定到 fd。
