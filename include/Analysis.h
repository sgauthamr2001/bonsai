#pragma once

#include "Stmt.h"

namespace bonsai {

std::vector<std::pair<std::string, Type>> gather_free_vars(const Expr &expr);
std::vector<std::pair<std::string, Type>> gather_free_vars(const Stmt &stmt);

bool always_returns(const Stmt &stmt);
Type get_return_type(const Stmt &stmt);

std::vector<const Struct_t *> gather_struct_types(const Stmt &stmt);

} // namespace bonsai
