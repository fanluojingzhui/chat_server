#include "Crypto.h"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>

std::string Crypto::generateSalt(size_t length) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (size_t i = 0; i < length; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    return ss.str();
}

std::string Crypto::sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)data.c_str(), data.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string Crypto::hashPassword(const std::string& password, 
                                  const std::string& salt) {
    return sha256(salt + password + salt);
}

bool Crypto::verifyPassword(const std::string& password,
                             const std::string& salt,
                             const std::string& hash) {
    return hashPassword(password, salt) == hash;
}