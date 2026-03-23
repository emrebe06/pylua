#include "lunara/engine.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "lunara/interpreter.hpp"
#include "lunara/lexer.hpp"
#include "lunara/parser.hpp"
#include "lunara/vm.hpp"

namespace lunara::engine {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

ast::Program parse_program(const std::string& source) {
    Lexer lexer(source);
    auto tokens = lexer.scan_tokens();
    Parser parser(std::move(tokens));
    return parser.parse();
}

}  // namespace

Backend parse_backend(const std::string& raw) {
    if (raw == "interp" || raw == "interpreter" || raw == "run") {
        return Backend::Interpreter;
    }
    if (raw == "vm") {
        return Backend::Vm;
    }
    throw std::runtime_error("unknown backend: " + raw);
}

std::string backend_name(Backend backend) {
    return backend == Backend::Vm ? "vm" : "interpreter";
}

void check_file(const std::filesystem::path& script_path) {
    static_cast<void>(parse_program(read_file(script_path)));
}

void run_source(const std::string& source, const std::filesystem::path& virtual_path, Backend backend, std::ostream& output) {
    auto program = parse_program(source);

    if (backend == Backend::Vm) {
        try {
            VmCompiler compiler;
            auto chunk = compiler.compile(program);
            VirtualMachine vm(&output);
            vm.execute(chunk);
            return;
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            if (message.rfind("vm backend does not yet support", 0) == 0) {
                Interpreter interpreter(virtual_path, &output);
                interpreter.interpret(program);
                return;
            }
            throw;
        }
    }

    Interpreter interpreter(virtual_path, &output);
    interpreter.interpret(program);
}

void run_file(const std::filesystem::path& script_path, Backend backend, std::ostream& output) {
    run_source(read_file(script_path), script_path, backend, output);
}

}  // namespace lunara::engine

