#pragma once

#include "IR/Expr.h"

namespace bonsai {

const int64_t *as_const_int(const ir::Expr &e);
bool is_const_one(const ir::Expr &e);

ir::Expr make_zero(const ir::Type &t);
ir::Expr make_one(const ir::Type &t);

bool is_power_of_two(int32_t x);
int32_t next_power_of_two(int32_t x);

size_t find_struct_index(const std::string &field, const ir::Struct_t::Map &fields);

}  // namespace bonsai
