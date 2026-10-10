#include "WebSocket.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

std::string WebSocket::base64Encode(const std::string& data) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.size() % 4) {
        result.push_back('=');
    }
    
    return result;
}

std::string WebSocket::handshake(const std::string& key) {
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char*)combined.c_str(), combined.size(), hash);
    
    std::string accept = base64Encode(std::string((char*)hash, SHA_DIGEST_LENGTH));
    
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";
    
    return response;
}

bool WebSocket::isHandshake(const std::string& data) {
    return data.find("Upgrade: websocket") != std::string::npos;
}

std::string WebSocket::getKey(const std::string& data) {
    std::string key = "Sec-WebSocket-Key: ";
    size_t pos = data.find(key);
    if (pos == std::string::npos) return "";
    
    pos += key.size();
    size_t end = data.find("\r\n", pos);
    if (end == std::string::npos) return "";
    
    return data.substr(pos, end - pos);
}

WebSocket::Frame WebSocket::parseFrame(const std::string& data) {
    Frame frame;
    frame.fin = false;
    frame.opcode = 0;
    frame.masked = false;
    frame.payloadLen = 0;
    
    if (data.size() < 2) return frame;
    
    const uint8_t* p = (const uint8_t*)data.data();
    
    frame.fin = (p[0] & 0x80) != 0;
    frame.opcode = p[0] & 0x0F;
    frame.masked = (p[1] & 0x80) != 0;
    frame.payloadLen = p[1] & 0x7F;
    
    size_t offset = 2;
    
    if (frame.payloadLen == 126) {
        if (data.size() < 4) return frame;
        frame.payloadLen = (p[2] << 8) | p[3];
        offset = 4;
    } else if (frame.payloadLen == 127) {
        if (data.size() < 10) return frame;
        frame.payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            frame.payloadLen = (frame.payloadLen << 8) | p[2 + i];
        }
        offset = 10;
    }
    
    if (frame.masked) {
        if (data.size() < offset + 4) return frame;
        uint8_t mask[4];
        memcpy(mask, p + offset, 4);
        offset += 4;
        
        if (data.size() < offset + frame.payloadLen) return frame;
        
        frame.payload.resize(frame.payloadLen);
        for (uint64_t i = 0; i < frame.payloadLen; i++) {
            frame.payload[i] = p[offset + i] ^ mask[i % 4];
        }
    } else {
        if (data.size() < offset + frame.payloadLen) return frame;
        frame.payload = data.substr(offset, frame.payloadLen);
    }
    
    return frame;
}

std::string WebSocket::buildFrame(const std::string& payload, uint8_t opcode) {
    std::string frame;
    
    frame += (char)(0x80 | opcode);
    
    if (payload.size() < 126) {
        frame += (char)payload.size();
    } else if (payload.size() < 65536) {
        frame += (char)126;
        frame += (char)((payload.size() >> 8) & 0xFF);
        frame += (char)(payload.size() & 0xFF);
    } else {
        frame += (char)127;
        for (int i = 7; i >= 0; i--) {
            frame += (char)((payload.size() >> (i * 8)) & 0xFF);
        }
    }
    
    frame += payload;
    return frame;
}

std::string WebSocket::buildCloseFrame() {
    return buildFrame("", 0x8);
}

std::string WebSocket::buildPongFrame(const std::string& payload) {
    return buildFrame(payload, 0xA);
}