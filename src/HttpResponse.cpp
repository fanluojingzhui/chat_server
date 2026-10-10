#include "HttpResponse.h"

std::string HttpResponse::build(int statusCode,
                                const std::string& contentType,
                                const std::string& body) {
    std::string response;
    
    response += "HTTP/1.1 " + std::to_string(statusCode) + " " 
                + getStatusText(statusCode) + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "Server: MyHttpServer/1.0\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

std::string HttpResponse::buildWithCookie(int statusCode,
                                          const std::string& contentType,
                                          const std::string& body,
                                          const std::string& cookie) {
    std::string response;
    
    response += "HTTP/1.1 " + std::to_string(statusCode) + " " 
                + getStatusText(statusCode) + "\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Set-Cookie: " + cookie + "\r\n";
    response += "Connection: close\r\n";
    response += "Server: MyHttpServer/1.0\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

std::string HttpResponse::redirect(const std::string& location) {
    std::string response;
    
    response += "HTTP/1.1 302 Found\r\n";
    response += "Location: " + location + "\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    
    return response;
}

std::string HttpResponse::notFound() {
    std::string body = 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>404 Not Found</title></head>"
        "<body>"
        "<h1>404 Not Found</h1>"
        "<p>请求的资源不存在</p>"
        "</body>"
        "</html>";
    
    return build(404, "text/html", body);
}

std::string HttpResponse::serverError() {
    std::string body = 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>500 Internal Server Error</title></head>"
        "<body>"
        "<h1>500 Internal Server Error</h1>"
        "<p>服务器内部错误</p>"
        "</body>"
        "</html>";
    
    return build(500, "text/html", body);
}

std::string HttpResponse::getStatusText(int statusCode) {
    switch (statusCode) {
        case 200: return "OK";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}