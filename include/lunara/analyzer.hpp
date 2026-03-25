#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "lunara/ast.hpp"

namespace lunara::analysis {

struct Diagnostic {
    std::string message;
};

std::vector<Diagnostic> analyze_program(const ast::Program& program);
std::vector<Diagnostic> analyze_file(const std::filesystem::path& path);

}  // namespace lunara::analysis
