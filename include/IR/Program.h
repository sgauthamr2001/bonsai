#pragma once

#include <map>

#include "Function.h"
#include "Type.h"

namespace bonsai {
namespace ir {

struct Program {
    // TODO: more things?

    // Intentionally ordered, this will be the order of arguments to the executable.
    std::vector<std::pair<std::string, Type>> externs;
    // All function declarations except for main()
    std::map<std::string, Function> funcs;
    // All types (including aliases).
    std::map<std::string, Type> types;
    // TODO: interfaces / inheritance?

    Stmt main_body;
};

}  // namespace ir
}  // namespace bonsai
