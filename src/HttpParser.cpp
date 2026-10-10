#include "HttpParser.h"
#include <sstream>

HttpRequest HttpParser::parse(const std::string& data) {
    HttpRequest req;
    
    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return req;
    }
    
    size_t firstLineEnd = data.find("\r\n");
    std::string requestLine = data.substr(0, firstLineEnd);
    parseRequestLine(requestLine, req);
    
    parseHeaders(data, firstLineEnd + 2, headerEnd, req);
    
    if (headerEnd + 4 < data.size()) {
        req.body = data.substr(headerEnd + 4);
    }
    
    return req;
}

void HttpParser::parseRequestLine(const std::string& line, HttpRequest& req) {
    std::istringstream iss(line);
    iss >> req.method >> req.path >> req.version;
}

void HttpParser::parseHeaders(const std::string& data, 
                              size_t start, 
                              size_t end, 
                              HttpRequest& req) {
    std::string headerSection = data.substr(start, end - start);
    std::istringstream iss(headerSection);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            size_t valueStart = value.find_first_not_of(' ');
            if (valueStart != std::string::npos) {
                value = value.substr(valueStart);
            }
            
            req.headers[key] = value;
        }
    }
}

std::map<std::string, std::string> HttpRequest::parseFormData() const {
    std::map<std::string, std::string> form;
    
    std::istringstream iss(body);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);
            form[key] = value;
        }
    }
    
    return form;
}

std::map<std::string, std::string> HttpRequest::parseCookies() const {
    std::map<std::string, std::string> cookies;
    
    auto it = headers.find("Cookie");
    if (it == headers.end()) {
        return cookies;
    }
    
    std::istringstream iss(it->second);
    std::string pair;
    
    while (std::getline(iss, pair, ';')) {
        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = pair.substr(0, pos);
            std::string value = pair.substr(pos + 1);
            
            size_t keyStart = key.find_first_not_of(' ');
            if (keyStart != std::string::npos) {
                key = key.substr(keyStart);
            }
            
            cookies[key] = value;
        }
    }
    
    return cookies;
}

bool HttpParser::isComplete(const std::string& data) {
    return data.find("\r\n\r\n") != std::string::npos;
}