#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "pylua/engine.hpp"

namespace {

void print_help() {
    std::cout
        << "PyLua CLI\n"
        << "Usage:\n"
        << "  pylua <script.pylua>\n"
        << "  pylua run <script.pylua>\n"
        << "  pylua vm <script.pylua>\n"
        << "  pylua check <script.pylua>\n"
        << "  pylua version\n"
        << "  pylua help\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc <= 1) {
            print_help();
            return 1;
        }

        const std::string command = argv[1];
        if (command == "help" || command == "--help" || command == "-h") {
            print_help();
            return 0;
        }

        if (command == "version" || command == "--version") {
            std::cout << "pylua 0.1.0\n";
            return 0;
        }

        if (argc == 2) {
            pylua::engine::run_file(argv[1], pylua::engine::Backend::Interpreter, std::cout);
            return 0;
        }

        const std::filesystem::path script_path = argv[2];

        if (command == "run") {
            pylua::engine::run_file(script_path, pylua::engine::Backend::Interpreter, std::cout);
            return 0;
        }

        if (command == "vm") {
            pylua::engine::run_file(script_path, pylua::engine::Backend::Vm, std::cout);
            return 0;
        }

        if (command == "check") {
            pylua::engine::check_file(script_path);
            std::cout << "OK: " << script_path.string() << '\n';
            return 0;
        }

        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
