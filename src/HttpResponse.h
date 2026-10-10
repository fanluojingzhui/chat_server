#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>

class HttpResponse {
public:
    static std::string build(int statusCode,
                             const std::string& contentType,
                             const std::string& body);
    
    static std::string buildWithCookie(int statusCode,
                                       const std::string& contentType,
                                       const std::string& body,
                                       const std::string& cookie);
    
    static std::string redirect(const std::string& location);
    static std::string notFound();
    static std::string serverError();
    
private:
    static std::string getStatusText(int statusCode);
};

#endif