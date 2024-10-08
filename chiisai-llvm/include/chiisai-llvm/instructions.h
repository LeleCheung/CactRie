//
// Created by creeper on 8/23/24.
//

#ifndef CACTRIE_CACT_RIE_INCLUDE_CACT_RIE_LLVM_INSTRUCTIONS_H
#define CACTRIE_CACT_RIE_INCLUDE_CACT_RIE_LLVM_INSTRUCTIONS_H
#include <variant>
#include <chiisai-llvm/literal.h>
#include <chiisai-llvm/llvm-ir.h>
namespace llvm {

struct Operand {
  [[nodiscard]] bool isLiteral() const {
    return std::holds_alternative<Literal>(operand);
  }
  [[nodiscard]] bool isVariable() const {
    return std::holds_alternative<std::reference_wrapper<Variable>>(operand);
  }
  Variable &variable() {
    return std::get<std::reference_wrapper<Variable>>(operand);
  }
  Literal literal() {
    return std::get<Literal>(operand);
  }
  std::variant<std::reference_wrapper<Variable>, Literal> operand;
};

struct ArithInst : public Instruction {

  Variable &result;
  Operand lhs;
  Operand rhs;
  BinaryOp op;
};

struct CmpInst : public Instruction {
  Variable &result;
  Operand lhs;
  Operand rhs;
  BinaryOp op;
};

struct MemoryInst : public Instruction {
  LLVMType type;
  Variable &result;
  Variable &pointer;
  size_t alignment;
  MemoryOp op;
};

}
#endif //CACTRIE_CACT_RIE_INCLUDE_CACT_RIE_LLVM_INSTRUCTIONS_H
