#pragma once

#include "IRFwdDecl.h"

namespace bonsai {

struct IRVisitor {
    // Types
    virtual void visit(const Int_t *);
    virtual void visit(const Float_t *);
    virtual void visit(const Bool_t *);
    virtual void visit(const Ptr_t *);
    virtual void visit(const Vector_t *);
    virtual void visit(const Struct_t *);
    // Exprs
    virtual void visit(const IntImm *);
    virtual void visit(const FloatImm *);
    virtual void visit(const Var *);
    virtual void visit(const BinOp *);
    virtual void visit(const Broadcast *);
    virtual void visit(const VectorReduce *);
    virtual void visit(const Ramp *);
    virtual void visit(const Build *);
    virtual void visit(const Access *);
    // Stmts
    virtual void visit(const Return *);
    virtual void visit(const Store *);
    virtual void visit(const LetStmt *);
    virtual void visit(const IfElse *);
    virtual void visit(const Sequence *);
};

} // namespace bonsai
