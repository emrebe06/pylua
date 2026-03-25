#pragma once

#include <string>
#include <string_view>

namespace sqluna::security::encryption {

class FieldEncryptor {
  public:
    virtual ~FieldEncryptor() = default;
    virtual std::string encrypt(std::string_view plaintext) const = 0;
    virtual std::string decrypt(std::string_view ciphertext) const = 0;
};

class PassthroughEncryptor final : public FieldEncryptor {
  public:
    std::string encrypt(std::string_view plaintext) const override;
    std::string decrypt(std::string_view ciphertext) const override;
};

}  // namespace sqluna::security::encryption
