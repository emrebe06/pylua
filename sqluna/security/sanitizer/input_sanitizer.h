#pragma once

#include <string>
#include <string_view>

namespace sqluna::security::sanitizer {

void ensure_identifier(std::string_view identifier);
std::string sanitize_identifier(std::string_view identifier);

}  // namespace sqluna::security::sanitizer
