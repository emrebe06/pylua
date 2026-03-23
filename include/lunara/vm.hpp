#pragma once

#include <ostream>

#include "lunara/ast.hpp"
#include "lunara/bytecode.hpp"

namespace lunara {

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

}  // namespace lunara

