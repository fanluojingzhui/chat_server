#include "TcpServer.h"
#include "Config.h"
#include <iostream>

int main() {
    try {
        DbConfig db;
        if (!loadDbConfig(".env", db)) {
            std::cerr << "无法读取数据库配置：请将 .env.example 复制为 .env "
                         "并填写数据库连接信息。" << std::endl;
            return 1;
        }

        TcpServer server(8888, 8, db);

        if (!server.start()) {
            std::cerr << "服务器启动失败" << std::endl;
            return 1;
        }
        
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}