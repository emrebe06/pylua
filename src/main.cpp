#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "lunara/engine.hpp"

namespace {

void print_help() {
    std::cout
        << "Lunara CLI\n"
        << "Usage:\n"
        << "  lunara <script.lunara>\n"
        << "  lunara run <script.lunara>\n"
        << "  lunara vm <script.lunara>\n"
        << "  lunara check <script.lunara>\n"
        << "  lunara version\n"
        << "  lunara help\n";
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
            std::cout << "lunara 0.1.0\n";
            return 0;
        }

        if (argc == 2) {
            lunara::engine::run_file(argv[1], lunara::engine::Backend::Interpreter, std::cout);
            return 0;
        }

        const std::filesystem::path script_path = argv[2];

        if (command == "run") {
            lunara::engine::run_file(script_path, lunara::engine::Backend::Interpreter, std::cout);
            return 0;
        }

        if (command == "vm") {
            lunara::engine::run_file(script_path, lunara::engine::Backend::Vm, std::cout);
            return 0;
        }

        if (command == "check") {
            lunara::engine::check_file(script_path);
            std::cout << "OK: " << script_path.string() << '\n';
            return 0;
        }

        throw std::runtime_error("unknown command: " + command);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}

