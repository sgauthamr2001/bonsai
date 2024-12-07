#include "IR/Expr.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "IR/IREquality.h"
#include "IR/IRPrinter.h"

namespace bonsai {
namespace ir {

Expr::Expr(int8_t x)
    : IRHandle(IntImm::make(Int_t::make(8), x)) {
}

Expr::Expr(int16_t x)
    : IRHandle(IntImm::make(Int_t::make(16), x)) {
}

Expr::Expr(int32_t x)
    : IRHandle(IntImm::make(Int_t::make(32), x)) {
}

Expr::Expr(int64_t x)
    : IRHandle(IntImm::make(Int_t::make(64), x)) {
}

const Expr IntImm::make(Type t, int64_t value) {
    // TODO: assert safety.
    // internal_assert(t.is_int() && t.is_scalar())
    //     << "IntImm must be a scalar Int\n";
    // internal_assert(t.bits() >= 1 && t.bits() <= 64)
    //     << "IntImm must have between 1 and 64 bits\n";

    // Normalize the value by dropping the high bits.
    // Since left-shift of negative value is UB in C++, cast to uint64 first;
    // it's unlikely any compilers we care about will misbehave, but UBSan will complain.
    value = (int64_t)(((uint64_t)value) << (64 - t.bits()));

    // Then sign-extending to get them back
    value >>= (64 - t.bits());

    IntImm *node = new IntImm;
    node->type = t;
    node->value = value;
    return node;
}

const Expr FloatImm::make(Type t, double value) {
    // internal_assert(t.is_float() && t.is_scalar())
    //     << "FloatImm must be a scalar Float\n";
    FloatImm *node = new FloatImm;
    node->type = t;
    switch (t.bits()) {
    case 16:

    case 32:
        node->value = (float)value;
        break;
    case 64:
        node->value = value;
        break;
    default:
        throw std::runtime_error("FloatImm must be f32 or f64");
    }

    return node;
}

Expr Var::make(Type type, const std::string &name) {
    // internal_assert(!name.empty());
    Var *node = new Var;
    node->type = type;
    node->name = name;
    return node;
}

bool BinOp::is_numeric_op(const BinOp::OpType &op) {
    switch (op) {
        case BinOp::Add:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Sub: return true;
        case BinOp::Eq:
        case BinOp::Le:
        case BinOp::Lt: return false;
    }
}

bool BinOp::is_boolean_op(const BinOp::OpType &op) {
    switch (op) {
        case BinOp::Add:
        case BinOp::Mul:
        case BinOp::Div:
        case BinOp::Sub: return false;
        case BinOp::Eq:
        case BinOp::Le:
        case BinOp::Lt: return true;
    }
}

Expr BinOp::make(BinOp::OpType op, Expr a, Expr b) {
    // TODO: operator overloading and broadcasting!
    if (!a.defined() || !b.defined()) {
        throw std::runtime_error("BinOp of undefined: " + to_string(a) + " op " + to_string(b));
    }
    if (!equals(a.type(), b.type())) {
        throw std::runtime_error("BinOp of mismatched types: " + to_string(a) + " op " + to_string(b));
    }

    BinOp *node = new BinOp;
    node->op = op;
    if (BinOp::is_numeric_op(op)) {
        node->type = a.type();
    } else if (BinOp::is_boolean_op(op)) {
        node->type = a.type().to_bool();
    } else {
        throw std::runtime_error("Cannot infer output type: " + to_string(a) + to_string(op) + to_string(b));
    }

    node->a = std::move(a);
    node->b = std::move(b);
    return node;
}

Expr Broadcast::make(uint32_t lanes, Expr value) {
    if (!value.defined()) {
        throw std::runtime_error("Broadcast of undefined: " + to_string(value));
    }
    if (!value.type().is_scalar()) {
        // TODO: support.
        throw std::runtime_error("Broadcast of non-scalar: " + to_string(value));
    }
    Broadcast *node = new Broadcast;
    node->type = Vector_t::make(value.type(), lanes);
    node->lanes = lanes;
    node->value = std::move(value);
    return node;
}

Expr VectorReduce::make(VectorReduce::OpType op, Expr value) {
    if (!value.defined()) {
        throw std::runtime_error("VectorReduce of undefined: " + to_string(value));
    }
    if (!value.type().is_vector()) {
        // TODO: support.
        throw std::runtime_error("VectorReduce of non-vector: " + to_string(value));
    }
    VectorReduce *node = new VectorReduce;
    node->type = value.type().element_of();
    node->op = op;
    node->value = std::move(value);
    return node;
}

Expr Ramp::make(Expr base, Expr stride, int lanes) {
    // TODO: would be far easier with internal_assert
    if (!base.defined()) {
        throw std::runtime_error("Ramp of undefined (base)");
    }
    if (!stride.defined()) {
        throw std::runtime_error("Ramp of undefined (stride)");
    }
    if (lanes <= 1) {
        throw std::runtime_error("Ramp of lanes 1 >= " + std::to_string(lanes));
    }
    if (!equals(stride.type(), base.type())) {
        throw std::runtime_error("Ramp of mismatched types: " + to_string(stride) + " vs " + to_string(base));
    }

    Ramp *node = new Ramp;
    node->type = Vector_t::make(base.type(), lanes);
    node->base = std::move(base);
    node->stride = std::move(stride);
    node->lanes = lanes;
    return node;
}

Expr Build::make(Type type, std::vector<Expr> values) {
    if (!type.defined()) {
        throw std::runtime_error("Build of undefined Type");
    }
    if (values.empty()) {
        throw std::runtime_error("Build with no values, of type: " + to_string(type));
    }
    for (const auto& expr : values) {
        if (!expr.defined()) {
            throw std::runtime_error("Build with undefined field");
        }
    }

    if (type.is<Vector_t>()) {
        if (type.as<Vector_t>()->lanes != values.size()) {
            throw std::runtime_error("Build<Vector_t> with incorrect number of arguments, expected: " + to_string(type) + " but recieved " + std::to_string(values.size()) + " elements.");
        }
        Type etype = type.as<Vector_t>()->etype;
        for (const auto& expr : values) {
            if (!equals(expr.type(), etype)) {
                throw std::runtime_error("Build<Vector_t> requires uniform element type, expected: " + to_string(etype) + " but recieved " + to_string(expr.type()));
            }
        }
    } else if (type.is<Struct_t>()) {
        // Struct_t
        if (type.as<Struct_t>()->fields.size() != values.size()) {
            throw std::runtime_error("Build<Struct_t> with incorrect number of arguments, expected: " + to_string(type) + " but recieved " + std::to_string(values.size()) + " elements.");
        }
        const auto &fields = type.as<Struct_t>()->fields;
        for (size_t i = 0; i < values.size(); i++) {
            if (!equals(fields[i].second, values[i].type())) {
                throw std::runtime_error("Build<Vector_t> requires matching field types, expected: " + to_string(fields[i].second) + " but recieved " + to_string(values[i].type()) + " from value " + to_string(values[i]));
            }
        }
    } else {
        throw std::runtime_error("Build with non-(vector, struct) type: " + to_string(type));
    }

    Build *node = new Build;
    node->type = std::move(type);
    node->values = std::move(values);
    return node;
}

Expr Access::make(std::string field, Expr value) {
    if (field.empty()) {
        throw std::runtime_error("Access with empty field");
    }
    if (!value.defined()) {
        throw std::runtime_error("Access with undefined value");
    }

    if (const Struct_t *as_struct = value.type().as<Struct_t>()) {
        Type etype;
        for (const auto& [key, value] : as_struct->fields) {
            if (key == field) {
                etype = value;
                break;
            }
        }
        if (!etype.defined()) {
            throw std::runtime_error("Access with field name not in struct: " + field + " of " + to_string(value.type()));
        }
        Access *node = new Access;
        node->type = std::move(etype);
        node->field = std::move(field);
        node->value = std::move(value);
        return node;
    } else {
        // TODO: also support UnorderedStruct_t
        throw std::runtime_error("Access of non-struct: " + to_string(value));
    }
}

}  // namespace ir
}  // namespace bonsai
