#include "sqluna/security/encryption/field_encryption.h"

namespace sqluna::security::encryption {

std::string PassthroughEncryptor::encrypt(std::string_view plaintext) const { return std::string(plaintext); }

std::string PassthroughEncryptor::decrypt(std::string_view ciphertext) const { return std::string(ciphertext); }

}  // namespace sqluna::security::encryption
