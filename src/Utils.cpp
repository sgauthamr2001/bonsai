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
    // TODO: unsigned integers
    } else {
        return false;
    }
}

Expr make_zero(const Type &t) {
    if (t.is_vector()) {
        Expr inner = make_zero(t.element_of());
        return Broadcast::make(t.as<Vector_t>()->lanes, inner);
    }
    if (t.is_float()) {
        return FloatImm::make(t, 0.0f);
    } else if (t.is_int()) {
        return IntImm::make(t, 0);
    } else {
        internal_error << "TODO: handle: " << t << " in make_zero";
        return Expr();
    }
}

Expr make_one(const Type &t) {
    if (t.is_vector()) {
        Expr inner = make_one(t.element_of());
        return Broadcast::make(t.as<Vector_t>()->lanes, inner);
    }
    if (t.is_float()) {
        return FloatImm::make(t, 1.0f);
    } else if (t.is_int()) {
        return IntImm::make(t, 1);
    } else {
        internal_error << "TODO: handle: " << t << " in make_one";
        return Expr();
    }
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

} // namespace bonsai
