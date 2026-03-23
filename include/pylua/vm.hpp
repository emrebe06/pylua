#pragma once

#include <ostream>

#include "pylua/ast.hpp"
#include "pylua/bytecode.hpp"

namespace pylua {

class VmCompiler {
  public:
    bytecode::Chunk compile(const ast::Program& program);
};

class VirtualMachine {
  public:
    explicit VirtualMachine(std::ostream* output = nullptr);

    void execute(const bytecode::Chunk& chunk);

  private:
    std::ostream* output_;
};

}  // namespace pylua
