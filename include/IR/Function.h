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
        std::optional<Expr> default_value;
    };
    std::vector<Argument> args;
    Type return_t;
    Stmt body;

    Function() {}

    Function(std::string _name, std::vector<Argument> _args, Type _return_t, Stmt _body)
        : name(std::move(_name)), args(std::move(_args)), return_t(std::move(_return_t)), body(std::move(_body)) {}
};

}  // namespace ir
}  // namespace bonsai
