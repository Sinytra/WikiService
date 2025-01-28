#pragma once

#include <string>

namespace crypto {
    std::string generateSecureRandomString(const size_t length);

    std::string encryptString(const std::string& plaintext, const std::string& key);
    std::string decryptString(const std::string& encodedCiphertext, const std::string& key);
}