#pragma once

#include <map>
#include <iostream>

#include "Function.h"
#include "Type.h"

namespace bonsai {
namespace ir {

Expr operator+(Expr a, Expr b);
Expr operator-(Expr a, Expr b);
Expr operator*(Expr a, Expr b);
Expr operator/(Expr a, Expr b);
Expr operator&&(Expr a, Expr b);
Expr operator||(Expr a, Expr b);
Expr operator^(Expr a, Expr b);
Expr operator==(Expr a, Expr b);
Expr operator<=(Expr a, Expr b);
Expr operator<(Expr a, Expr b);
// TODO: the rest

}  // namespace ir
}  // namespace bonsai
