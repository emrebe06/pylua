#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lunara/runtime.hpp"

namespace lunara::bytecode {

enum class OpCode : std::uint8_t {
    LoadConst,
    LoadName,
    DefineGlobal,
    StoreName,
    Pop,
    BuildList,
    BuildObject,
    GetMember,
    GetIndex,
    Add,
    Sub,
    Mul,
    Div,
    Negate,
    Not,
    Equal,
    NotEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    JumpIfFalse,
    Jump,
    Print,
    Halt,
};

struct Instruction {
    OpCode op;
    std::size_t operand = 0;
};

struct ObjectShape {
    std::vector<std::string> keys;
};

struct Chunk {
    std::vector<Instruction> code;
    std::vector<runtime::Value> constants;
    std::vector<std::string> names;
    std::vector<ObjectShape> object_shapes;
};

std::string opcode_name(OpCode op);

}  // namespace lunara::bytecode

