#pragma once

#include <string>

#include "IntrusivePtr.h"
#include "IRHandle.h"
#include "IRNode.h"
#include "IRVisitor.h"
#include "IRMutator.h"

#include "Type.h"

namespace bonsai {
namespace ir {

struct Expr;

enum class IRExprEnum {
    IntImm,
    FloatImm,
    Var,
    BinOp,
    Add,
    Mul,
    // Vector ops
    Broadcast,
    VectorReduce,
    Ramp,
    // Struct ops.
    Build,
    Access,
    // Calls
    Intrinsic,
    Lambda,
    GeomOp,
    SetOp,
    Call,
};

using IRExprNode = IRNode<Expr, IRExprEnum>;

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
        And,
        Div,
        Eq,
        Le,
        Lt,
        Mod,
        Mul,
        Neq,
        Or,
        Sub,
        Xor,
        // TODO: Min/Max?
        // TODO: UnaryOp for Neg/Not?
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

struct Intrinsic : ExprNode<Intrinsic> {
    // For now, just supporting (seemingly relevant) LLVM intrinsic ops:
    // https://llvm.org/docs/LangRef.html#standard-c-c-library-intrinsics
    enum OpType {
        abs,
        sqrt,
        sin,
        cos,
        // TODO: more
    };

    OpType op;
    Expr value;

    static Expr make(OpType op, Expr value);

    static const IRExprEnum _node_type = IRExprEnum::Intrinsic;
};

struct Lambda : ExprNode<Lambda> {
    // TODO: do we need types for the args?
    struct Argument {
        std::string name;
        Type type; // optional
    };
    std::vector<Argument> args;
    Expr value;

    static Expr make(std::vector<Argument> args, Expr value);

    static const IRExprEnum _node_type = IRExprEnum::Lambda;
};

struct GeomOp : ExprNode<GeomOp> {
    enum OpType {
        distance, // minimum (TODO: maximum?)
        intersects,
        contains,
        // TODO: the rest
    };

    OpType op;
    Expr a, b;

    static Expr make(OpType op, Expr a, Expr b);

    static const IRExprEnum _node_type = IRExprEnum::GeomOp;
};

struct SetOp : ExprNode<SetOp> {
    enum OpType {
        argmin,
        filter,
        map,
        product,
        // TODO: reduce
        // TODO: geometric intrinsics for lambda
    };

    OpType op;
    // For Argmin/Map/Filter, a: Lambda, b: Set
    // For Product, a and b are Sets
    Expr a, b;

    static Expr make(OpType op, Expr a, Expr b);

    static const IRExprEnum _node_type = IRExprEnum::SetOp;
};

struct Call : ExprNode<Call> {
    Expr func;
    std::vector<Expr> args;

    static Expr make(Expr func, std::vector<Expr> args);

    static const IRExprEnum _node_type = IRExprEnum::Call;
};


// TODO: need Load with more info than Halide, can load from arbitrary pointer...


// TODO: ??? Select, Load, (?)Let, Not, Negate

}  // namespace ir

template<>
inline RefCount &ref_count<ir::IRExprNode>(const ir::IRExprNode *t) noexcept {
    return t->ref_count;
}

template<>
inline void destroy<ir::IRExprNode>(const ir::IRExprNode *t) {
    delete t;
}

}  // namespace bonsai
