#include "Utils.h"

#include "IR/IRPrinter.h"

namespace bonsai {

using namespace ir;

const int64_t *as_const_int(const Expr &e) {
    if (!e.defined()) {
        return nullptr;
    } else if (const Broadcast *b = e.as<Broadcast>()) {
        return as_const_int(b->value);
    } else if (const IntImm *i = e.as<IntImm>()) {
        return &(i->value);
    } else {
        return nullptr;
    }
}

bool is_const_one(const Expr &e) {
    if (!e.defined()) {
        internal_error << "is_const_one called on undefined value";
        return false;
    } else if (const Broadcast *b = e.as<Broadcast>()) {
        return is_const_one(b->value);
    } else if (const IntImm *i = e.as<IntImm>()) {
        return i->value == 1;
    } else if (const UIntImm *u = e.as<UIntImm>()) {
        return u->value == 1;
    } else if (const FloatImm *f = e.as<FloatImm>()) {
        return f->value == 1.f;
    } else {
        return false;
    }
}

bool is_const(const Expr &e) {
    if (!e.defined()) {
        internal_error << "is_const called on undefined value";
        return false;
    } else if (const Broadcast *b = e.as<Broadcast>()) {
        return is_const(b->value);
    } else {
        return e.is<IntImm>() || e.is<UIntImm>() || e.is<FloatImm>(); // TODO: bools
    }
}

Expr make_zero(const Type &t) {
    return make_const(t, 0);
}

Expr make_one(const Type &t) {
    return make_const(t, 1);
}

bool is_power_of_two(int32_t x) {
    return (x & (x - 1)) == 0;
}

int32_t next_power_of_two(int32_t x) {
    return static_cast<int32_t>(1) << static_cast<int32_t>(std::ceil(std::log2(x)));
}

size_t find_struct_index(const std::string &field, const Struct_t::Map &fields) {
    for (size_t i = 0; i < fields.size(); i++) {
        if (field == fields[i].first) {
            return i;
        }
    }
    internal_error << "find_struct_index did not find field " << field;
    return 0;
}

}  // namespace bonsai
