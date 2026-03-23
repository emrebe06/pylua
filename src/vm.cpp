#include "lunara/vm.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lunara::bytecode {

std::string opcode_name(OpCode op) {
    switch (op) {
        case OpCode::LoadConst: return "LoadConst";
        case OpCode::LoadName: return "LoadName";
        case OpCode::DefineGlobal: return "DefineGlobal";
        case OpCode::StoreName: return "StoreName";
        case OpCode::Pop: return "Pop";
        case OpCode::BuildList: return "BuildList";
        case OpCode::BuildObject: return "BuildObject";
        case OpCode::GetMember: return "GetMember";
        case OpCode::GetIndex: return "GetIndex";
        case OpCode::Add: return "Add";
        case OpCode::Sub: return "Sub";
        case OpCode::Mul: return "Mul";
        case OpCode::Div: return "Div";
        case OpCode::Negate: return "Negate";
        case OpCode::Not: return "Not";
        case OpCode::Equal: return "Equal";
        case OpCode::NotEqual: return "NotEqual";
        case OpCode::Greater: return "Greater";
        case OpCode::GreaterEqual: return "GreaterEqual";
        case OpCode::Less: return "Less";
        case OpCode::LessEqual: return "LessEqual";
        case OpCode::JumpIfFalse: return "JumpIfFalse";
        case OpCode::Jump: return "Jump";
        case OpCode::Print: return "Print";
        case OpCode::Halt: return "Halt";
    }
    return "Unknown";
}

}  // namespace lunara::bytecode

namespace lunara {

namespace {

using runtime::RuntimeError;
using runtime::Value;

class VmCompileError : public std::runtime_error {
  public:
    explicit VmCompileError(const std::string& message) : std::runtime_error(message) {}
};

class CompilerImpl {
  public:
    bytecode::Chunk compile(const ast::Program& program) {
        for (const auto& statement : program.statements) {
            emit_statement(*statement);
        }
        emit(bytecode::OpCode::Halt);
        return std::move(chunk_);
    }

  private:
    std::size_t add_constant(const Value& value) {
        chunk_.constants.push_back(value);
        return chunk_.constants.size() - 1;
    }

    std::size_t add_name(const std::string& name) {
        for (std::size_t i = 0; i < chunk_.names.size(); ++i) {
            if (chunk_.names[i] == name) {
                return i;
            }
        }
        chunk_.names.push_back(name);
        return chunk_.names.size() - 1;
    }

    std::size_t emit(bytecode::OpCode op, std::size_t operand = 0) {
        chunk_.code.push_back(bytecode::Instruction{op, operand});
        return chunk_.code.size() - 1;
    }

    std::size_t emit_jump(bytecode::OpCode op) {
        return emit(op, 0);
    }

    void patch_jump(std::size_t instruction_index) {
        chunk_.code[instruction_index].operand = chunk_.code.size();
    }

    void emit_statement(const ast::Stmt& statement) {
        if (const auto* expr_stmt = dynamic_cast<const ast::ExpressionStmt*>(&statement)) {
            if (try_emit_print_stmt(*expr_stmt)) {
                return;
            }
            emit_expression(*expr_stmt->expression);
            emit(bytecode::OpCode::Pop);
            return;
        }

        if (const auto* var_stmt = dynamic_cast<const ast::VarStmt*>(&statement)) {
            emit_expression(*var_stmt->initializer);
            emit(bytecode::OpCode::DefineGlobal, add_name(var_stmt->name));
            return;
        }

        if (const auto* if_stmt = dynamic_cast<const ast::IfStmt*>(&statement)) {
            emit_if_statement(*if_stmt);
            return;
        }

        if (const auto* while_stmt = dynamic_cast<const ast::WhileStmt*>(&statement)) {
            const std::size_t loop_start = chunk_.code.size();
            emit_expression(*while_stmt->condition);
            const std::size_t exit_jump = emit_jump(bytecode::OpCode::JumpIfFalse);
            emit(bytecode::OpCode::Pop);
            for (const auto& body_stmt : while_stmt->body) {
                emit_statement(*body_stmt);
            }
            emit(bytecode::OpCode::Jump, loop_start);
            patch_jump(exit_jump);
            emit(bytecode::OpCode::Pop);
            return;
        }

        if (dynamic_cast<const ast::ImportStmt*>(&statement) || dynamic_cast<const ast::FunctionStmt*>(&statement) ||
            dynamic_cast<const ast::ForInStmt*>(&statement) || dynamic_cast<const ast::ReturnStmt*>(&statement)) {
            throw VmCompileError("vm backend does not yet support this statement");
        }

        throw VmCompileError("vm backend encountered an unknown statement");
    }

    bool try_emit_print_stmt(const ast::ExpressionStmt& expr_stmt) {
        const auto* call = dynamic_cast<const ast::CallExpr*>(expr_stmt.expression.get());
        if (!call) {
            return false;
        }

        const auto* callee = dynamic_cast<const ast::VariableExpr*>(call->callee.get());
        if (!callee || callee->name != "print" || call->arguments.size() != 1) {
            return false;
        }

        emit_expression(*call->arguments[0]);
        emit(bytecode::OpCode::Print);
        return true;
    }

    void emit_if_statement(const ast::IfStmt& if_stmt) {
        std::vector<std::size_t> end_jumps;
        std::vector<std::size_t> false_jumps;

        for (const auto& branch : if_stmt.branches) {
            emit_expression(*branch.condition);
            const std::size_t false_jump = emit_jump(bytecode::OpCode::JumpIfFalse);
            emit(bytecode::OpCode::Pop);
            for (const auto& stmt : branch.body) {
                emit_statement(*stmt);
            }
            end_jumps.push_back(emit_jump(bytecode::OpCode::Jump));
            false_jumps.push_back(false_jump);
            patch_jump(false_jump);
            emit(bytecode::OpCode::Pop);
        }

        for (const auto& stmt : if_stmt.else_branch) {
            emit_statement(*stmt);
        }

        for (const std::size_t jump_index : end_jumps) {
            patch_jump(jump_index);
        }
    }

    void emit_expression(const ast::Expr& expression) {
        if (const auto* literal = dynamic_cast<const ast::LiteralExpr*>(&expression)) {
            if (std::holds_alternative<std::monostate>(literal->value)) {
                emit(bytecode::OpCode::LoadConst, add_constant(Value()));
            } else if (const auto* number = std::get_if<double>(&literal->value)) {
                emit(bytecode::OpCode::LoadConst, add_constant(Value(*number)));
            } else if (const auto* boolean = std::get_if<bool>(&literal->value)) {
                emit(bytecode::OpCode::LoadConst, add_constant(Value(*boolean)));
            } else {
                emit(bytecode::OpCode::LoadConst, add_constant(Value(std::get<std::string>(literal->value))));
            }
            return;
        }

        if (const auto* variable = dynamic_cast<const ast::VariableExpr*>(&expression)) {
            emit(bytecode::OpCode::LoadName, add_name(variable->name));
            return;
        }

        if (const auto* grouping = dynamic_cast<const ast::GroupingExpr*>(&expression)) {
            emit_expression(*grouping->expression);
            return;
        }

        if (const auto* assign = dynamic_cast<const ast::AssignExpr*>(&expression)) {
            emit_expression(*assign->value);
            emit(bytecode::OpCode::StoreName, add_name(assign->name));
            return;
        }

        if (const auto* list_expr = dynamic_cast<const ast::ListExpr*>(&expression)) {
            for (const auto& element : list_expr->elements) {
                emit_expression(*element);
            }
            emit(bytecode::OpCode::BuildList, list_expr->elements.size());
            return;
        }

        if (const auto* object_expr = dynamic_cast<const ast::ObjectExpr*>(&expression)) {
            bytecode::ObjectShape shape;
            for (const auto& entry : object_expr->entries) {
                shape.keys.push_back(entry.key);
                emit_expression(*entry.value);
            }
            chunk_.object_shapes.push_back(std::move(shape));
            emit(bytecode::OpCode::BuildObject, chunk_.object_shapes.size() - 1);
            return;
        }

        if (const auto* member_expr = dynamic_cast<const ast::MemberExpr*>(&expression)) {
            emit_expression(*member_expr->object);
            emit(bytecode::OpCode::GetMember, add_name(member_expr->name));
            return;
        }

        if (const auto* index_expr = dynamic_cast<const ast::IndexExpr*>(&expression)) {
            emit_expression(*index_expr->object);
            emit_expression(*index_expr->index);
            emit(bytecode::OpCode::GetIndex);
            return;
        }

        if (const auto* unary = dynamic_cast<const ast::UnaryExpr*>(&expression)) {
            emit_expression(*unary->right);
            switch (unary->op.type) {
                case TokenType::Minus:
                    emit(bytecode::OpCode::Negate);
                    return;
                case TokenType::Not:
                    emit(bytecode::OpCode::Not);
                    return;
                default:
                    break;
            }
        }

        if (const auto* binary = dynamic_cast<const ast::BinaryExpr*>(&expression)) {
            emit_expression(*binary->left);
            emit_expression(*binary->right);
            switch (binary->op.type) {
                case TokenType::Plus: emit(bytecode::OpCode::Add); return;
                case TokenType::Minus: emit(bytecode::OpCode::Sub); return;
                case TokenType::Star: emit(bytecode::OpCode::Mul); return;
                case TokenType::Slash: emit(bytecode::OpCode::Div); return;
                case TokenType::EqualEqual: emit(bytecode::OpCode::Equal); return;
                case TokenType::BangEqual: emit(bytecode::OpCode::NotEqual); return;
                case TokenType::Greater: emit(bytecode::OpCode::Greater); return;
                case TokenType::GreaterEqual: emit(bytecode::OpCode::GreaterEqual); return;
                case TokenType::Less: emit(bytecode::OpCode::Less); return;
                case TokenType::LessEqual: emit(bytecode::OpCode::LessEqual); return;
                default: break;
            }
        }

        throw VmCompileError("vm backend does not yet support this expression");
    }

    bytecode::Chunk chunk_;
};

std::string stringify(const Value& value) {
    return value.to_string();
}

}  // namespace

bytecode::Chunk VmCompiler::compile(const ast::Program& program) {
    CompilerImpl impl;
    return impl.compile(program);
}

VirtualMachine::VirtualMachine(std::ostream* output) : output_(output ? output : &std::cout) {}

void VirtualMachine::execute(const bytecode::Chunk& chunk) {
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;

    auto pop = [&]() -> Value {
        if (stack.empty()) {
            throw RuntimeError("vm stack underflow");
        }
        Value value = stack.back();
        stack.pop_back();
        return value;
    };

    auto peek = [&]() -> Value& {
        if (stack.empty()) {
            throw RuntimeError("vm stack underflow");
        }
        return stack.back();
    };

    std::size_t ip = 0;
    while (ip < chunk.code.size()) {
        const auto instruction = chunk.code[ip++];
        switch (instruction.op) {
            case bytecode::OpCode::LoadConst:
                stack.push_back(chunk.constants[instruction.operand]);
                break;
            case bytecode::OpCode::LoadName: {
                const auto it = globals.find(chunk.names[instruction.operand]);
                if (it == globals.end()) {
                    throw RuntimeError("undefined variable '" + chunk.names[instruction.operand] + "'");
                }
                stack.push_back(it->second);
                break;
            }
            case bytecode::OpCode::DefineGlobal: {
                globals[chunk.names[instruction.operand]] = pop();
                break;
            }
            case bytecode::OpCode::StoreName: {
                const Value value = peek();
                globals[chunk.names[instruction.operand]] = value;
                break;
            }
            case bytecode::OpCode::Pop:
                static_cast<void>(pop());
                break;
            case bytecode::OpCode::BuildList: {
                std::vector<Value> items(instruction.operand);
                for (std::size_t i = instruction.operand; i > 0; --i) {
                    items[i - 1] = pop();
                }
                auto list_value = std::make_shared<runtime::ListData>();
                list_value->items = std::move(items);
                stack.push_back(Value(list_value));
                break;
            }
            case bytecode::OpCode::BuildObject: {
                if (instruction.operand >= chunk.object_shapes.size()) {
                    throw RuntimeError("invalid object shape index");
                }
                const auto& shape = chunk.object_shapes[instruction.operand];
                auto object_value = std::make_shared<runtime::ObjectData>();
                for (std::size_t i = shape.keys.size(); i > 0; --i) {
                    object_value->fields[shape.keys[i - 1]] = pop();
                }
                stack.push_back(Value(object_value));
                break;
            }
            case bytecode::OpCode::GetMember: {
                const Value object = pop();
                if (!object.is_object()) {
                    throw RuntimeError("attempted member access on " + object.type_name());
                }
                const auto object_value = object.as_object();
                const auto it = object_value->fields.find(chunk.names[instruction.operand]);
                if (it == object_value->fields.end()) {
                    throw RuntimeError("object has no field '" + chunk.names[instruction.operand] + "'");
                }
                stack.push_back(it->second);
                break;
            }
            case bytecode::OpCode::GetIndex: {
                const Value index = pop();
                const Value object = pop();
                if (object.is_list()) {
                    const auto list_value = object.as_list();
                    if (!index.is_number()) {
                        throw RuntimeError("list index must be a number");
                    }
                    const double raw_index = index.as_number();
                    if (raw_index < 0 || std::floor(raw_index) != raw_index) {
                        throw RuntimeError("list index must be a non-negative integer");
                    }
                    const std::size_t list_index = static_cast<std::size_t>(raw_index);
                    if (list_index >= list_value->items.size()) {
                        throw RuntimeError("list index out of range");
                    }
                    stack.push_back(list_value->items[list_index]);
                    break;
                }
                if (object.is_object()) {
                    const auto object_value = object.as_object();
                    const auto it = object_value->fields.find(index.as_string());
                    if (it == object_value->fields.end()) {
                        stack.push_back(Value());
                        break;
                    }
                    stack.push_back(it->second);
                    break;
                }
                throw RuntimeError("index access is only supported on list and object values");
            }
            case bytecode::OpCode::Add: {
                const Value rhs = pop();
                const Value lhs = pop();
                if (lhs.is_number() && rhs.is_number()) {
                    stack.push_back(Value(lhs.as_number() + rhs.as_number()));
                } else {
                    stack.push_back(Value(stringify(lhs) + stringify(rhs)));
                }
                break;
            }
            case bytecode::OpCode::Sub: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() - rhs.as_number()));
                break;
            }
            case bytecode::OpCode::Mul: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() * rhs.as_number()));
                break;
            }
            case bytecode::OpCode::Div: {
                const Value rhs = pop();
                const Value lhs = pop();
                if (std::abs(rhs.as_number()) < 1e-12) {
                    throw RuntimeError("division by zero");
                }
                stack.push_back(Value(lhs.as_number() / rhs.as_number()));
                break;
            }
            case bytecode::OpCode::Negate: {
                const Value value = pop();
                stack.push_back(Value(-value.as_number()));
                break;
            }
            case bytecode::OpCode::Not: {
                const Value value = pop();
                stack.push_back(Value(!value.is_truthy()));
                break;
            }
            case bytecode::OpCode::Equal: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs == rhs));
                break;
            }
            case bytecode::OpCode::NotEqual: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs != rhs));
                break;
            }
            case bytecode::OpCode::Greater: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() > rhs.as_number()));
                break;
            }
            case bytecode::OpCode::GreaterEqual: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() >= rhs.as_number()));
                break;
            }
            case bytecode::OpCode::Less: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() < rhs.as_number()));
                break;
            }
            case bytecode::OpCode::LessEqual: {
                const Value rhs = pop();
                const Value lhs = pop();
                stack.push_back(Value(lhs.as_number() <= rhs.as_number()));
                break;
            }
            case bytecode::OpCode::JumpIfFalse:
                if (!peek().is_truthy()) {
                    ip = instruction.operand;
                }
                break;
            case bytecode::OpCode::Jump:
                ip = instruction.operand;
                break;
            case bytecode::OpCode::Print:
                (*output_) << pop().to_string() << '\n';
                break;
            case bytecode::OpCode::Halt:
                return;
        }
    }
}

}  // namespace lunara

