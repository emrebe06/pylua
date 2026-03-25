#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "lunara/ast.hpp"
#include "lunara/runtime.hpp"

namespace lunara {

class Interpreter {
  public:
    explicit Interpreter(std::filesystem::path entry_script = {}, std::ostream* output = nullptr);

    void interpret(const ast::Program& program);
    void execute_block(const std::vector<ast::StmtPtr>& statements, std::shared_ptr<runtime::Environment> environment);

  private:
    void execute(const ast::Stmt& statement);
    runtime::Value evaluate(const ast::Expr& expression);
    runtime::Value load_module(const std::string& module_name);
    std::filesystem::path resolve_module_path(const std::string& module_name) const;
    runtime::Value run_module_file(const std::filesystem::path& path);

    std::shared_ptr<runtime::Environment> globals_;
    std::shared_ptr<runtime::Environment> environment_;
    std::unordered_map<std::string, runtime::Value> stdlib_modules_;
    std::unordered_map<std::string, runtime::Value> module_cache_;
    std::vector<std::shared_ptr<ast::Program>> owned_programs_;
    std::vector<std::filesystem::path> script_stack_;
    std::vector<std::vector<std::pair<const ast::Expr*, std::shared_ptr<runtime::Environment>>>> deferred_blocks_;
    std::ostream* output_;
};

}  // namespace lunara

