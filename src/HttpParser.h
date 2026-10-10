#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    std::map<std::string, std::string> parseFormData() const;
    std::map<std::string, std::string> parseCookies() const;
};

class HttpParser {
public:
    static HttpRequest parse(const std::string& data);
    static bool isComplete(const std::string& data);
    
private:
    static void parseRequestLine(const std::string& line, HttpRequest& req);
    static void parseHeaders(const std::string& data, 
                             size_t start, 
                             size_t end, 
                             HttpRequest& req);
};

#endif