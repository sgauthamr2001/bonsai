#include "Expr.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "IREquality.h"
#include "IRPrinter.h"

namespace bonsai {


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
    std::cout << "Making IntImm: " << value << "\n";
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

} // namespace bonsai
