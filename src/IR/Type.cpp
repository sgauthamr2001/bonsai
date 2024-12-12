#include "IR/Type.h"

#include <utility>
#include <stdexcept>

#include "IR/IRPrinter.h"

namespace bonsai {
namespace ir {

uint32_t Type::bits() const {
    if (auto as_int = this->as<Int_t>()) {
        return as_int->bits;
    } else if (auto as_float = this->as<Float_t>()) {
        return as_float->bits;
    } else {
        throw std::runtime_error("Called bits() on bad type: " + to_string(*this));
    }
}

uint32_t Type::lanes() const {
    if (auto as_vec = this->as<Vector_t>()) {
        // TODO: handle recursive vectors?
        return as_vec->lanes;
    } else {
        throw std::runtime_error("Called lanes() on bad type: " + to_string(*this));
    }
}

bool Type::is_int() const {
    return this->is<Int_t>() ||
           (this->is<Vector_t>() && this->as<Vector_t>()->etype.is_int());
}

bool Type::is_float() const {
    return this->is<Float_t>() ||
           (this->is<Vector_t>() && this->as<Vector_t>()->etype.is_float());
}

bool Type::is_bool() const {
    return this->is<Bool_t>() ||
           (this->is<Vector_t>() && this->as<Vector_t>()->etype.is_bool());
}

bool Type::is_scalar() const {
    // TODO: what counts as scalar?
    return this->is<Int_t>() || this->is<Float_t>() || this->is<Bool_t>();
}

bool Type::is_vector() const {
    // TODO: what counts as vector?
    return this->is<Vector_t>();
}

bool Type::is_numeric() const {
    // scalar + vector of numbers
    // TODO: let Struct_ts overload their numeric operators.
    return this->is_int() || this->is_float();
}

bool Type::is_callable() const {
    throw std::runtime_error("TODO: implement Type::is_callable()");
}

Type Type::to_bool() const {
    if (this->is<Int_t>() || this->is<Float_t>() || this->is<UInt_t>()) {
        return Bool_t::make();
    } else if (this->is<Vector_t>()) {
        const Vector_t *v = this->as<Vector_t>();
        return Vector_t::make(v->etype.to_bool(), v->lanes);
    } else {
        throw std::runtime_error("Called to_bool() on bad type: " + to_string(*this));
    }
}

Type Type::to_uint() const {
    if (this->is<Int_t>()) {
        return UInt_t::make(this->as<Int_t>()->bits);
    } else if (this->is<Float_t>()) {
        return UInt_t::make(this->as<Float_t>()->bits);
    } else if (this->is<Vector_t>()) {
        const Vector_t *v = this->as<Vector_t>();
        return Vector_t::make(v->etype.to_uint(), v->lanes);
    } else {
        throw std::runtime_error("Called to_uint() on bad type: " + to_string(*this));
    }
}

Type Type::element_of() const {
    if (this->is<Vector_t>()) {
        return this->as<Vector_t>()->etype;
    } else if (this->is<Set_t>()) {
        return this->as<Set_t>()->etype;
    } else {
        throw std::runtime_error("Called element_of() on bad type: " + to_string(*this));
    }
}

Type Int_t::make(uint32_t bits) {
    // TODO: consider caching common bit-widths, here and below.
    // TODO: assert safety.
    Int_t *node = new Int_t;
    node->bits = bits;
    return node;
}

Type UInt_t::make(uint32_t bits) {
    // TODO: assert safety.
    UInt_t *node = new UInt_t;
    node->bits = bits;
    return node;
}

Type Float_t::make(uint32_t bits) {
    // TODO: assert safety.
    Float_t *node = new Float_t;
    node->bits = bits;
    return node;
}

Type Bool_t::make() {
    static Bool_t *global_bool = new Bool_t;
    return global_bool;
}

Type Ptr_t::make(Type etype) {
    // TODO: assert safety?
    Ptr_t *node = new Ptr_t;
    node->etype = std::move(etype);
    return node;
}

Type Vector_t::make(Type etype, uint32_t lanes) {
    // TODO: assert safety?
    Vector_t *node = new Vector_t;
    node->etype = std::move(etype);
    node->lanes = lanes;
    return node;
}

Type Struct_t::make(std::string name, Struct_t::Map fields) {
    // TODO: assert safety?
    Struct_t *node = new Struct_t;
    node->name = std::move(name);
    node->fields = std::move(fields);
    return node;
}

Type Tuple_t::make(std::vector<Type> etypes) {
    // TODO: assert safety
    for (const auto &type : etypes) {
        assert(type.defined());
    }
    Tuple_t *node = new Tuple_t;
    node->etypes = std::move(etypes);
    return node;
}

Type Option_t::make(Type etype) {
    // TODO: assert safety?
    assert(etype.defined());
    Option_t *node = new Option_t;
    node->etype = std::move(etype);
    return node;
}

Type Set_t::make(Type etype) {
    // TODO: assert safety?
    assert(etype.defined());
    Set_t *node = new Set_t;
    node->etype = std::move(etype);
    return node;
}

Type Function_t::make(Type ret_type, std::vector<Type> arg_types) {
    // TODO: assert safety
    assert(ret_type.defined());
    for (const auto &type : arg_types) {
        assert(type.defined());
    }
    Function_t *node = new Function_t;
    node->ret_type = std::move(ret_type);
    node->arg_types = std::move(arg_types);
    return node;
}

}  // namespace ir
}  // namespace bonsai
