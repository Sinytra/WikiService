#include "crypto.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <vector>
#include <sstream>
#include <iomanip>

#define AES_KEY_SIZE 32
#define AES_BLOCK_SIZE 16

namespace crypto {
    std::string generateSecureRandomString(const size_t length) {
        const size_t byteCount = (length + 1) / 2;
        std::vector<unsigned char> buffer(byteCount);

        if (RAND_bytes(buffer.data(), static_cast<int>(byteCount)) != 1) {
            throw std::runtime_error("Error generating random bytes");
        }

        std::ostringstream oss;
        for (const unsigned char byte: buffer) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }

        return oss.str();
    }

    std::string urlSafeBase64Encode(std::string encoded) {
        for (char &c: encoded) {
            if (c == '+')
                c = '-';
            else if (c == '/')
                c = '_';
        }
        std::erase(encoded, '=');
        return encoded;
    }

    std::string urlSafeBase64Decode(std::string modified) {
        for (char &c: modified) {
            if (c == '-')
                c = '+';
            else if (c == '_')
                c = '/';
        }
        while (modified.size() % 4 != 0) {
            modified += '=';
        }
        return modified;
    }

    std::string base64Encode(const std::vector<unsigned char> &data) {
        BIO *bio = BIO_new(BIO_f_base64());
        BIO *bmem = BIO_new(BIO_s_mem());
        bio = BIO_push(bio, bmem);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newline

        BIO_write(bio, data.data(), data.size());
        BIO_flush(bio);

        BUF_MEM *bptr;
        BIO_get_mem_ptr(bio, &bptr);

        const std::string encoded(bptr->data, bptr->length);
        BIO_free_all(bio);
        return urlSafeBase64Encode(encoded);
    }

    std::vector<unsigned char> base64Decode(const std::string &original) {
        const auto encoded = urlSafeBase64Decode(original);

        BIO *bio = BIO_new_mem_buf(encoded.data(), encoded.size());
        BIO *b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newline

        std::vector<unsigned char> decoded(encoded.size());
        const int len = BIO_read(bio, decoded.data(), encoded.size());
        if (len < 0) {
            BIO_free_all(bio);
            throw std::runtime_error("Base64 decoding failed.");
        }
        decoded.resize(len);
        BIO_free_all(bio);
        return decoded;
    }

    // Encrypt a string using AES-256-CBC
    std::string encryptString(const std::string &plaintext, const std::string &key) {
        if (key.size() != AES_KEY_SIZE) {
            throw std::invalid_argument("Key must be 256 bits (32 bytes) long.");
        }

        // Generate a random IV
        std::vector<unsigned char> iv(AES_BLOCK_SIZE);
        if (!RAND_bytes(iv.data(), AES_BLOCK_SIZE)) {
            throw std::runtime_error("Failed to generate random IV.");
        }

        // Initialize encryption context
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create encryption context.");
        }

        std::vector<unsigned char> ciphertext(plaintext.size() + AES_BLOCK_SIZE);
        int len = 0;

        try {
            int ciphertext_len = 0;
            // Initialize encryption operation
            if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char *>(key.data()), iv.data()) != 1) {
                throw std::runtime_error("Encryption initialization failed.");
            }

            // Encrypt the plaintext
            if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, reinterpret_cast<const unsigned char *>(plaintext.data()),
                                  plaintext.size()) != 1)
            {
                throw std::runtime_error("Encryption update failed.");
            }
            ciphertext_len = len;

            // Finalize encryption
            if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
                throw std::runtime_error("Encryption finalization failed.");
            }
            ciphertext_len += len;
            ciphertext.resize(ciphertext_len);

            // Prepend the IV to the ciphertext
            std::vector combined(iv.begin(), iv.end());
            combined.insert(combined.end(), ciphertext.begin(), ciphertext.end());

            // Encode the result in Base64
            return base64Encode(combined);
        } catch (...) {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }
    }

    // Decrypt a string using AES-256-CBC
    std::string decryptString(const std::string &encodedCiphertext, const std::string &key) {
        if (key.size() != AES_KEY_SIZE) {
            throw std::invalid_argument("Key must be 256 bits (32 bytes) long.");
        }

        // Decode the Base64 input
        std::vector<unsigned char> combined = base64Decode(encodedCiphertext);
        if (combined.size() < AES_BLOCK_SIZE) {
            throw std::invalid_argument("Ciphertext is too short to contain an IV.");
        }

        // Extract the IV and ciphertext
        const std::vector iv(combined.begin(), combined.begin() + AES_BLOCK_SIZE);
        const std::vector actual_ciphertext(combined.begin() + AES_BLOCK_SIZE, combined.end());

        // Initialize decryption context
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create decryption context.");
        }

        std::vector<unsigned char> plaintext(actual_ciphertext.size() + AES_BLOCK_SIZE);
        int len = 0;

        try {
            int plaintext_len = 0;
            // Initialize decryption operation
            if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char *>(key.data()), iv.data()) != 1) {
                throw std::runtime_error("Decryption initialization failed.");
            }

            // Decrypt the ciphertext
            if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, actual_ciphertext.data(), actual_ciphertext.size()) != 1) {
                throw std::runtime_error("Decryption update failed.");
            }
            plaintext_len = len;

            // Finalize decryption
            if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
                throw std::runtime_error("Decryption finalization failed. Invalid key or corrupted ciphertext?");
            }
            plaintext_len += len;
            plaintext.resize(plaintext_len);
        } catch (...) {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }

        EVP_CIPHER_CTX_free(ctx);
        return std::string(plaintext.begin(), plaintext.end());
    }
}
