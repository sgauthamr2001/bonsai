#pragma once

#include "Expr.h"

namespace bonsai {

const int64_t *as_const_int(const Expr &e);

Expr make_zero(const Type &t);
Expr make_one(const Type &t);

}  // namespace bonsai
