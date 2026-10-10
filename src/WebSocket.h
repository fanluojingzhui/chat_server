#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>
#include <cstdint>

class WebSocket {
public:
    static std::string handshake(const std::string& key);
    static bool isHandshake(const std::string& data);
    static std::string getKey(const std::string& data);
    
    struct Frame {
        bool fin;
        uint8_t opcode;
        bool masked;
        uint64_t payloadLen;
        std::string payload;
    };
    
    static Frame parseFrame(const std::string& data);
    static std::string buildFrame(const std::string& payload, uint8_t opcode = 0x1);
    static std::string buildCloseFrame();
    static std::string buildPongFrame(const std::string& payload);
    
private:
    static std::string base64Encode(const std::string& data);
};

#endif