#include "IR/Expr.h"
#include "IR/Stmt.h"
#include "IR/Type.h"
#include "IR/IRPrinter.h"
#include "CodeGen_LLVM.h"

#include <iostream>

using namespace bonsai::ir;
using namespace bonsai;

void test_example() {
    Type f32 = Float_t::make(32);
    Expr _1 = FloatImm::make(f32, 1.0);
    Expr a = Var::make(f32, "a");
    Expr ap1 = BinOp::make(BinOp::Add, a, _1);
    std::cout << ap1 << std::endl;
}

int main(void) {
    test_example();
}
