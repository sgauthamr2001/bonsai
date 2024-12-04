#include "Utils.h"

#include "IRPrinter.h"

namespace bonsai {

const int64_t *as_const_int(const Expr &e) {
    if (!e.defined()) {
        return nullptr;
    // } else if (const Broadcast *b = e.as<Broadcast>()) {
    //     return as_const_int(b->value);
    } else if (const IntImm *i = e.as<IntImm>()) {
        return &(i->value);
    } else {
        return nullptr;
    }
}

Expr make_zero(const Type &t) {
    if (t.is_vector()) {
        throw std::runtime_error("TODO: handle vector types in make_zero");
    }
    if (t.is_float()) {
        return FloatImm::make(t, 0.0f);
    } else if (t.is_int()) {
        return IntImm::make(t, 0);
    } else {
        throw std::runtime_error("TODO: handle " + to_string(t) + " in make_zero");
    }
}

Expr make_one(const Type &t) {
    if (t.is_vector()) {
        throw std::runtime_error("TODO: handle vector types in make_one");
    }
    if (t.is_float()) {
        return FloatImm::make(t, 1.0f);
    } else if (t.is_int()) {
        return IntImm::make(t, 1);
    } else {
        throw std::runtime_error("TODO: handle " + to_string(t) + " in make_one");
    }
}

} // namespace bonsai
