#ifndef CRYPTO_H
#define CRYPTO_H

#include <string>

class Crypto {
public:
    static std::string generateSalt(size_t length = 16);
    static std::string sha256(const std::string& data);
    static std::string hashPassword(const std::string& password, 
                                     const std::string& salt);
    static bool verifyPassword(const std::string& password,
                                const std::string& salt,
                                const std::string& hash);
};

#endif