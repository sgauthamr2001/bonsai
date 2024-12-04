#pragma once

#include "Expr.h"

namespace bonsai {

const int64_t *as_const_int(const Expr &e);
bool is_const_one(const Expr &e);

Expr make_zero(const Type &t);
Expr make_one(const Type &t);

bool is_power_of_two(int32_t x);
int32_t next_power_of_two(int32_t x);

}  // namespace bonsai
