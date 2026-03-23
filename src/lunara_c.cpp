#include "lunara/lunara_c.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

#include "lunara/engine.hpp"

namespace {

char* copy_c_string(const std::string& text) {
    char* result = static_cast<char*>(std::malloc(text.size() + 1));
    if (!result) {
        return nullptr;
    }
    std::memcpy(result, text.c_str(), text.size() + 1);
    return result;
}

}  // namespace

extern "C" {

const char* lunara_run_file(const char* script_path, const char* backend, int* exit_code) {
    std::ostringstream output;
    try {
        const auto parsed_backend = lunara::engine::parse_backend(backend ? backend : "interpreter");
        lunara::engine::run_file(script_path, parsed_backend, output);
        if (exit_code) {
            *exit_code = 0;
        }
        return copy_c_string(output.str());
    } catch (const std::exception& ex) {
        if (exit_code) {
            *exit_code = 1;
        }
        return copy_c_string(ex.what());
    }
}

const char* lunara_run_source(const char* source, const char* virtual_path, const char* backend, int* exit_code) {
    std::ostringstream output;
    try {
        const auto parsed_backend = lunara::engine::parse_backend(backend ? backend : "interpreter");
        lunara::engine::run_source(source ? source : "", virtual_path ? virtual_path : "<memory>", parsed_backend, output);
        if (exit_code) {
            *exit_code = 0;
        }
        return copy_c_string(output.str());
    } catch (const std::exception& ex) {
        if (exit_code) {
            *exit_code = 1;
        }
        return copy_c_string(ex.what());
    }
}

void lunara_string_free(const char* value) {
    std::free(const_cast<char*>(value));
}

}

