#pragma once

#include <string>

#include "IntrusivePtr.h"
#include "IRHandle.h"
#include "IRNode.h"
#include "IRVisitor.h"
#include "IRMutator.h"

#include "Type.h"

namespace bonsai {

struct Expr;

enum class IRExprEnum {
    IntImm,
    FloatImm,
    Var,
    BinOp,
    Add,
    Mul,
    Broadcast,
    VectorReduce,
    Ramp,
    Build,
    Access,
};

using IRExprNode = IRNode<Expr, IRExprEnum>;

template<>
inline RefCount &ref_count<IRExprNode>(const IRExprNode *t) noexcept {
    return t->ref_count;
}

template<>
inline void destroy<IRExprNode>(const IRExprNode *t) {
    delete t;
}

/** This is necessary to get mutate() to work properly...
 *  They all contain their types (e.g. Int(32), Float(32))
 */
struct BaseExprNode : public IRExprNode {
    BaseExprNode(IRExprEnum t)
        : IRExprNode(t) {
    }
    virtual Expr mutate_expr(IRMutator *m) const = 0;
    Type type;
};


template<typename T>
struct ExprNode : public BaseExprNode {
    void accept(IRVisitor *v) const override {
        return v->visit((const T*)this);
    }
    Expr mutate_expr(IRMutator *m) const override;
    ExprNode() : BaseExprNode(T::_node_type) {}
    ~ExprNode() override = default;
};


struct Expr : public IRHandle<IRExprNode> {
    /** Make an undefined expr */
    Expr() = default;

    /** Make an expr from a concrete expr node pointer (e.g. Add) */
    Expr(const IRExprNode *n)
        : IRHandle<IRExprNode>(n) {
    }

    /** Override get() to return a BaseExprNode * instead of an IRNode.
     *  This is necessary to get mutate() to work properly. **/
    const BaseExprNode *get() const {
        return (const BaseExprNode *)ptr;
    }

    // TODO: implement copy/move semantics!

    Type type() const {
        return get()->type;
    }

    explicit Expr(int8_t x);
    explicit Expr(int16_t x);
    Expr(int32_t x);
    explicit Expr(int64_t x);
    // TODO: floats, uints, etc.
};

template<typename T>
Expr ExprNode<T>::mutate_expr(IRMutator *m) const {
    return m->visit((const T*)this);
}

struct IntImm : ExprNode<IntImm> {
    int64_t value;

    static const Expr make(Type t, int64_t value);

    static const IRExprEnum _node_type = IRExprEnum::IntImm;
};

struct FloatImm : ExprNode<FloatImm> {
    double value;

    static const Expr make(Type t, double value);

    static const IRExprEnum _node_type = IRExprEnum::IntImm;
};

struct Var : ExprNode<Var> {
    std::string name;

    static Expr make(Type t, const std::string &name);

    static const IRExprEnum _node_type = IRExprEnum::Var;
};

struct BinOp : ExprNode<BinOp> {
    enum OpType {
        Add,
        Div,
        Eq,
        Le,
        Lt,
        Mul,
        Sub,
        // TODO: Mod, Min, Max, Ne?
        // TODO: And, Or, Not?
    };

    OpType op;
    Expr a, b;

    static Expr make(OpType op, Expr a, Expr b);

    static const IRExprEnum _node_type = IRExprEnum::BinOp;

    static bool is_numeric_op(const OpType &op);
    static bool is_boolean_op(const OpType &op);
};

struct Broadcast : ExprNode<Broadcast> {
    uint32_t lanes;
    Expr value;

    static Expr make(uint32_t lanes, Expr value);

    static const IRExprEnum _node_type = IRExprEnum::Broadcast;
};

struct VectorReduce : ExprNode<VectorReduce> {
    enum OpType {
        Add,
        Mul,
        Min,
        Max,
        // TODO: and, or, saturating_add?
    };

    OpType op;
    Expr value;

    static Expr make(OpType op, Expr value);

    static const IRExprEnum _node_type = IRExprEnum::VectorReduce;
};

struct Ramp : ExprNode<Ramp> {
    Expr base, stride;
    int lanes;

    static Expr make(Expr base, Expr stride, int lanes);

    static const IRExprEnum _node_type = IRExprEnum::Ramp;
};

// Construct a value of a Type (e.g. Vector_t or Struct_t)
struct Build : ExprNode<Build> {
    std::vector<Expr> values;

    static Expr make(Type type, std::vector<Expr> values);

    static const IRExprEnum _node_type = IRExprEnum::Build;
};

// Access a value of a Struct_t
// TODO: implement for Vector_t?
struct Access : ExprNode<Access> {
    std::string field;
    Expr value;

    static Expr make(std::string field, Expr value);

    static const IRExprEnum _node_type = IRExprEnum::Access;
};

// TODO: need Load with more info than Halide, can load from arbitrary pointer...


// TODO: Call, Set Intrinsics, Lambdas, ??? Select, Load, (?)Let, Not, Negate
// TODO: intrinsics

} // namespace bonsai
