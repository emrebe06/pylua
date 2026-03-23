#pragma once

#include <filesystem>
#include <ostream>
#include <string>

namespace pylua::engine {

enum class Backend {
    Interpreter,
    Vm,
};

Backend parse_backend(const std::string& raw);
std::string backend_name(Backend backend);

void check_file(const std::filesystem::path& script_path);
void run_file(const std::filesystem::path& script_path, Backend backend, std::ostream& output);
void run_source(const std::string& source, const std::filesystem::path& virtual_path, Backend backend, std::ostream& output);

}  // namespace pylua::engine
