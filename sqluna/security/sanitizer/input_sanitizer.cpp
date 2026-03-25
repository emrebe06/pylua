#include "sqluna/security/sanitizer/input_sanitizer.h"

#include <cctype>

#include "sqluna/utils/error/error.h"

namespace sqluna::security::sanitizer {

void ensure_identifier(std::string_view identifier) {
    if (identifier.empty()) {
        throw utils::error::QueryError("identifier cannot be empty");
    }

    for (const char ch : identifier) {
        const unsigned char byte = static_cast<unsigned char>(ch);
        if (std::isalnum(byte) || ch == '_') {
            continue;
        }
        throw utils::error::QueryError("unsafe identifier: " + std::string(identifier));
    }
}

std::string sanitize_identifier(std::string_view identifier) {
    ensure_identifier(identifier);
    return std::string(identifier);
}

}  // namespace sqluna::security::sanitizer
