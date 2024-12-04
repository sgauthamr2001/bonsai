#pragma once

#include "IRFwdDecl.h"

namespace bonsai {

struct IRMutator {
    virtual Type mutate(const Type &type);
    virtual Expr mutate(const Expr &expr);
    virtual Stmt mutate(const Stmt &stmt);

// protected:
    // Types
    virtual Type visit(const Int_t *);
    virtual Type visit(const Float_t *);
    virtual Type visit(const Bool_t *);
    virtual Type visit(const Ptr_t *);
    virtual Type visit(const Vector_t *);
    virtual Type visit(const Struct_t *);
    // Exprs
    virtual Expr visit(const IntImm *);
    virtual Expr visit(const FloatImm *);
    virtual Expr visit(const Var *);
    virtual Expr visit(const BinOp *);
    virtual Expr visit(const Broadcast *);
    virtual Expr visit(const VectorReduce *);
    // Stmts
    virtual Stmt visit(const Return *);
    virtual Stmt visit(const Store *);

    virtual Stmt visit(const LetStmt *);
    virtual Stmt visit(const IfElse *);
    virtual Stmt visit(const Sequence *);
};

} // namespace bonsai
