#pragma once

#include "Stmt.h"
#include "Type.h"

#include <optional>

namespace bonsai {
namespace ir {

struct Function {
    std::string name;
    struct Argument {
        std::string name;
        Type type;
        Expr default_value;
    };
    std::vector<Argument> args;
    Type ret_type;
    Stmt body;

    Function() {}

    Function(std::string _name, std::vector<Argument> _args, Type _ret_type, Stmt _body)
        : name(std::move(_name)), args(std::move(_args)), ret_type(std::move(_ret_type)), body(std::move(_body)) {}
};

}  // namespace ir
}  // namespace bonsai
