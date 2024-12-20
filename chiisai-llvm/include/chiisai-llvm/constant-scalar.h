//
// Created by creeper on 10/13/24.
//

#ifndef CACTRIE_CHIISAI_LLVM_INCLUDE_CHIISAI_LLVM_CONSTANT_SCALAR_H
#define CACTRIE_CHIISAI_LLVM_INCLUDE_CHIISAI_LLVM_CONSTANT_SCALAR_H
#include <chiisai-llvm/constant.h>
namespace llvm {

struct ConstantScalar : Constant {
  ConstantScalar(const std::string &name, CRef<Type> type) : Constant(name, type) {}
};

}
#endif //CACTRIE_CHIISAI_LLVM_INCLUDE_CHIISAI_LLVM_CONSTANT_SCALAR_H
