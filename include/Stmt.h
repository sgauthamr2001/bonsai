#pragma once

#include <vector>

#include "IntrusivePtr.h"
#include "IRHandle.h"
#include "IRNode.h"
#include "IRVisitor.h"
#include "IRMutator.h"

#include "Expr.h"

namespace bonsai {

struct Stmt;

enum class IRStmtEnum {
    Return,
    Store,
    LetStmt,
    IfElse,
    Sequence,
};

using IRStmtNode = IRNode<Stmt, IRStmtEnum>;

template<>
inline RefCount &ref_count<IRStmtNode>(const IRStmtNode *t) noexcept {
    return t->ref_count;
}

template<>
inline void destroy<IRStmtNode>(const IRStmtNode *t) {
    delete t;
}

/* This is necessary to get mutate() to work properly... */
struct BaseStmtNode : public IRStmtNode {
    BaseStmtNode(IRStmtEnum t)
        : IRStmtNode(t) {
    }
    virtual Stmt mutate_stmt(IRMutator *m) const = 0;
};


template<typename T>
struct StmtNode : public BaseStmtNode {
    void accept(IRVisitor *v) const override {
        return v->visit((const T*)this);
    }
    Stmt mutate_stmt(IRMutator *m) const override;
    StmtNode() : BaseStmtNode(T::_node_type) {}
    ~StmtNode() override = default;
};


struct Stmt : public IRHandle<IRStmtNode> {
    /** Make an undefined stmt */
    Stmt() = default;

    /** Make a stmt from a concrete stmt node pointer (e.g. Return) */
    Stmt(const IRStmtNode *n)
        : IRHandle<IRStmtNode>(n) {
    }

    /** Override get() to return a BaseStmtNode * instead of an IRNode.
     *  This is necessary to get mutate() to work properly. **/
    const BaseStmtNode *get() const {
        return (const BaseStmtNode *)ptr;
    }

    // TODO: implement copy/move semantics!
};

template<typename T>
Stmt StmtNode<T>::mutate_stmt(IRMutator *m) const {
    return m->visit((const T*)this);
}

struct Return : StmtNode<Return> {
    Expr value;

    static Stmt make(Expr value);

    static const IRStmtEnum _node_type = IRStmtEnum::Return;
};

struct Store: StmtNode<Store> {
    // TODO: predicate
    std::string name;
    Expr index;
    Expr value;

    static Stmt make(std::string name, Expr index, Expr value);

    static const IRStmtEnum _node_type = IRStmtEnum::Store;
};

struct LetStmt : StmtNode<LetStmt> {
    std::string name;
    Expr value;
    Stmt body;

    static Stmt make(std::string name, Expr value, Stmt body);

    static const IRStmtEnum _node_type = IRStmtEnum::LetStmt;
};

struct IfElse : StmtNode<IfElse> {
    Expr cond;
    Stmt then_body;
    Stmt else_body;

    static Stmt make(Expr cond, Stmt then_body, Stmt else_body = Stmt());

    static const IRStmtEnum _node_type = IRStmtEnum::IfElse;
};

struct Sequence : StmtNode<Sequence> {
    std::vector<Stmt> stmts;

    static Stmt make(std::vector<Stmt> stmts);

    static const IRStmtEnum _node_type = IRStmtEnum::Sequence;
};

} // namespace bonsai
