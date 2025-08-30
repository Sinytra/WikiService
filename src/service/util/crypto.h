#pragma once

#include <string>

namespace crypto {
    std::string generateSecureRandomString(size_t length);

    std::string hashSecureString(std::string input, std::string salt);

    std::string encryptString(const std::string& plaintext, const std::string& key);
    std::string decryptString(const std::string& encodedCiphertext, const std::string& key);
}