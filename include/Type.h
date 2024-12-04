#pragma once

#include "IntrusivePtr.h"
#include "IRHandle.h"
#include "IRNode.h"
#include "IRVisitor.h"
#include "IRMutator.h"

#include <map>
#include <string>

namespace bonsai {

struct Type;

enum class IRTypeEnum {
    Int_t,
    Float_t,
    Bool_t,
    Ptr_t,
    Vector_t,
    Struct_t,
};

using IRTypeNode = IRNode<Type, IRTypeEnum>;

template<>
inline RefCount &ref_count<IRTypeNode>(const IRTypeNode *t) noexcept {
    return t->ref_count;
}

template<>
inline void destroy<IRTypeNode>(const IRTypeNode *t) {
    delete t;
}

/* This is necessary to get mutate() to work properly... */
struct BaseTypeNode : public IRTypeNode {
    BaseTypeNode(IRTypeEnum t)
        : IRTypeNode(t) {
    }
    virtual Type mutate_type(IRMutator *m) const = 0;
};


template<typename T>
struct TypeNode : public BaseTypeNode {
    void accept(IRVisitor *v) const override {
        return v->visit((const T*)this);
    }
    Type mutate_type(IRMutator *m) const override;
    TypeNode() : BaseTypeNode(T::_node_type) {}
    ~TypeNode() override = default;
};


struct Type : public IRHandle<IRTypeNode> {
    /** Make an undefined type */
    Type() = default;

    /** Make a type from a concrete type node pointer (e.g. Int_t) */
    Type(const IRTypeNode *n)
        : IRHandle<IRTypeNode>(n) {
    }

    /** Override get() to return a BaseExprNode * instead of an IRNode.
     *  This is necessary to get mutate() to work properly. **/
    const BaseTypeNode *get() const {
        return (const BaseTypeNode *)ptr;
    }

    uint32_t bits() const;
    uint32_t bytes() const {
        // TODO booleans better! we need to bit-pack, unlike Halide.
        return (bits() + 7) / 8;
    }
    uint32_t lanes() const;
    bool is_int() const;
    bool is_float() const;
    bool is_bool() const;
    bool is_scalar() const;
    bool is_vector() const;
    bool is_numeric() const;

    // Type casts
    // Rewrites (through vectors) to boolean base.
    Type to_bool() const;
    // returns Vector_t's etype
    Type element_of() const;

    // TODO: implement copy/move semantics!
};

template<typename T>
Type TypeNode<T>::mutate_type(IRMutator *m) const {
    return m->visit((const T*)this);
}

struct Int_t : TypeNode<Int_t> {
    uint32_t bits;

    static Type make(uint32_t bits);

    static const IRTypeEnum _node_type = IRTypeEnum::Int_t;
};

struct Float_t : TypeNode<Float_t> {
    uint32_t bits;

    static Type make(uint32_t bits);

    static const IRTypeEnum _node_type = IRTypeEnum::Float_t;
};

struct Bool_t : TypeNode<Bool_t> {
    static Type make();

    static const IRTypeEnum _node_type = IRTypeEnum::Bool_t;
};

struct Ptr_t : TypeNode<Ptr_t> {
    Type etype;

    static Type make(Type etype);

    static const IRTypeEnum _node_type = IRTypeEnum::Ptr_t;
};

struct Vector_t : TypeNode<Vector_t> {
    Type etype;
    uint32_t lanes;

    static Type make(Type etype, uint32_t lanes);

    static const IRTypeEnum _node_type = IRTypeEnum::Vector_t;
};

struct Struct_t : TypeNode<Struct_t> {
    // intentionally unordered!
    // TODO: let users control order?
    // this helps with packing, but should be a low-level concern, I think.
    // let this be user-facing.
    std::string name;
    std::map<std::string, Type> fields;

    static Type make(std::string name, std::map<std::string, Type> fields);

    static const IRTypeEnum _node_type = IRTypeEnum::Struct_t;
};

// TODO: Optional, Struct/Aggregate, List_t


} // namespace bonsai
