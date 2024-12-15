#pragma once

#include "IR/Program.h"

namespace bonsai {
namespace lower {

ir::Program infer_types(const ir::Program &program);

}  // namespace parser
}  // namespace bonsai
